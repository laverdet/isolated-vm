module;
#include "auto_js/export_tag.h"
export module isolated_vm:value;
export import :handle.bound_value;
export import :handle.value_of;
export import :support.environment_fwd;
export import :value.primitive;

namespace isolated_vm {

export enum class value_typeof : std::uint8_t {
	// Reminder: Don't sort these
	undefined,
	null,
	boolean,
	number,
	number_i32,
	string,
	string_latin1,
	symbol,
	bigint,
	bigint_i64,
};

EXPORT auto inspect_type(const environment& env, value_of<value_tag> value) -> value_typeof;

// primitives
EXPORT auto inspect_type(const environment& env, value_of<primitive_tag> value) -> value_typeof;
EXPORT auto inspect_type(const environment& env, value_of<number_tag> value) -> value_typeof;
EXPORT auto inspect_type(const environment& env, value_of<name_tag> value) -> value_typeof;
EXPORT auto inspect_type(const environment& env, value_of<string_tag> value) -> value_typeof;
EXPORT auto inspect_type(const environment& env, value_of<bigint_tag> value) -> value_typeof;

} // namespace isolated_vm
