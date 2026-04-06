export module auto_js:reference.accept;
import :referential_value;
import :transfer;
import std;
import util;

namespace js {

// This is the reference type that is returned by the `reference_of<T>` acceptor, and is stored by
// visitors. The type index of the reference is stored along with the id (which is not globally
// unique amongst each `reference_of<T>` template instantiation). The `referential_value` acceptor
// knows how to reaccept it.
struct accepted_reference {
	protected:
		template <class Type>
		constexpr explicit accepted_reference(unsigned type_index, reference_of<Type> reference) noexcept :
				type_{type_index},
				id_{reference.id()} {}

	public:
		[[nodiscard]] constexpr auto id() const -> unsigned { return id_; }
		[[nodiscard]] constexpr auto type_index() const -> unsigned { return type_; }

	private:
		unsigned type_ : 8;
		unsigned id_ : std::numeric_limits<unsigned>::digits - 8; // (it's 24)
};

template <class Type>
struct accepted_reference_of : accepted_reference {
		// nb: This is not an exact forwarding constructor. `Type` is restricted.
		constexpr explicit accepted_reference_of(unsigned type_index, reference_of<Type> reference) noexcept :
				accepted_reference{type_index, reference} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr operator reference_of<Type>() const {
			return reference_of<Type>{id()};
		}
};

// Instantiated by `referential_value_of` acceptor. This stashes the underlying value in storage
// which is managed by this class. It also serves as the `reference_of<T>` acceptor delegate.
template <class Meta, class Type>
struct accept_reference_of : accept<Meta, Type> {
	private:
		using accept_type = accept<Meta, Type>;
		using reference_type = reference_of<Type>;
		using storage_type = reference_storage_of<Type>;

	public:
		using accept_target_type = reference_of<Type>;

		constexpr accept_reference_of(auto* transfer, unsigned type_index) :
				accept_type{transfer},
				type_index_{type_index} {}

		constexpr auto operator()(auto tag, auto& visit, auto&& subject) const
			-> js::deferred_receiver<accepted_reference_of<Type>, decltype(*this), decltype(tag), decltype(visit), decltype(subject)>
			requires std::invocable<const accept_type&, decltype(tag), decltype(visit), decltype(subject)> {
			return {
				accepted_reference_of{type_index_, storage_.allocate()},
				std::forward_as_tuple(*this, tag, visit, std::forward<decltype(subject)>(subject)),
				[](accepted_reference_of<Type> reference, auto& self, auto tag, auto& visit, auto /*&&*/ subject) -> void {
					self.storage_.at(reference_type{reference.id()}) =
						util::invoke_as<accept_type>(self, tag, visit, std::forward<decltype(subject)>(subject));
				},
			};
		}

		constexpr auto operator()(accepted_reference_of<Type> reference) const -> reference_type {
			return reference_type{reference};
		}

		constexpr auto take_reference_storage() const -> storage_type {
			return std::exchange(storage_, {});
		}

		constexpr static auto extract_type_index(accept_reference_of<Meta, Type>* transfer) -> unsigned {
			return transfer->type_index_;
		}

	private:
		mutable storage_type storage_;
		unsigned type_index_;
};

// `reference_of` acceptor delegates to `accept_reference_of`.
template <class Meta, class Type>
struct accept<Meta, reference_of<Type>> : accept_delegated<accept_reference_of<Meta, Type>> {
		using accept_type = accept_delegated<accept_reference_of<Meta, Type>>;
		using accept_type::accept_type;

