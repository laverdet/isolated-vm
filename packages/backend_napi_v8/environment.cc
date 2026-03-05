module backend_napi_v8;
import :environment;
using namespace js;

namespace backend_napi_v8 {

struct inherited_classes {
		js::forward<napi::value<function_tag>> agent;
		js::forward<napi::value<function_tag>> module_;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"Agent">, &inherited_classes::agent},
			js::struct_member{util::cw<"Module">, &inherited_classes::module_},
		};
};

auto environment::make_initialize() -> napi::value<function_tag> {
	auto initialize = [](environment& env, inherited_classes inherited) -> void {
		env.agent_class_ = napi::reference{env, *inherited.agent};
		env.module_class_ = napi::reference{env, *inherited.module_};
	};
	return napi::value<function_tag>::make(*this, js::free_function{initialize});
}

} // namespace backend_napi_v8
