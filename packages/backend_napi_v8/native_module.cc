module;
#if __MUSL__
#include <dlfcn.h>
#endif
#include <cassert>
module backend_napi_v8;
import :lock;
import :module_;
import :native_module;
import isolated_vm;
import v8_js;

namespace backend_napi_v8 {

#if __MUSL__
void* promoted_self_handle = nullptr;
#endif

native_module_handle::native_module_handle(
	js::napi::uv_dlib lib,
	isolated_vm::detail::initialize_addon* initialize,
	create_native_module_options options,
	std::vector<std::u16string> names
) :
		lib_{std::move(lib)},
		initialize_{initialize},
		options_{std::move(options)},
		names_{std::move(names)} {}

auto native_module_handle::instantiate(environment& env, realm_handle* realm) -> forward_promise_type {
	auto [ promise, resolver ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js::iv8::shared_remote<v8::Module> module_record) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(agent), std::move(module_record))};
		}
	);
	if (realm == nullptr) {
		return js::forward{promise};
	}
	realm->agent().schedule(
		[ realm = realm->realm(),
			names = names_,
			options = options_,
			initialize = initialize_ ](
			const agent_handle::lock& lock,
			agent_handle agent,
			auto resolver
		) -> void {
			context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) -> void {
				auto addon_lock = isolated_vm::basic_lock_implementation{lock};
				auto module_result = v8::Local<v8::Module>{};
				auto make = [ & ](std::span<isolated_vm::value_of<prototype_tag>> values) -> void {
					auto v8_origin = js::transfer_in<v8::Local<v8::String>>(options.origin, lock);
					auto v8_names = js::transfer_in<std::vector<v8::Local<v8::String>>>(names, lock);
					auto v8_values = std::bit_cast<std::span<v8::Local<v8::Data>>>(values);
					module_result = js::iv8::module_record::create_synthetic(realm, v8_origin, v8_names, v8_values);
				};
				initialize(addon_lock, make);
				assert(module_result != v8::Local<v8::Module>{});
				resolver(std::move(agent), make_shared_remote(lock, module_result));
			});
		},
		realm->agent(),
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

auto native_module_handle::create(environment& env, std::string filename, create_native_module_options options) -> forward_promise_type {

#if __MUSL__
	// musl has some unique behavior with bare .so names. There is a lot of discussion here:

	// https://github.com/JuliaLang/julia/issues/40556
	// and also a totally deranged but impressive PR which mucks with internal dynamic so structures to
	// workaround it here:
	// https://github.com/JuliaPackaging/JLLWrappers.jl/pull/34/files

	// Anyway, this is a different approach which just loads our symbols into the global namespace,
	// which is also fine. nodejs forces your module to load with RTLD_LOCAL but if you reopen it you
	// can "promote" it.
	Dl_info info;
	dladdr(reinterpret_cast<void*>(&create), &info);
	promoted_self_handle = dlopen(info.dli_fname, RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
#endif

	if (options.suffix) {
		filename += *options.suffix;
	} else {
#if __APPLE__
		filename += ".dylib";
#elif _WIN64
		filename += ".dll";
#else
		filename += ".so";
#endif
	}
	auto [ promise, resolver ] = make_promise(
		env,
		[ filename = std::move(filename) ](environment& env, create_native_module_options options) -> auto {
			auto [ lib, names, initialize ] = isolated_vm::subscribe_registration([ & ]() -> auto {
				return js::napi::uv_dlib{filename};
			});
			auto handle = native_module_handle::class_template(env).construct(env, std::move(lib), initialize, std::move(options), names());
			return js::forward{handle};
		}
	);
	resolver(std::move(options));
	return js::forward{promise};
}

auto native_module_handle::unload_hook() -> void {
#if __MUSL__
	if (promoted_self_handle) {
		dlclose(std::exchange(promoted_self_handle, nullptr));
	}
#endif
}

} // namespace backend_napi_v8