		consteval static auto types(auto recursive) -> auto {
			return accept<Meta, Type>::types(recursive);
		}
};

// `referential_value_of` acceptor delegates to its underlying value acceptor. It also instantiates
// `accept_reference_of` instances for each reference type.
template <class Meta, class Value, class Refs>
struct accept_referential_value_of;

template <class Meta, class Value>
using accept_referential_value_of_from = accept_referential_value_of<Meta, Value, typename Value::reference_types>;

template <class Meta, template <class> class Make, auto Extract>
struct accept<Meta, referential_value_of<Make, Extract>>
		: accept_delegated<accept_referential_value_of_from<Meta, referential_value_of<Make, Extract>>> {
	private:
		using delegated_type = accept_referential_value_of_from<Meta, referential_value_of<Make, Extract>>;
		using accept_type = accept_delegated<delegated_type>;

	public:
		// nb: This marks this acceptor as referential!
		using accept_reference_type = accepted_reference;

		using accept_type::accept_type;

		consteval static auto types(auto recursive) -> auto {
			return util::type_pack{type<delegated_type>} + delegated_type::types(recursive);
		}

	protected:
		constexpr auto take_reference_storage() const {
			return this->get().take_reference_storage();
		}
};

template <class Meta, class Value, class... Refs>
struct accept_referential_value_of<Meta, Value, util::type_pack<Refs...>>
		: accept<Meta, typename Value::value_type>,
			accept_reference_of<Meta, Refs>... {
	private:
		using value_type = Value;
		using value_of_type = value_type::value_type;
		using storage_type = reference_storage<Refs...>;
		using accept_type = accept<Meta, value_of_type>;

		template <std::size_t... Indices>
		constexpr explicit accept_referential_value_of(auto* transfer, std::index_sequence<Indices...> /*indices*/) :
				accept_type{transfer},
				accept_reference_of<Meta, Refs>{transfer, Indices}... {}

	public:
		explicit constexpr accept_referential_value_of(auto* transfer) :
				accept_referential_value_of{transfer, std::make_index_sequence<sizeof...(Refs)>{}} {}

		// forward underlying acceptor
		using accept_type::operator();

		// wrap into `value_type`
		constexpr auto operator()(value_of_type&& value) const -> value_type {
			return value_type{std::in_place, std::move(value)};
		}

		// delegated reference unwrap
		template <class Type>
		constexpr auto operator()(accepted_reference_of<Type> reference) const -> reference_of<Type> {
			return util::invoke_as<accept_reference_of<Meta, Type>>(*this, reference);
		}

		// reference provider
		constexpr auto operator()(std::type_identity<value_type> /*type*/, accepted_reference reference) const -> value_type {
			const auto reaccept = [ = ]<std::size_t Index>(std::integral_constant<std::size_t, Index> /*index*/) -> value_type {
				using mapped_reference_type = reference_of<Refs...[ Index ]>;
				return value_type{std::in_place, mapped_reference_type{reference.id()}};
			};
			return util::template_switch(
				reference.type_index(),
				util::sequence<sizeof...(Refs)>,
				util::overloaded{
					reaccept,
					[]() -> value_type { std::unreachable(); },
				}
			);
		}

		// Infinite recursion check. The failure case is actually just `'types<...>' with deduced return
		// type cannot be used before it is defined'`
		template <class... Seen>
		consteval static auto types(util::type_pack<Seen...> recursive)
			requires((type<Seen> != type<value_type>) && ...) {
			return accept_type::types(recursive + util::type_pack{type<value_type>});
		}

		consteval static auto types(auto /*recursive*/) {
			return util::type_pack{};
		}

		constexpr auto take_reference_storage() const -> storage_type {
			return storage_type{accept_reference_of<Meta, Refs>::take_reference_storage()...};
		}
};

// `referential_value` top acceptor. It delegates through `accept_value` in order to forward the
// underlying reference storage to the final value.
template <class Meta, template <class> class Make, auto Extract>
struct accept<Meta, referential_value<Make, Extract>>
		: accept_value<Meta, typename referential_value<Make, Extract>::value_type> {
		using value_type = referential_value<Make, Extract>;
		using accept_type = accept_value<Meta, typename referential_value<Make, Extract>::value_type>;
		using accept_type::accept_type;

		// reference provider
		// nb: Unreachable. This only comes up in the case where `lookup_or_visit` finds a reference on the
		// first visit.
		constexpr auto operator()(std::type_identity<value_type> /*type*/, accepted_reference /*reference*/) const -> value_type {
			std::terminate();
		}

		// accept as value, moving reference storage along with the value
		constexpr auto operator()(auto_tag auto tag, auto& visit, auto&& subject) const -> value_type {
			return value_type{
				util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(subject)>(subject)),
				this->take_reference_storage(),
			};
		}
};

} // namespace js
