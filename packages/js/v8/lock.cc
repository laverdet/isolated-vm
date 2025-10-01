export module v8_js:lock;
import ivm.utility;
import v8;

namespace js::iv8 {

// Isolate lock witness. Provides some assurance that the given isolate is locked in this thread.
export class isolate_lock_witness {
	private:
		explicit isolate_lock_witness(v8::Isolate* isolate) :
				isolate_{isolate} {}

	public:
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

		[[nodiscard]] static auto make_witness(v8::Isolate* isolate) -> isolate_lock_witness {
			return isolate_lock_witness{isolate};
		}

	private:
		v8::Isolate* isolate_;
};

// Locks without a `v8::HandleScope`
export class isolate_destructor_lock : public isolate_lock_witness {
	public:
		explicit isolate_destructor_lock(v8::Isolate* isolate) :
				isolate_lock_witness{isolate_lock_witness::make_witness(isolate)},
				locker_{isolate},
				isolate_scope_{isolate} {}

	private:
		v8::Locker locker_;
		v8::Isolate::Scope isolate_scope_;
};

// Performs a full isolate lock which may allocate handles
export class isolate_execution_lock : public isolate_destructor_lock {
	public:
		explicit isolate_execution_lock(v8::Isolate* isolate) :
				isolate_destructor_lock{isolate},
				handle_scope_{isolate} {}

	private:
		v8::HandleScope handle_scope_;
};

// Yield v8 lock
export class isolate_unlock : util::non_copyable {
	private:
		class isolate_exit : util::non_copyable {
			public:
				explicit isolate_exit(v8::Isolate* isolate) :
						isolate_{isolate} {
					isolate_->Exit();
				}
				~isolate_exit() { isolate_->Enter(); }

			private:
				v8::Isolate* isolate_;
		};

	public:
		explicit isolate_unlock(const isolate_lock_witness& lock) :
				isolate_exit_{lock.isolate()},
				unlocker_{lock.isolate()} {}

	private:
		isolate_exit isolate_exit_;
		v8::Unlocker unlocker_;
};

// Context lock witness
export class context_lock_witness : public isolate_lock_witness {
	private:
		explicit context_lock_witness(
			const js::iv8::isolate_lock_witness& lock,
			v8::Local<v8::Context> context
		) :
				isolate_lock_witness{lock},
				context_{context} {}

	public:
		[[nodiscard]] auto context() const -> v8::Local<v8::Context> { return context_; }

		[[nodiscard]] static auto make_witness(
			const js::iv8::isolate_lock_witness& lock,
			v8::Local<v8::Context> context
		) -> context_lock_witness {
			return context_lock_witness{lock, context};
		}

	private:
		v8::Local<v8::Context> context_;
};

// Enters the context.
export class context_managed_lock : public context_lock_witness {
	public:
		explicit context_managed_lock(const js::iv8::isolate_lock_witness& lock, v8::Local<v8::Context> context) :
				context_lock_witness{context_lock_witness::make_witness(lock, context)},
				context_scope_{context} {}

	private:
		v8::Context::Scope context_scope_;
};

} // namespace js::iv8
