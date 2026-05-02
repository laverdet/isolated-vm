export module auto_js:builtin.accept;
import :intrinsics.array_buffer;
import :intrinsics.bigint;
import :intrinsics.date;
import :intrinsics.external;
import :reference.accept;
import std;
import util;

namespace js {

// Static value acceptor
template <class Tag, auto Value>
struct accept_as_constant {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> decltype(Value) {
			return Value;
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

// Lossless value acceptor
template <class Tag, class Type>
struct accept_without_narrowing {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Type {
			return Type{std::forward<decltype(subject)>(subject)};
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

// Generic coercing acceptor
template <template <class> class TagOf, class Canonical, class Type>
struct accept_coerced {
	private:
		// nb: For example, `number_of_tag<double>`
		using covariant_tag_type = TagOf<Type>;
		// nb: For example, `number_tag`
		using contravariant_tag_type = covariant_tag_type::tag_type;

	public:
		constexpr auto operator()(contravariant_tag_type /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Type {
			return coerce(std::type_identity<Canonical>{}, std::forward<decltype(subject)>(subject));
		}

		template <class Subject>
		constexpr auto operator()(TagOf<Subject> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Type {
			return coerce(std::type_identity<Subject>{}, std::forward<decltype(subject)>(subject));
		}

		constexpr auto operator()(covariant_tag<covariant_tag_type> tag, visit_holder visit, auto&& subject) const -> Type {
			return (*this)(*tag, visit, std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		[[nodiscard]] constexpr auto coerce(std::type_identity<Type> /*tag*/, auto&& subject) const -> Type {
			return Type{std::forward<decltype(subject)>(subject)};
		}

		template <class Subject>
		[[nodiscard]] constexpr auto coerce(std::type_identity<Subject> /*tag*/, auto&& subject) const -> Type {
			auto coerced = static_cast<Subject>(std::forward<decltype(subject)>(subject));
			auto result = static_cast<Type>(coerced);
			if (static_cast<Subject>(result) != coerced) {
				throw js::range_error{u"Could not coerce value"};
			}
			return result;
		}
};

// `undefined` & `null`
template <>
struct accept<void, std::monostate> : accept_as_constant<undefined_tag, std::monostate{}> {};

template <>
struct accept<void, std::nullptr_t> : accept_as_constant<null_tag, nullptr> {};

// `bool`
template <>
struct accept<void, bool> : accept_without_narrowing<boolean_tag, bool> {};

// `number` types
template <>
struct accept<void, double> : accept_coerced<number_tag_of, double, double> {};

template <>
struct accept<void, std::int32_t> : accept_coerced<number_tag_of, double, std::int32_t> {};

template <>
struct accept<void, std::uint32_t> : accept_coerced<number_tag_of, double, std::uint32_t> {};

// `bigint` types
template <>
struct accept<void, bigint> : accept_coerced<bigint_tag_of, bigint, bigint> {};

template <>
struct accept<void, std::int64_t> : accept_coerced<bigint_tag_of, bigint, std::int64_t> {};

template <>
struct accept<void, std::uint64_t> : accept_coerced<bigint_tag_of, bigint, std::uint64_t> {};

// `date`
template <>
struct accept<void, js_clock::time_point> : accept_without_narrowing<date_tag, js_clock::time_point> {};

// `string` types. utf8 string interpolation is not implemented, so non-latin1 characters will throw
template <class Char>
struct accept_coerced_string {
	private:
		using value_type = std::basic_string<Char>;

	public:
		constexpr auto operator()(string_tag /*tag*/, visit_holder visit, auto&& subject) const -> value_type {
			return (*this)(string_tag_of<char16_t>{}, visit, std::forward<decltype(subject)>(subject));
		}

		constexpr auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> value_type {
			return value_type{std::forward<decltype(subject)>(subject)};
		}

		constexpr auto operator()(covariant_tag<string_tag_of<Char>> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> value_type {
			return value_type{std::forward<decltype(subject)>(subject)};
		}

		template <class Subject>
		constexpr auto operator()(string_tag_of<Subject> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> value_type {
			return coerce<Subject>(std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		template <class Subject>
		constexpr auto coerce(auto&& subject) const -> value_type {
			return coerce(std::basic_string_view{std::basic_string<Subject>{std::forward<decltype(subject)>(subject)}});
		}

		template <class Subject>
		constexpr auto coerce(std::basic_string_view<Subject> subject) const -> value_type {
			return util::transcode_string<Char>(subject);
		}

		template <class Subject, auto Value>
		constexpr auto coerce(util::constant_wrapper<Value> subject) const -> value_type {
			return util::transcode_string<Char>(subject);
		}
};

template <class Char>
struct accept<void, std::basic_string<Char>> : accept_coerced_string<Char> {};

// `ArrayBuffer` & `SharedArrayBuffer` types
template <>
struct accept<void, array_buffer> : accept_without_narrowing<array_buffer_tag, array_buffer> {};

template <>
struct accept<void, shared_array_buffer> : accept_without_narrowing<shared_array_buffer_tag, shared_array_buffer> {};

template <class Meta>
struct accept<Meta, data_block_variant> : accept<Meta, data_block_variant::value_type> {
	private:
		using accept_type = accept<Meta, data_block_variant::value_type>;

	public:
		explicit constexpr accept(auto* transfer) :
				accept_type{transfer},
				array_buffer_index_{accept_reference_of<Meta, array_buffer>::extract_type_index(transfer)},
				shared_array_buffer_index_{accept_reference_of<Meta, shared_array_buffer>::extract_type_index(transfer)} {}

		using accept_type::operator();

		// reference provider
		auto operator()(std::type_identity<data_block_variant> /*type*/, accepted_reference reference) const -> data_block_variant {
			if (reference.type_index() == array_buffer_index_) {
				return reference_of<array_buffer>{reference.id()};
			} else if (reference.type_index() == shared_array_buffer_index_) {
				return reference_of<shared_array_buffer>{reference.id()};
			} else {
				std::unreachable();
			}
		}

	private:
		unsigned array_buffer_index_;
		unsigned shared_array_buffer_index_;
};

// `TypedArray` and `DataView` types
template <class Meta, class Tag, class Type>
struct accept_typed_array {
	public:
		explicit constexpr accept_typed_array(auto* transfer) : accept_{transfer} {}

		constexpr auto operator()(Tag /*tag*/, auto& visit, auto&& subject) const -> Type {
			auto byte_offset = subject.byte_offset();
			auto size = subject.size();
			return Type{visit(std::forward<decltype(subject)>(subject).buffer(), accept_), byte_offset, size};
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		accept_value<Meta, data_block_variant> accept_;
};

template <class Meta, class Type>
struct accept<Meta, typed_array<Type>> : accept_typed_array<Meta, typed_array_tag_of<Type>, typed_array<Type>> {
		using accept_typed_array<Meta, typed_array_tag_of<Type>, typed_array<Type>>::accept_typed_array;
};

template <class Meta>
struct accept<Meta, typed_array<void>> : accept_typed_array<Meta, data_view_tag, typed_array<void>> {
		using accept_typed_array<Meta, data_view_tag, typed_array<void>>::accept_typed_array;
};

// `tagged_external` acceptors
template <class Type>
struct accept<void, tagged_external<Type>> {
		constexpr auto operator()(object_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> tagged_external<Type> {
			auto* pointer = subject.try_cast(type<Type>);
			if (pointer == nullptr) {
				throw js::type_error{u"Invalid object type"};
			} else {
				return tagged_external{*pointer};
			}
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

template <class Type>
	requires requires { typename transfer_type_t<Type>; }
struct accept<void, Type&> : accept<void, transfer_type_t<Type>> {
		using accept<void, transfer_type_t<Type>>::accept;
};

template <class Type>
struct accept<void, const Type&> : accept<void, Type&> {
		using accept<void, Type&>::accept;
};

// `std::optional` allows `undefined` in addition to the next acceptor
template <class Meta, class Type>
struct accept<Meta, std::optional<Type>> : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		using accept_type::operator();
		constexpr auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> std::optional<Type> {
			return std::nullopt;
		}
};

// `std::pair` uses `first` & `second` acceptors. This corresponds to "entries" in a vector / array.
// TODO: Revisit whether or not `first` & `second` make sense or if `operator()` should be the only
// way to do this.
template <class Meta, class Key, class Value>
struct accept<Meta, std::pair<Key, Value>> {
		explicit constexpr accept(auto* transfer) :
				first{transfer},
				second{transfer} {}

		constexpr auto operator()(auto& visit, auto&& entry) const -> std::pair<Key, Value> {
			return std::pair{
				// NOLINTNEXTLINE(bugprone-use-after-move)
				visit.first(std::forward<decltype(entry)>(entry).first, first),
				// NOLINTNEXTLINE(bugprone-use-after-move)
				visit.second(std::forward<decltype(entry)>(entry).second, second),
			};
		}

		consteval static auto types(auto recursive) -> auto {
			return accept<Meta, Key>::types(recursive) + accept<Meta, Value>::types(recursive);
		}

		accept_value<Meta, Key> first;
		accept_value<Meta, Value> second;
};

// Accepting a pointer uses the reference acceptor, while also accepting `undefined`
template <class Meta, class Type>
struct accept<Meta, Type*> : accept<Meta, Type&> {
		using accept_type = accept<Meta, Type&>;
		using accept_type::accept_type;

		using accept_type::operator();

		constexpr auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> Type* {
			return nullptr;
		}

		constexpr auto operator()(Type& target) const -> Type* {
			return std::addressof(target);
		}
};

// `std::expected` is abused to provide an undefined case similar to `std::optional`, but only when
// `undefined_in_tag` is passed. This detects missing object properties, as as opposed to
// `undefined` properties.
template <class Meta, class Type>
struct accept<Meta, std::expected<Type, undefined_in_tag>> : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using value_type = std::expected<Type, undefined_in_tag>;
		using accept_type::accept_type;

		using accept_type::operator();

		constexpr auto operator()(undefined_in_tag tag, visit_holder /*visit*/, const auto& /*value*/) const -> value_type {
			return value_type{std::unexpect, tag};
		}

		constexpr auto operator()(Type&& target) const -> value_type {
			return value_type{std::in_place, std::move(target)};
		}
};

} // namespace js
