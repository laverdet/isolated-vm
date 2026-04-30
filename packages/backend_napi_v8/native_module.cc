module backend_napi_v8;
import isolated_vm;
import :native_module;

namespace backend_napi_v8 {

native_module_handle::native_module_handle(js::napi::uv_dlib lib, isolated_vm::detail::registration_signature* registration, void* data) :
		lib_{std::move(lib)},
		registration_{registration},
		registration_data_{data} {}

auto native_module_handle::instantiate(environment& env, agent_handle& agent) -> js::forward<js::napi::value_of<>> {
	auto [ promise, resolver ] = make_promise(env);
	agent.schedule(
		[ registration = registration_,
			registration_data = registration_data_ ](
			const agent_handle::lock& lock,
			auto resolver
		) -> void {
			auto module_environment = isolated_vm::environment{lock};
			registration(module_environment, registration_data);
			resolver.resolve(std::monostate{});
		},
		std::move(resolver)
	);
	return js::forward{promise};
	// auto addon_environment = isolated_vm::environment{env};
}

auto native_module_handle::class_template(environment& env) -> js::napi::value_of<js::class_tag_of<native_module_handle>> {
	return env.class_template(
		std::type_identity<native_module_handle>{},
		js::class_template{
			js::class_constructor{util::cw<"NativeModule">},
			js::class_static{util::cw<"create">, create},
			js::class_method{util::cw<"instantiate">, util::fn<&native_module_handle::instantiate>},
		}
	);
}

auto native_module_handle::create(environment& env, std::string filename) -> js::forward<js::napi::value_of<>> {
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
			auto [ lib, registration ] = isolated_vm::subscribe_registration([ & ]() -> auto {
				return js::napi::uv_dlib{filename};
			});
			auto [ callback, data ] = registration;
			auto handle = native_module_handle::class_template(env).construct(env, std::move(lib), callback, data);
			return js::forward{handle};
		}
	);
	resolver();
	return js::forward{promise};
}

} // namespace backend_napi_v8
