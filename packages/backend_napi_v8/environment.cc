module backend_napi_v8;
import :environment;
import :native_module;
using namespace js;

namespace backend_napi_v8 {

struct inherited_classes {
		js::forward<napi::value_of<function_tag>> agent;
		js::forward<napi::value_of<function_tag>> module_;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"Agent">, &inherited_classes::agent},
			js::struct_member{util::cw<"Module">, &inherited_classes::module_},
		};
};

environment::~environment() {
	native_module_handle::unload_hook();
}

auto environment::make_initialize() -> napi::value_of<function_tag> {
	auto initialize = [](environment& env, inherited_classes inherited) -> void {
		env.agent_class_ = napi::reference{env, *inherited.agent};
		env.module_class_ = napi::reference{env, *inherited.module_};
	};
	return napi::value_of<function_tag>::make(*this, js::free_function{initialize});
}

} // namespace backend_napi_v8
