module backend_napi_v8;
import :native_module;

namespace backend_napi_v8 {

native_module_handle::native_module_handle(const std::string& filename) :
		lib_{filename} {}

auto native_module_handle::class_template(environment& env) -> js::napi::value<js::class_tag_of<native_module_handle>> {
	return env.class_template(
		std::type_identity<native_module_handle>{},
		js::class_template{
			js::class_constructor{util::cw<"NativeModule">},
			js::class_static{util::cw<"create">, create},
		}
	);
}

auto native_module_handle::create(environment& env, std::string filename) -> js::forward<js::napi::value<>> {
#if __APPLE__
	filename += ".dylib";
#elif _WIN64
	filename += ".dll";
#else
	filename += ".so";
#endif
	auto [ promise, resolver ] = make_promise(
		env,
		[ filename = std::move(filename) ](environment& env) -> auto {
			return js::forward{native_module_handle::class_template(env).construct(env, filename)};
		}
	);
	resolver();
	return js::forward{promise};
}

} // namespace backend_napi_v8
