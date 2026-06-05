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
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, const auto& /*subject*/) const -> decltype(Value) {
			return Value;
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
		constexpr static auto tag_types = std::tuple{Tag{}};
};

// Lossless value acceptor
template <class Tag, class Type>
struct accept_without_narrowing {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Type {
			return Type{std::forward<decltype(subject)>(subject)};
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
		constexpr static auto tag_types = std::tuple{Tag{}};
};

// Generic casting acceptor. It throws if the value cannot be safely cast.
template <template <class> class TagOf, class Canonical, class Type>
struct accept_with_cast {
	private:
		// nb: For example, `number_of_tag<double>`
		using covariant_tag_type = TagOf<Type>;
		// nb: For example, `number_tag`
		using contravariant_tag_type = covariant_tag_type::tag_type;

	public:
		constexpr auto operator()(this const auto& self, contravariant_tag_type /*tag*/, visit_holder /*visit*/, auto&& subject) -> Type {
			return self.cast(std::type_identity<Canonical>{}, std::forward<decltype(subject)>(subject));
		}

		template <class Subject>
		constexpr auto operator()(this const auto& self, TagOf<Subject> /*tag*/, visit_holder /*visit*/, auto&& subject) -> Type {
			return self.cast(std::type_identity<Subject>{}, std::forward<decltype(subject)>(subject));
		}

