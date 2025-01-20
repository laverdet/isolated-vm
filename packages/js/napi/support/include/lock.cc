export module napi_js.lock;
import isolated_js;
import ivm.utility;
import nodejs;
import v8_js;
import v8;

namespace js {

export class napi_witness_lock : util::non_moveable {
	public:
		explicit napi_witness_lock(napi_env env) :
				env_{env} {}

		[[nodiscard]] auto env() const -> napi_env { return env_; }

	private:
		napi_env env_;
};

export class napi_isolate_witness_lock
		: public napi_witness_lock,
			public iv8::isolate_implicit_witness_lock {
	public:
		napi_isolate_witness_lock(napi_env env, v8::Isolate* isolate) :
				napi_witness_lock{env},
				isolate_implicit_witness_lock{isolate} {}
};

} // namespace js
