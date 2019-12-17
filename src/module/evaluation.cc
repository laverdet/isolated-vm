#include "../isolate/generic/read_option.h"
#include "./evaluation.h"

using namespace v8;
namespace ivm {

ScriptOriginHolder::ScriptOriginHolder(MaybeLocal<Object> maybe_options, bool is_module) :
		is_module{is_module} {
	Local<Object> options;
	if (maybe_options.ToLocal(&options)) {
		filename = ReadOption<std::string>(options, "filename", filename);
		column_offset = ReadOption<int32_t>(options, "columnOffset", column_offset);
		line_offset = ReadOption<int32_t>(options, "lineOffset", line_offset);
	}
}

ScriptOriginHolder::operator ScriptOrigin() const {
	return {
		HandleCast<Local<String>>(filename), // resource_name,
		HandleCast<Local<Integer>>(line_offset), // resource_line_offset
		HandleCast<Local<Integer>>(column_offset), // resource_column_offset
		{}, // resource_is_shared_cross_origin
		{}, // script_id
		{}, // source_map_url
		{}, // resource_is_opaque
		{}, // is_wasm
		HandleCast<Local<Boolean>>(is_module)
	};
}

} // namespace ivm
