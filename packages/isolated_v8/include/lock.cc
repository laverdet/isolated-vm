export module isolated_v8.lock;
import v8;
import ivm.utility;

namespace isolated_v8 {

// Does no actual locking. Used as a witness for operations which require a lock on an isolate.
export class isolate_lock : util::non_moveable {
	public:
		explicit isolate_lock(v8::Isolate* isolate) :
				isolate_{isolate} {}

		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

	private:
		v8::Isolate* isolate_;
};

// Locks the v8 isolate
export class isolate_scope : public isolate_lock {
	public:
		explicit isolate_scope(v8::Isolate* isolate) :
				isolate_lock{isolate},
				locker_{isolate},
				scope_{isolate} {}

	private:
		v8::Locker locker_;
		v8::Isolate::Scope scope_;
};

// Enters a context
export class context_scope {
	public:
		explicit context_scope(v8::Local<v8::Context> context) :
				scope_{context} {}

	private:
		v8::Context::Scope scope_;
};

} // namespace isolated_v8