		constexpr auto operator()(this const auto& self, covariant_tag<covariant_tag_type> tag, visit_holder visit, auto&& subject) -> Type {
			return self(*tag, visit, std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
		constexpr static auto tag_types = std::tuple{contravariant_tag_type{}};

	private:
		[[nodiscard]] constexpr auto cast(std::type_identity<Type> /*tag*/, auto&& subject) const -> Type {
			return Type{std::forward<decltype(subject)>(subject)};
		}

		template <class Accept, class Subject>
		[[nodiscard]] constexpr auto cast(this const Accept& /*self*/, std::type_identity<Subject> /*tag*/, auto&& subject) -> Type {
			auto value = Subject{std::forward<decltype(subject)>(subject)};
			if constexpr (requires { Type{value}; }) {
				return Type{value};
			} else if constexpr (util::safe_numeric_conversion_v<Subject, Type>) {
				return static_cast<Type>(value);
			} else if constexpr (js::accept_allows_throw<Accept>) {
				auto coerced = static_cast<Type>(value);
				if (static_cast<Subject>(coerced) != value) {
					throw js::range_error{u"Could not cast value"};
				}
				return coerced;
			} else {
				static_assert(false, "cannot guarantee runtime cast");
			}
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
struct accept<void, double> : accept_with_cast<number_tag_of, double, double> {};

template <>
struct accept<void, std::int8_t> : accept_with_cast<number_tag_of, double, std::int8_t> {};

template <>
struct accept<void, std::int16_t> : accept_with_cast<number_tag_of, double, std::int16_t> {};

template <>
struct accept<void, std::int32_t> : accept_with_cast<number_tag_of, double, std::int32_t> {};

template <>
struct accept<void, std::uint8_t> : accept_with_cast<number_tag_of, double, std::uint8_t> {};

template <>
struct accept<void, std::uint16_t> : accept_with_cast<number_tag_of, double, std::uint16_t> {};

template <>
struct accept<void, std::uint32_t> : accept_with_cast<number_tag_of, double, std::uint32_t> {};

// `bigint` types
template <>
struct accept<void, bigint> : accept_with_cast<bigint_tag_of, bigint, bigint> {};

template <>
struct accept<void, std::int64_t> : accept_with_cast<bigint_tag_of, bigint, std::int64_t> {};

template <>
struct accept<void, std::uint64_t> : accept_with_cast<bigint_tag_of, bigint, std::uint64_t> {};

// `date`
template <>
struct accept<void, js_clock::time_point> : accept_without_narrowing<date_tag, js_clock::time_point> {};

// `string` types
template <class Char>
struct accept_with_casted_string {
	private:
		using value_type = std::basic_string<Char>;

	public:
		template <class Accept>
		constexpr auto operator()(this const Accept& self, string_tag /*tag*/, visit_holder visit, auto&& subject) -> value_type {
			return self(string_tag_of<char16_t>{}, visit, std::forward<decltype(subject)>(subject));
		}

		constexpr auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> value_type {
			return value_type{std::forward<decltype(subject)>(subject)};
		}

		constexpr auto operator()(covariant_tag<string_tag_of<Char>> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> value_type {
			return value_type{std::forward<decltype(subject)>(subject)};
		}

		template <class Accept, class Subject>
		constexpr auto operator()(this const Accept& self, string_tag_of<Subject> /*tag*/, visit_holder /*visit*/, auto&& subject) -> value_type {
			return self.cast(std::type_identity<Subject>{}, std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
		constexpr static auto tag_types = std::tuple{string_tag{}};

	private:
		template <class Accept, class Subject>
		constexpr auto cast(this const Accept& self, std::type_identity<Subject> tag, auto&& subject) -> value_type {
			return self.cast(tag, std::basic_string_view{std::basic_string<Subject>{std::forward<decltype(subject)>(subject)}});
		}

		template <class Accept, class Subject>
		constexpr auto cast(this const Accept& /*self*/, std::type_identity<Subject> /*tag*/, std::basic_string_view<Subject> subject) -> value_type {
			static_assert(js::accept_allows_throw<Accept> || type<Char> != type<char> || type<Subject> == type<char>, "cannot guarantee runtime cast");
			return util::transcode_string<Char>(subject);
		}

		template <class Subject, auto Value>
		constexpr auto cast(this auto& /*self*/, std::type_identity<Subject> /*tag*/, const util::constant_wrapper<Value>& subject) -> value_type {
			using transcoded_type = decltype(util::transcode_string<Char>(subject));
			return transcoded_type::value;
		}
};

template <class Char>
struct accept<void, std::basic_string<Char>> : accept_with_casted_string<Char> {};

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
		constexpr auto operator()(std::type_identity<data_block_variant> /*type*/, accepted_reference reference) const -> data_block_variant {
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

struct accept_data_block_span {
		using accept_target_type = std::span<std::byte>;

		constexpr auto operator()(data_block_tag /*tag*/, visit_holder /*visit*/, const auto& subject) const -> accept_target_type {
			return std::span{subject.data(), subject.byte_length()};
		}
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
		constexpr static auto tag_types = std::tuple{Tag{}};

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

template <class Meta, class Type>
struct accept_typed_array_span : private accept_data_block_span {
		explicit constexpr accept_typed_array_span(auto* /*transfer*/) {}

		constexpr auto operator()(typed_array_tag_of<Type> /*tag*/, auto& visit, const auto& subject) const -> std::span<Type> {
			const accept_data_block_span& accept_block = *this;
			auto byte_span = visit(subject.buffer(), accept_block);
			auto typed_span = std::span{reinterpret_cast<Type*>(byte_span.data() + subject.byte_offset()), byte_span.size() / sizeof(Type)};
			return typed_span.subspan(0, subject.size());
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
		constexpr static auto tag_types = std::tuple{typed_array_tag_of<std::remove_const_t<Type>>{}};
};

template <class Meta, class Type>
struct accept<Meta, std::span<Type>> : accept_typed_array_span<Meta, std::remove_const_t<Type>> {
		using accept_typed_array_span<Meta, std::remove_const_t<Type>>::accept_typed_array_span;
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
		constexpr auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*subject*/) const -> std::optional<Type> {
			return std::nullopt;
		}

		constexpr static auto tag_types = std::tuple_cat(accept_tags_of_v<accept_type>, std::tuple<undefined_tag>{});
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

		constexpr auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*subject*/) const -> Type* {
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

		constexpr auto operator()(undefined_in_tag tag, visit_holder /*visit*/, const auto& /*subject*/) const -> value_type {
			return value_type{std::unexpect, tag};
		}

		constexpr auto operator()(Type&& target) const -> value_type {
			return value_type{std::in_place, std::move(target)};
		}
};

} // namespace js
