module;
#include <cassert>
module backend_napi_v8;
import :lock;
import :module_;
import :native_module;
import isolated_vm;
import v8_js;

namespace backend_napi_v8 {

native_module_handle::native_module_handle(
	js::napi::uv_dlib lib,
	isolated_vm::detail::initialize_addon* initialize,
	std::u16string origin,
	std::vector<std::u16string> names
) :
		lib_{std::move(lib)},
		initialize_{initialize},
		origin_{std::move(origin)},
		names_{std::move(names)} {}

auto native_module_handle::instantiate(environment& env, realm_handle& realm) -> js::forward<js::napi::value_of<>> {
	auto [ promise, resolver ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js::iv8::shared_remote<v8::Module> module_record) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(agent), std::move(module_record))};
		}
	);
	realm.agent().schedule(
		[ realm = realm.realm(),
			names = names_,
			origin = origin_,
			initialize = initialize_ ](
			const agent_handle::lock& lock,
			agent_handle agent,
			auto resolver
		) -> void {
			context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) mutable -> void {
				auto addon_environment = isolated_vm::environment{lock};
				auto module_result = v8::Local<v8::Module>{};
				auto make = [ & ](std::span<isolated_vm::value_of<>> values) -> void {
					auto v8_origin = js::transfer_in<v8::Local<v8::String>>(origin, lock);
					auto v8_names = js::transfer_in<std::vector<v8::Local<v8::String>>>(names, lock);
					auto v8_values = std::bit_cast<std::span<v8::Local<v8::Value>>>(values);
					module_result = js::iv8::module_record::create_synthetic(realm, v8_origin, v8_names, v8_values);
				};
				initialize(addon_environment, make);
				assert(module_result != v8::Local<v8::Module>{});
				resolver(std::move(agent), make_shared_remote(lock, module_result));
			});
		},
		realm.agent(),
		std::move(resolver)
	);
	return js::forward{promise};
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

auto native_module_handle::create(environment& env, std::string filename, std::u16string origin) -> js::forward<js::napi::value_of<>> {
#if __APPLE__
	filename += ".dylib";
#elif _WIN64
	filename += ".dll";
#else
	filename += ".so";
#endif
	auto [ promise, resolver ] = make_promise(
		env,
		[ filename = std::move(filename) ](environment& env, std::u16string origin) -> auto {
			auto [ lib, names, initialize ] = isolated_vm::subscribe_registration([ & ]() -> auto {
				return js::napi::uv_dlib{filename};
			});
			auto handle = native_module_handle::class_template(env).construct(env, std::move(lib), initialize, std::move(origin), names());
			return js::forward{handle};
		}
	);
	resolver(std::move(origin));
	return js::forward{promise};
}

} // namespace backend_napi_v8
