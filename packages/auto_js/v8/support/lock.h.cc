module;
#include <concepts>
#include <functional>
export module v8_js:lock;
import util;
import v8;

namespace js::iv8 {

// Forward declaration
export template <class Agent, class... Implements>
class context_lock_witness_of;

// Isolate lock witness. Provides some assurance that the given isolate is locked in this thread.
export class isolate_lock_witness {
	private:
		explicit isolate_lock_witness(v8::Isolate* isolate) : isolate_{isolate} {}

	public:
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }
		[[nodiscard]] static auto make_witness(v8::Isolate* isolate) -> isolate_lock_witness;

	private:
		v8::Isolate* isolate_;
};

// Helper which does not instantiate `Implements` in the case it requires a context lock instead of
// an isolate lock. We still need these "implements" types to carry forward from function templates
// to call signature, through `context_lock_witness_of`.
template <class Agent, class Implements>
struct isolate_lock_implements {
		isolate_lock_implements(isolate_lock_witness& /*witness*/, Agent& /*agent*/) {}
};

template <class Agent, std::constructible_from<isolate_lock_witness, Agent&> Implements>
struct isolate_lock_implements<Agent, Implements> : Implements {
		isolate_lock_implements(isolate_lock_witness& witness, Agent& agent) : Implements{witness, agent} {}
};

// Isolate lock witness with backing agent host reference
export template <class Agent, class... Implements>
class isolate_lock_witness_of
		: public util::pointer_facade,
			public isolate_lock_witness,
			public isolate_lock_implements<Agent, Implements>... {
	public:
		using isolate_lock_witness::isolate;

		isolate_lock_witness_of(isolate_lock_witness& witness, Agent& agent) :
				isolate_lock_witness{witness},
				isolate_lock_implements<Agent, Implements>{witness, agent}...,
				agent_{agent} {}

		[[nodiscard]] auto operator*() const -> Agent& { return agent_; }

	private:
		std::reference_wrapper<Agent> agent_;
};

// Locks without a `v8::HandleScope`
export class isolate_destructor_lock : public isolate_lock_witness {
	public:
		explicit isolate_destructor_lock(v8::Isolate* isolate);

	private:
		v8::Locker locker_;
		v8::Isolate::Scope isolate_scope_;
};

// Performs a full isolate lock which may allocate handles
export class isolate_execution_lock : public isolate_destructor_lock {
	public:
		explicit isolate_execution_lock(v8::Isolate* isolate);

	private:
		v8::HandleScope handle_scope_;
};

// Exit isolate, re-enter on destruction
class isolate_exit_scope : util::non_copyable {
	public:
		explicit isolate_exit_scope(v8::Isolate* isolate);
		~isolate_exit_scope();

	private:
		v8::Isolate* isolate_;
};

// Yield v8 lock
export class isolate_unlock {
	public:
		explicit isolate_unlock(isolate_lock_witness lock);

	private:
		isolate_exit_scope isolate_exit_scope_;
		v8::Unlocker unlocker_;
};

// Context lock witness
export class context_lock_witness : public isolate_lock_witness {
	private:
		context_lock_witness(isolate_lock_witness& lock, v8::Local<v8::Context> context) :
				isolate_lock_witness{lock},
				context_{context} {}

	public:
		[[nodiscard]] auto context() const -> v8::Local<v8::Context> { return context_; }
		[[nodiscard]] static auto make_witness(isolate_lock_witness lock, v8::Local<v8::Context> context) -> context_lock_witness;

	private:
		v8::Local<v8::Context> context_;
};

// Context lock witness with backing agent host reference
export template <class Agent, class... Implements>
class context_lock_witness_of
		: public util::pointer_facade,
			public context_lock_witness,
			public Implements... {
	public:
		using context_lock_witness::isolate;

		context_lock_witness_of(context_lock_witness& witness, Agent& agent) :
				context_lock_witness{witness},
				Implements{witness, agent}...,
				agent_{agent} {}

		context_lock_witness_of(context_lock_witness& witness, const isolate_lock_witness_of<Agent, Implements...>& lock) :
				context_lock_witness_of{witness, *lock} {}

		[[nodiscard]] auto operator*() const -> Agent& { return agent_; }

	private:
		std::reference_wrapper<Agent> agent_;
};

template <class Agent, class... Implements>
context_lock_witness_of(context_lock_witness&, const isolate_lock_witness_of<Agent, Implements...>&) -> context_lock_witness_of<Agent, Implements...>;

// Enters the context.
export class context_managed_lock : public context_lock_witness {
	public:
		explicit context_managed_lock(isolate_lock_witness lock, v8::Local<v8::Context> context);

	private:
		v8::Context::Scope context_scope_;
};

// Invoke an operation within a context
export auto context_scope_operation(isolate_lock_witness lock, v8::Local<v8::Context> context, auto operation) -> decltype(auto) {
	auto context_lock = context_managed_lock{lock, context};
	return operation(context_lock);
}

export template <class Agent, class... Implements>
auto context_scope_operation(const isolate_lock_witness_of<Agent, Implements...>& lock, v8::Local<v8::Context> context, auto operation) -> decltype(auto) {
	auto context_lock = context_managed_lock{lock, context};
	auto witness = context_lock_witness_of{context_lock, lock};
	return operation(witness);
}

} // namespace js::iv8
