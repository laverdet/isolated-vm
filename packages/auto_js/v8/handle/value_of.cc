export module v8_js:handle.value;
import :value.tag;
import auto_js;
import std;
import v8;

namespace js::iv8 {

template <class Tag>
struct value_specialization;

template <class Tag>
class value_of : public value_specialization<Tag>::type {
	public:
		using value_specialization<Tag>::type::type;
};

template <class Type>
value_of(auto, v8::Local<Type>, auto...) -> value_of<v8_to_tag<Type>>;

template <class Tag>
struct value_specialization : value_specialization<typename Tag::tag_type> {};

template <>
struct value_specialization<boolean_tag> : std::type_identity<class value_for_boolean> {};

template <>
struct value_specialization<number_tag> : std::type_identity<class value_for_number> {};

template <>
struct value_specialization<string_tag> : std::type_identity<class value_for_string> {};

template <>
struct value_specialization<symbol_tag> : std::type_identity<class value_for_symbol> {};

template <>
struct value_specialization<bigint_tag> : std::type_identity<class value_for_bigint> {};

template <>
struct value_specialization<bigint_tag_of<std::int64_t>> : std::type_identity<class value_for_bigint_i64> {};

template <>
struct value_specialization<date_tag> : std::type_identity<class value_for_date> {};

template <>
struct value_specialization<external_tag> : std::type_identity<class value_for_external> {};

template <>
struct value_specialization<object_tag> : std::type_identity<class value_for_object> {};

template <>
struct value_specialization<array_buffer_tag> : std::type_identity<class value_for_array_buffer> {};

template <>
struct value_specialization<shared_array_buffer_tag> : std::type_identity<class value_for_shared_array_buffer> {};

template <class Type>
class value_for_array_buffer_view;

template <class Tag>
struct value_specialization<typed_array_tag_of<Tag>>
		: std::type_identity<value_for_array_buffer_view<tag_to_v8<typed_array_tag_of<Tag>>>> {};

template <>
struct value_specialization<data_view_tag>
		: std::type_identity<value_for_array_buffer_view<v8::DataView>> {};

} // namespace js::iv8
