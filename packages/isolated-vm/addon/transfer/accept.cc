export module isolated_vm:transfer.accept;
import :support.free_function;
import :value;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

struct accept_vm_primitive {
	public:
		explicit accept_vm_primitive(const basic_lock& lock) : lock_{lock} {}

		// undefined
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, std::monostate /*subject*/) const -> value_of<undefined_tag> {
			return value_of<undefined_tag>::make(lock());
		}

		auto operator()(undefined_tag tag, visit_holder visit, const auto& /*subject*/) const -> value_of<undefined_tag> {
			return (*this)(tag, visit, std::monostate{});
		}

		// null
		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, std::nullptr_t /*subject*/) const -> value_of<null_tag> {
			return value_of<null_tag>::make(lock());
		}

		auto operator()(null_tag tag, visit_holder visit, const auto& /*subject*/) const -> value_of<null_tag> {
			return (*this)(tag, visit, nullptr);
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, bool subject) const -> value_of<boolean_tag> {
			return value_of<boolean_tag>::make(lock(), subject);
		}

		// number
		auto operator()(number_tag_of<double> /*tag*/, visit_holder /*visit*/, double subject) const -> value_of<number_tag_of<double>> {
			return value_of<number_tag>::make(lock(), subject);
		}

		auto operator()(number_tag_of<std::int32_t> /*tag*/, visit_holder /*visit*/, std::int32_t subject) const -> value_of<number_tag_of<std::int32_t>> {
			return value_of<number_tag>::make(lock(), subject);
		}

		auto operator()(number_tag /*tag*/, visit_holder visit, auto&& subject) const -> value_of<number_tag> {
			return (*this)(number_tag_of<double>{}, visit, double{std::forward<decltype(subject)>(subject)});
		}

		template <class Type>
		auto operator()(number_tag_of<Type> tag, visit_holder visit, auto&& subject) const -> value_of<number_tag_of<Type>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		// string
		auto operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, std::string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char>>> {
			return js::referenceable_value{value_of<string_tag>::make(lock(), subject)};
		}

		auto operator()(string_tag_of<char16_t> /*tag*/, visit_holder /*visit*/, std::u16string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char16_t>>> {
			return js::referenceable_value{value_of<string_tag>::make(lock(), subject)};
		}

		auto operator()(string_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<string_tag>> {
			auto value = (*this)(string_tag_of<char16_t>{}, visit, std::u16string_view{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{value_of<string_tag>{*value}};
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, std::convertible_to<std::basic_string_view<Char>> auto&& subject) const
			-> js::referenceable_value<value_of<string_tag_of<Char>>> {
			return (*this)(tag, visit, std::basic_string_view<Char>{std::forward<decltype(subject)>(subject)});
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<string_tag_of<Char>>> {
			return (*this)(tag, visit, std::basic_string<Char>{std::forward<decltype(subject)>(subject)});
		}

		// bigint (int64)
		auto operator()(bigint_tag_of<std::int64_t> /*tag*/, visit_holder /*visit*/, std::int64_t subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<std::int64_t>>> {
			return js::referenceable_value{value_of<bigint_tag>::make(lock(), subject)};
		}

		[[nodiscard]] auto lock() const -> const basic_lock& { return lock_; }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		std::reference_wrapper<const basic_lock> lock_;
};

struct accept_vm_prototype : accept_vm_primitive {
	public:
		using accept_vm_primitive::accept_vm_primitive;
		using accept_vm_primitive::lock;

		// cast primitive to prototype
		template <std::convertible_to<primitive_tag> Tag>
		auto operator()(Tag tag, auto& visit, auto&& subject) const -> value_of<prototype_tag> {
			const accept_vm_primitive& accept = *this;
			auto result = accept(tag, visit, std::forward<decltype(subject)>(subject));
			constexpr auto unwrap = util::overloaded{
				[](auto value) { return value; },
				[]<class Type>(js::referenceable_value<Type> value) { return *std::move(value); },
			};
			return value_of<prototype_tag>::from(unwrap(result));
		}

		// function
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> value_of<function_prototype_tag> {
			using lock_type = const runtime_lock&;
			auto [ function, length ] = make_free_function<lock_type>(std::move(subject).callback);
			auto data = make_callback_storage(std::move(function));
			return value_of<function_prototype_tag>::make(lock(), std::move(data), length);
		}
};

struct accept_vm_value : accept_vm_primitive {
	public:
		explicit accept_vm_value(const runtime_lock& lock) : accept_vm_primitive{lock} {}
		using accept_vm_primitive::operator();

		// bigint (from words)
		auto operator()(bigint_tag_of<js::bigint> /*tag*/, visit_holder /*visit*/, const js::bigint& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>> {
			return js::referenceable_value{value_of<bigint_tag>::make(lock(), subject)};
		}

		auto operator()(bigint_tag_of<js::bigint> /*tag*/, visit_holder /*visit*/, js::bigint&& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>> {
			return js::referenceable_value{value_of<bigint_tag>::make(lock(), std::move(subject))};
		}

		auto operator()(bigint_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<bigint_tag>> {
			auto value = (*this)(bigint_tag_of<js::bigint>{}, visit, js::bigint{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{value_of<bigint_tag>{*std::move(value)}};
		}

		template <class Type>
		auto operator()(bigint_tag_of<Type> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<Type>>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		// array
		auto operator()(this const auto& self, list_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<list_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<list_tag>::make(self.lock()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<list_tag> receiver, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
						auto name = visit.first(std::forward<decltype(key)>(key), self);
						auto item = visit.first(std::forward<decltype(value)>(value), self);
						receiver.set(self.lock(), name, item);
					}
				},
			};
		}

		// vectors
		auto operator()(this const auto& self, vector_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<list_tag>, decltype(self), decltype(visit), decltype(subject)> {
			auto [... size ] = util::maybe_range_size(subject);
			return {
				value_of<list_tag>::make(self.lock(), size...),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<list_tag> receiver, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					int ii = 0;
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& element : util::forward_range(std::forward<decltype(range)>(range))) {
						auto item = visit(std::forward<decltype(element)>(element), self);
						receiver.set(self.lock(), ii++, item);
					}
				},
			};
		}

		template <std::size_t Size>
		auto operator()(this const auto& self, tuple_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<list_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<list_tag>::make(self.lock(), Size),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<list_tag> receiver, auto& self, auto& visit, auto /*&&*/ tuple) -> void {
					const auto [... indices ] = util::sequence<Size>;
					(..., [ & ]() -> void {
						auto element = visit(indices, std::forward<decltype(tuple)>(tuple), self);
						unmaybe(receiver.set(self.lock(), indices, element));
					}());
				}
			};
		}

		// dictionary
		auto operator()(this const auto& self, dictionary_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<dictionary_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<record_tag>::make(self.lock()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<dictionary_tag> receiver, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					for (auto&& [ key, value ] : util::into_range(subject)) {
						auto name = visit.first(std::forward<decltype(key)>(key), self);
						auto item = visit.first(std::forward<decltype(value)>(value), self);
						receiver.set(self.lock(), name, item);
					}
				},
			};
		}

		// structs
		template <std::size_t Size>
		auto operator()(this const auto& self, struct_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<record_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<record_tag>::make(self.lock()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<record_tag> receiver, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					self.template accept_entry_pair_struct<Size>(visit, receiver, std::forward<decltype(subject)>(subject));
				}
			};
		}

		template <std::size_t Size>
		auto accept_entry_pair_struct(this const auto& self, auto& visit, value_of<record_tag> receiver, auto&& subject) -> void {
			const auto [... indices ] = util::sequence<Size>;
			(..., [ & ]() -> void {
				auto accept_entry = accept_entry_pair<decltype(self), decltype(self)>{self};
				auto entry = visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(subject)>(subject), accept_entry);
				receiver.set(self.lock(), entry.first, entry.second);
			}());
		}

		// array buffer
		auto operator()(array_buffer_tag /*tag*/, visit_holder /*visit*/, js::array_buffer subject) const
			-> js::referenceable_value<value_of<array_buffer_tag>> {
			return js::referenceable_value{value_of<array_buffer_tag>::make(lock(), std::move(subject))};
		}

		[[nodiscard]] auto lock() const -> const runtime_lock& {
			return reinterpret_cast<const runtime_lock&>(accept_vm_primitive::lock());
		}
};

// Object key lookup (struct_template, etc)
template <class Meta, auto Key, class Type>
struct accept_vm_property_value {
	public:
		explicit constexpr accept_vm_property_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		auto operator()(dictionary_tag /*tag*/, auto& visit, const auto& subject) const -> Type {
			if (auto local = first(std::type_identity<void>{}, visit.first); subject.has(local)) {
				return visit.second(subject.get(local), second);
			} else {
				return second(undefined_in_tag{}, visit.second, std::monostate{});
			}
		}

	private:
		mutable visit_key_literal<Key, value_of<record_tag>> first;
		accept_value<Meta, Type> second;
};

} // namespace isolated_vm

namespace js {
using namespace isolated_vm;

template <class Tag>
struct accept_property_subject<value_of<Tag>> : std::type_identity<value_of<record_tag>> {};

// Primitive acceptor
template <std::convertible_to<value_tag> Tag>
	requires std::is_convertible_v<Tag, primitive_tag>
struct accept<void, value_of<Tag>> : accept_vm_primitive {
		using accept_vm_primitive::accept_vm_primitive;
};

// Template / prototype acceptor
template <class Tag>
	requires std::is_convertible_v<Tag, prototype_tag>
struct accept<void, value_of<Tag>> : accept_vm_prototype {
		using accept_vm_prototype::accept_vm_prototype;
};

// Value acceptor
template <std::convertible_to<value_tag> Tag>
struct accept<void, value_of<Tag>> : accept_vm_value {
		using accept_vm_value::accept_vm_value;
};

// Object key lookup (struct_template, etc)
template <class Meta, auto Key, class Type>
struct accept_property_value<Meta, Key, Type, value_of<record_tag>> : accept_vm_property_value<Meta, Key, Type> {
		using accept_vm_property_value<Meta, Key, Type>::accept_vm_property_value;
};

} // namespace js
