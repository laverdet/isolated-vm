export module napi_js:handle.value_of;
import :handle.types;
import nodejs;
import std;

namespace js::napi {

// Map of `value_of<Tag>` to concrete implementation classes.
template <class Tag>
struct value_specialization;

// Heirarchy (for example):
// value_of<record_tag>
// value_for_record ->
// value_next<record_tag> ->
// value_for_object ->
// value_next<object_tag> ->
// value_next<value_tag> ->
// value_next<datum_tag> ->
// value_of<void>

// Details applied to each level of the `value_of<T>` hierarchy.
template <class Tag>
class value_next : public value_of<typename Tag::tag_type> {
	public:
		using tag_type = Tag;
		value_next() = default;
		value_next(auto&& env, local_of<Tag> value, auto&&... rest) :
				value_of<typename Tag::tag_type>{std::forward<decltype(env)>(env), value, std::forward<decltype(rest)>(rest)...} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator local_of<Tag>() const { return local_of<Tag>::from(napi_value{*this}); }
};

// Member & method implementation for value semantics objects. It holds the type-erased environment
// and is used for common operations like casting & iteration.
template <class Tag>
class value_of : public value_specialization<Tag>::type {
	public:
		using value_type = value_specialization<Tag>::type;
		using value_type::value_type;
};

// Sentinel instantiation
template <>
class value_of<void> : public runtime_handle {
	protected:
		value_of() = default;
		value_of(napi_env env, napi_value value) :
				runtime_handle{value},
				env_{env} {}

		[[nodiscard]] auto env() const -> napi_env { return env_; }

	private:
		napi_env env_{};
};

// Deduction guide
template <class Tag>
value_of(auto, local_of<Tag>) -> value_of<Tag>;

// `value_of<Tag>` specializations. The default specialization selects `value_next` for the
// immediate parent tag, instantiating it for each tag unconditionally.
template <class Type> class value_for_typed_array_of;

template <class Tag>
struct value_specialization
		: std::type_identity<value_next<Tag>> {};

template <>
struct value_specialization<void>
		: std::type_identity<value_of<void>> {};

template <>
struct value_specialization<boolean_tag>
		: std::type_identity<class value_for_boolean> {};

template <>
struct value_specialization<false_tag>
		: std::type_identity<class value_for_false> {};

template <>
struct value_specialization<true_tag>
		: std::type_identity<class value_for_true> {};

template <>
struct value_specialization<number_tag>
		: std::type_identity<class value_for_number> {};

template <>
struct value_specialization<bigint_tag>
		: std::type_identity<class value_for_bigint> {};

template <>
struct value_specialization<string_tag>
		: std::type_identity<class value_for_string> {};

template <>
struct value_specialization<object_tag>
		: std::type_identity<class value_for_object> {};

template <>
struct value_specialization<date_tag>
		: std::type_identity<class value_for_date> {};

template <>
struct value_specialization<record_tag>
		: std::type_identity<class value_for_record> {};

template <>
struct value_specialization<list_tag>
		: std::type_identity<class value_for_list> {};

template <>
struct value_specialization<data_block_tag>
		: std::type_identity<class value_for_data_block> {};

template <>
struct value_specialization<array_buffer_tag>
		: std::type_identity<class value_for_array_buffer> {};

template <>
struct value_specialization<shared_array_buffer_tag>
		: std::type_identity<class value_for_shared_array_buffer> {};

template <>
struct value_specialization<array_buffer_view_tag>
		: std::type_identity<class value_for_array_buffer_view> {};

template <>
struct value_specialization<typed_array_tag>
		: std::type_identity<class value_for_typed_array> {};

template <class Type>
struct value_specialization<typed_array_tag_of<Type>>
		: std::type_identity<value_for_typed_array_of<Type>> {};

template <>
struct value_specialization<data_view_tag>
		: std::type_identity<class value_for_data_view> {};

template <>
struct value_specialization<external_tag>
		: std::type_identity<class value_for_external> {};

template <>
struct value_specialization<vector_tag>
		: std::type_identity<class value_for_vector> {};

} // namespace js::napi
