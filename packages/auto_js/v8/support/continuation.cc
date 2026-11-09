export module v8_js:continuation;
import :lock;
import util;
import v8;

namespace js::iv8 {

// Replaces the isolate's continuation-preserved embedder data for the duration of the scope.
export class continuation_scope : util::non_moveable {
	public:
		continuation_scope(isolate_lock_witness lock, v8::Local<v8::Data> data) :
				isolate_{lock.isolate()},
				previous_{isolate_->GetContinuationPreservedEmbedderData()} {
			isolate_->SetContinuationPreservedEmbedderData(data);
		}

		~continuation_scope() {
			isolate_->SetContinuationPreservedEmbedderData(previous_);
		}

	private:
		v8::Isolate* isolate_;
		v8::Local<v8::Data> previous_;
};

} // namespace js::iv8
