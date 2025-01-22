module;
#include <memory>
export module napi_js.environment;
import ivm.utility;
import napi_js.utility;
import nodejs;
import v8;

namespace napi_js {

export class environment {
	public:
		using napi_env_type = napi_env;

		explicit environment(napi_env env) :
				env_{env},
				isolate_{v8::Isolate::GetCurrent()} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		[[nodiscard]] operator napi_env() const { return env_; }
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

	private:
		napi_env env_;
		v8::Isolate* isolate_;
};

export template <class Type>
class environment_of : public environment {
	protected:
		friend Type;
		explicit environment_of(napi_env env) :
				environment{env} {}

	public:
		static auto make(napi_env_type env, auto&&... args) -> Type&
			requires std::constructible_from<Type, napi_env, decltype(args)...> {
			auto instance = std::make_unique<Type>(env, std::forward<decltype(args)>(args)...);
			napi_finalize finalizer = [](napi_env_type /*env*/, void* data, void* /*hint*/) {
				delete static_cast<Type*>(data);
			};
			js::napi::invoke0(napi_set_instance_data, env, instance.get(), finalizer, nullptr);
			// That's a spicy dereference
			return *instance.release();
		}
};

} // namespace napi_js
