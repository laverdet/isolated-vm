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

// Reference acceptor for `accepted_reference`
template <class Refs>
struct reaccept_accepted_reference;

template <class... Types>
struct reaccept_accepted_reference<util::type_pack<Types...>> {
	public:
		using reference_type = accepted_reference;

		template <class To>
		constexpr auto operator()(std::type_identity<To> /*type*/, accepted_reference reference) const -> To {
			const auto reaccept = [ = ]<std::size_t Index>(std::integral_constant<std::size_t, Index> /*index*/) -> To {
				using mapped_reference_type = reference_of<Types...[ Index ]>;
				if constexpr (requires(mapped_reference_type ref) { To{ref}; }) {
					return To{mapped_reference_type{reference.id()}};
				} else {
					// This would happen if a runtime visitor (napi, v8) cached a value for a
					// `reference_of<T>` which was then visited again later with an acceptor whose target
					// can't construct from the `reference_of<T>`.
					throw js::type_error{u"Internal acceptor error"};
				}
			};
			return util::template_switch(
				reference.type_index(),
				util::sequence<sizeof...(Types)>,
				util::overloaded{
					reaccept,
					[]() -> To { std::unreachable(); },
				}
			);
		}
};

// Instantiated by `recursive_value_holder` acceptor. This stashes the underlying value in storage
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
};

// `recursive_value_holder` acceptor delegates to its underlying value acceptor. It also instantiates
// `accept_reference_of` instances for each reference type.
template <class Meta, class Value, class Refs>
struct accept_recursive_value_holder;

template <class Meta, class Value>
using accept_recursive_value_holder_from = accept_recursive_value_holder<Meta, Value, typename Value::reference_types>;

template <class Meta, template <class> class Make, auto Extract>
struct accept<Meta, recursive_value_holder<Make, Extract>>
		: accept_delegated<accept_recursive_value_holder_from<Meta, recursive_value_holder<Make, Extract>>> {
	private:
		using delegated_type = accept_recursive_value_holder_from<Meta, recursive_value_holder<Make, Extract>>;
		using accept_type = accept_delegated<delegated_type>;

	public:
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
struct accept_recursive_value_holder<Meta, Value, util::type_pack<Refs...>>
		: accept<Meta, typename Value::value_type>,
			accept_reference_of<Meta, Refs>... {
	private:
		using value_type = Value;
		using value_of_type = value_type::value_type;
		using storage_type = reference_storage<Refs...>;
		using accept_type = accept<Meta, value_of_type>;

		template <std::size_t... Indices>
		constexpr explicit accept_recursive_value_holder(auto* transfer, std::index_sequence<Indices...> /*indices*/) :
				accept_type{transfer},
				accept_reference_of<Meta, Refs>{transfer, Indices}... {}

	public:
		explicit constexpr accept_recursive_value_holder(auto* transfer) :
				accept_recursive_value_holder{transfer, std::make_index_sequence<sizeof...(Refs)>{}} {}

		// forward underlying acceptor
		using accept_type::operator();

		// delegated reference unwrap
		template <class Type>
		constexpr auto operator()(accepted_reference_of<Type> reference) const -> reference_of<Type> {
			return util::invoke_as<accept_reference_of<Meta, Type>>(*this, reference);
		}

		constexpr auto take_reference_storage() const -> storage_type {
			return storage_type{accept_reference_of<Meta, Refs>::take_reference_storage()...};
		}

		// Infinite recursion check. The failure case is actually just `'types<...>' with deduced return
		// type cannot be used before it is defined'`
		template <class... Seen>
		consteval static auto types(util::type_pack<Seen...> recursive) {
			if constexpr ((... || (type<Seen> == type<value_type>))) {
				return util::type_pack{};
			} else {
				return accept_type::types(recursive + util::type_pack{type<value_type>});
			}
		}
};

// `referential_value` top acceptor. It delegates through `accept_value` in order to forward the
// underlying reference storage to the final value.
template <class Meta, class Value, class Holder>
struct accept<Meta, referential_value<Value, Holder>>
		: accept_value<Meta, typename referential_value<Value, Holder>::value_type> {
		using value_type = referential_value<Value, Holder>;
		using accept_type = accept_value<Meta, typename referential_value<Value, Holder>::value_type>;
		using accept_type::accept_type;

		// Declare reference provider
		using accept_reference_type = reaccept_accepted_reference<typename Holder::reference_types>;

		// accept as value, moving reference storage along with the value
		constexpr auto operator()(auto tag, auto& visit, auto&& subject) const -> value_type {
			return value_type{
				util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(subject)>(subject)),
				this->take_reference_storage(),
			};
		}
};

} // namespace js
