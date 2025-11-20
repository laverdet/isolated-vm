module v8_js;
import :lock;
import v8;

namespace js::iv8 {

// `isolate_lock_witness`
auto isolate_lock_witness::make_witness(v8::Isolate* isolate) -> isolate_lock_witness {
	return isolate_lock_witness{isolate};
}

// `isolate_destructor_lock`
isolate_destructor_lock::isolate_destructor_lock(v8::Isolate* isolate) :
		isolate_lock_witness{isolate_lock_witness::make_witness(isolate)},
		locker_{isolate},
		isolate_scope_{isolate} {}

// `isolate_execution_lock`
isolate_execution_lock::isolate_execution_lock(v8::Isolate* isolate) :
		isolate_destructor_lock{isolate},
		handle_scope_{isolate} {}

// `isolate_exit_scope`
isolate_exit_scope::isolate_exit_scope(v8::Isolate* isolate) :
		isolate_{isolate} {
	isolate_->Exit();
}

isolate_exit_scope::~isolate_exit_scope() {
	isolate_->Enter();
}

// `isolate_unlock`
isolate_unlock::isolate_unlock(isolate_lock_witness lock) :
		isolate_exit_scope_{lock.isolate()},
		unlocker_{lock.isolate()} {}

// `context_lock_witness`
auto context_lock_witness::make_witness(isolate_lock_witness lock, v8::Local<v8::Context> context) -> context_lock_witness {
	return context_lock_witness{lock, context};
}

// `context_managed_lock`
context_managed_lock::context_managed_lock(isolate_lock_witness lock, v8::Local<v8::Context> context) :
		context_lock_witness{context_lock_witness::make_witness(lock, context)},
		context_scope_{context} {}

} // namespace js::iv8
