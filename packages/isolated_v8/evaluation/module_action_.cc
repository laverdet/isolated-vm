module;
#include <functional>
export module isolated_v8:evaluation_module_action;
import ivm.utility;
import v8;

namespace isolated_v8 {

export using synthetic_module_action_type =
	util::maybe_move_only_function<void(v8::Local<v8::Context>, v8::Local<v8::Module>)>;

} // namespace isolated_v8
