export module v8_js.lock;
import ivm.utility;
import v8;

namespace js::iv8 {

// Isolate lock witness
export class isolate_lock : util::non_copyable {
	protected:
		explicit isolate_lock(v8::Isolate* isolate) :
				isolate_{isolate} {}

	public:
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

	private:
		v8::Isolate* isolate_;
};

// Actually performs a lock.
export class isolate_managed_lock : public isolate_lock {
	public:
		explicit isolate_managed_lock(v8::Isolate* isolate) :
				isolate_lock{isolate},
				locker_{isolate},
				isolate_scope_{isolate} {}

	private:
		v8::Locker locker_;
		v8::Isolate::Scope isolate_scope_;
};

// Does not lock! It should only be instantiated when there is an implicit lock somehow.
export class isolate_implicit_witness_lock : public isolate_lock {
	public:
		explicit isolate_implicit_witness_lock(v8::Isolate* isolate) :
				isolate_lock{isolate} {}
};

// Context lock witness
export class context_lock : public isolate_lock {
	protected:
		explicit context_lock(const js::iv8::isolate_lock& lock, v8::Local<v8::Context> context) :
				isolate_lock{lock.isolate()},
				context_{context} {}

	public:
		[[nodiscard]] auto context() const -> v8::Local<v8::Context> { return context_; }

	private:
		v8::Local<v8::Context> context_;
};

// Enters the context.
export class context_managed_lock : public context_lock {
	public:
		explicit context_managed_lock(const js::iv8::isolate_lock& lock, v8::Local<v8::Context> context) :
				context_lock{lock, context},
				context_scope_{context} {}

	private:
		v8::Context::Scope context_scope_;
};

// Does not lock! It should only be instantiated when there is an implicit lock somehow.
export class context_implicit_witness_lock : public context_lock {
	public:
		explicit context_implicit_witness_lock(const js::iv8::isolate_lock& lock, v8::Local<v8::Context> context) :
				context_lock{lock, context} {}
};

} // namespace js::iv8
