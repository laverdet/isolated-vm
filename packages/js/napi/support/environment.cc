module;
#include <memory>
export module napi_js.environment;
import ivm.utility;
import napi_js.utility;
import nodejs;
import v8_js;
import v8;

namespace js::napi {

// A reference to an environment is used as the lock witness
export class environment {
	public:
		explicit environment(napi_env env) :
				env_{env},
				isolate_{v8::Isolate::GetCurrent()} {}

		[[nodiscard]] explicit operator napi_env() const { return env_; }
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
		static auto make(napi_env env, auto&&... args) -> Type&
			requires std::constructible_from<Type, napi_env, decltype(args)...> {
			auto instance = std::make_unique<Type>(env, std::forward<decltype(args)>(args)...);
			napi_finalize finalizer = [](napi_env /*env*/, void* data, void* /*hint*/) {
				delete static_cast<Type*>(data);
			};
			js::napi::invoke0(napi_set_instance_data, env, instance.get(), finalizer, nullptr);
			// That's a spicy dereference
			return *instance.release();
		}
};

export template <class Type>
concept auto_environment = std::derived_from<Type, environment>;

} // namespace js::napi
