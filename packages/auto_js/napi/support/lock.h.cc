export module napi_js:lock;
import :environment;
import nodejs;
import std;
import util;
import v8;

namespace js::napi {

// Environment lock witness. Similar to `iv8::isolate_lock_witness`, this provides some assurance
// that the environment is locked in this thread. In the "fast" (nodejs) case it also carries the
// isolate and the current context.
export class environment_lock_witness {
	protected:
		environment_lock_witness(napi_env env, v8::Isolate* isolate, v8::Local<v8::Context> context) :
				env_{env},
				isolate_{isolate},
				context_{context} {}

	public:
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_env() const { return env_; }
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }
		[[nodiscard]] auto context() const -> v8::Local<v8::Context> { return context_; }
		[[nodiscard]] static auto make_witness(napi_env env, v8::Isolate* isolate, v8::Local<v8::Context> context) -> environment_lock_witness {
			return environment_lock_witness{env, isolate, context};
		}
		[[nodiscard]] static auto make_witness(napi::environment& env) -> environment_lock_witness;

	private:
		napi_env env_;
		v8::Isolate* isolate_;
		v8::Local<v8::Context> context_;
};

// Environment lock witness with backing environment reference
export template <class Environment>
class environment_lock_witness_of
		: public util::pointer_facade,
			public environment_lock_witness {
	public:
		environment_lock_witness_of(environment_lock_witness witness, Environment& env) :
				environment_lock_witness{witness},
				env_{env} {}

		[[nodiscard]] auto operator*() const -> Environment& { return env_; }

	private:
		std::reference_wrapper<Environment> env_;
};

// In the fast case this installs a `v8::TryCatch` which catches exceptions thrown by direct v8
// operations.
export class environment_try_catch : util::non_moveable {
	public:
		explicit environment_try_catch(environment_lock_witness lock);
		~environment_try_catch();
		[[nodiscard]] auto take_exception() -> napi_value;

	private:
		std::optional<v8::TryCatch> try_catch_;
};

} // namespace js::napi
