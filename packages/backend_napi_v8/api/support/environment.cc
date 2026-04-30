export module isolated_vm:support.environment;
import :support.environment_fwd;
import v8_js;

namespace isolated_vm {

class environment {
	public:
		explicit environment(js::iv8::isolate_lock_witness lock) :
				isolate_{lock.isolate()} {}

		[[nodiscard]] auto witness() const -> js::iv8::context_lock_witness {
			auto witness = js::iv8::isolate_lock_witness::make_witness(isolate_);
			return js::iv8::context_lock_witness::from_isolate(witness);
		}

	private:
		v8::Isolate* isolate_;
};

} // namespace isolated_vm
