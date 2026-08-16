module;
#include "auto_js/export_tag.h"
export module isolated_vm:handle.value_of;
import :handle.types;
import :support.lock_fwd;
import auto_js;
import std;

namespace isolated_vm {
using namespace js;

// Map of `value_of<Tag>` to concrete implementation classes.
template <class Tag>
struct value_specialization;

// Member & method implementation for stateful objects. Used internally in visitors.
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
		value_of(const runtime_lock& lock, runtime_handle value) :
				runtime_handle{value},
				lock_{&lock} {}

		[[nodiscard]] auto lock() const -> const runtime_lock& { return *lock_; }

	private:
		const runtime_lock* lock_{};
};

// Deduction guide
template <class Tag>
value_of(auto, local_of<Tag>) -> value_of<Tag>;

// Details applied to each level of the `value_of<T>` hierarchy.
template <class Tag>
class value_next : public value_of<typename Tag::tag_type> {
	public:
		using tag_type = Tag;

		value_next() = default;
		value_next(auto&& lock, local_of<Tag> value, auto&&... rest) :
				value_of<typename Tag::tag_type>{std::forward<decltype(lock)>(lock), value, std::forward<decltype(rest)>(rest)...} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator local_of<Tag>() const { return local_of<Tag>::from(runtime_handle{*this}); }
};

// `value_of<Tag>` specializations.
template <class Type> class EXPORT value_for_typed_array_of;

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
struct value_specialization<number_tag>
		: std::type_identity<class value_for_number> {};

template <>
struct value_specialization<string_tag>
		: std::type_identity<class value_for_string> {};

template <>
struct value_specialization<bigint_tag>
		: std::type_identity<class value_for_bigint> {};

template <>
struct value_specialization<record_tag>
		: std::type_identity<class value_for_record> {};

template <>
struct value_specialization<vector_tag>
		: std::type_identity<class value_for_vector> {};

template <>
struct value_specialization<data_block_tag>
		: std::type_identity<class value_for_data_block> {};

template <class Type>
struct value_specialization<typed_array_tag_of<Type>>
		: std::type_identity<value_for_typed_array_of<Type>> {};

template <>
struct value_specialization<data_view_tag>
		: std::type_identity<class value_for_data_view> {};

} // namespace isolated_vm
