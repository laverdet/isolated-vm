export module isolated_vm:handle.types;
import auto_js;

namespace isolated_vm {
using namespace js;

// `value_handle` is the base class of `value_of<T>`, and `bound_value<T>`.
class value_handle {
	protected:
		explicit value_handle(void* value) : value_{value} {}

	public:
		value_handle() = default;

		// Check empty value
		explicit operator bool() const { return value_ != nullptr; }

	private:
		void* value_{};
};

// `value_of` forward declarations
export template <class Tag = value_tag>
class value_of;

template <class Tag>
class value_next;

// `bound_value` forward declarations
export template <class Tag>
class bound_value;

template <class Tag>
class bound_value_next;

// Map of `value_of<Tag>` / `bound_value<Tag>` to concrete implementation classes.
template <class Tag>
struct value_defaults {
		using bound_type = bound_value_next<Tag>;
		using value_type = value_next<Tag>;
};

template <class Tag>
struct value_specialization : value_defaults<Tag> {};

// Typed specializations
template <>
struct value_specialization<boolean_tag> : value_defaults<boolean_tag> {
		using bound_type = class bound_value_for_boolean;
};

template <>
struct value_specialization<number_tag> : value_defaults<number_tag> {
		using bound_type = class bound_value_for_number;
};

template <>
struct value_specialization<string_tag> : value_defaults<string_tag> {
		using bound_type = class bound_value_for_string;
};

} // namespace isolated_vm
