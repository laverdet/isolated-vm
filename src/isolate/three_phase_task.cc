#include "three_phase_task.h"
#include "../external_copy.h"

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

ThreePhaseTask::Phase2Runner::Phase2Runner(
	unique_ptr<ThreePhaseTask> self,
	shared_ptr<IsolateHolder> first_isolate_ref,
	unique_ptr<Persistent<Promise::Resolver>> promise_persistent,
	unique_ptr<Persistent<Context>> context_persistent
) :
	self(std::move(self)),
	first_isolate_ref(std::move(first_isolate_ref)),
	promise_persistent(std::move(promise_persistent)), context_persistent(std::move(context_persistent)) {}

ThreePhaseTask::Phase2Runner::~Phase2Runner() {
	if (!did_run) {
		// The task never got to run
		struct Phase3Orphan : public Runnable {
			unique_ptr<ThreePhaseTask> self;
			unique_ptr<Persistent<Promise::Resolver>> promise_persistent;
			unique_ptr<Persistent<Context>> context_persistent;

			Phase3Orphan(
				unique_ptr<ThreePhaseTask> self,
				unique_ptr<Persistent<Promise::Resolver>> promise_persistent,
				unique_ptr<Persistent<Context>> context_persistent
			) : self(std::move(self)), promise_persistent(std::move(promise_persistent)), context_persistent(std::move(context_persistent)) {}

			void Run() final {
				// Revive our persistent handles
				Isolate* isolate = Isolate::GetCurrent();
				auto context_local = Local<Context>::New(isolate, *context_persistent);
				Context::Scope context_scope(context_local);
				auto promise_local = Local<Promise::Resolver>::New(isolate, *promise_persistent);
				// Throw from promise
				Unmaybe(promise_local->Reject(context_local, Exception::Error(v8_string("Isolate is disposed"))));
				isolate->RunMicrotasks();
			}
		};
		// Schedule a throw task back in first isolate
		first_isolate_ref->ScheduleTask(std::make_unique<Phase3Orphan>(std::move(self), std::move(promise_persistent), std::move(context_persistent)), false, true);
	}
}

void ThreePhaseTask::Phase2Runner::Run() {
	did_run = true;
	TryCatch try_catch(Isolate::GetCurrent());
	auto second_isolate = IsolateEnvironment::GetCurrent();
	// This class will be used if Phase2() throws an error
	struct Phase3Failure : public Runnable {
		unique_ptr<ThreePhaseTask> self;
		unique_ptr<Persistent<Promise::Resolver>> promise_persistent;
		unique_ptr<Persistent<Context>> context_persistent;
		unique_ptr<ExternalCopy> err;

		Phase3Failure(
			unique_ptr<ThreePhaseTask> self,
			unique_ptr<Persistent<Promise::Resolver>> promise_persistent,
			unique_ptr<Persistent<Context>> context_persistent,
			unique_ptr<ExternalCopy> err
		) : self(std::move(self)), promise_persistent(std::move(promise_persistent)), context_persistent(std::move(context_persistent)), err(std::move(err)) {}

		void Run() final {
			// Revive our persistent handles
			Isolate* isolate = Isolate::GetCurrent();
			auto context_local = Local<Context>::New(isolate, *context_persistent);
			Context::Scope context_scope(context_local);
			auto promise_local = Local<Promise::Resolver>::New(isolate, *promise_persistent);
			Local<Value> rejection;
			if (err) {
				rejection = err->CopyInto();
			} else {
				rejection = Exception::Error(v8_string("An exception was thrown. Sorry I don't know more."));
			}
			// If Reject fails then I think that's bad..
			Unmaybe(promise_local->Reject(context_local, rejection));
			isolate->RunMicrotasks();
		}
	};
	unique_ptr<ExternalCopy> err;
	try {
		// Continue the task
		self->Phase2();
		second_isolate->TaskEpilogue();
		// Finish back in first isolate
		struct Phase3Success : public Runnable {
			unique_ptr<ThreePhaseTask> self;
			unique_ptr<Persistent<Promise::Resolver>> promise_persistent;
			unique_ptr<Persistent<Context>> context_persistent;

			Phase3Success(
				unique_ptr<ThreePhaseTask> self,
				unique_ptr<Persistent<Promise::Resolver>> promise_persistent,
				unique_ptr<Persistent<Context>> context_persistent
			) : self(std::move(self)), promise_persistent(std::move(promise_persistent)), context_persistent(std::move(context_persistent)) {}

			void Run() final {
				Isolate* isolate = Isolate::GetCurrent();
				auto context_local = Local<Context>::New(isolate, *context_persistent);
				Context::Scope context_scope(context_local);
				auto promise_local = Local<Promise::Resolver>::New(isolate, *promise_persistent);
				TryCatch try_catch(isolate);
				try {
					// Final callback
					Unmaybe(promise_local->Resolve(context_local, self->Phase3()));
					isolate->RunMicrotasks();
				} catch (const js_runtime_error& cc_error) {
					// An error was caught while running Phase3()
					assert(try_catch.HasCaught());
					// If Reject fails then I think that's bad..
					Unmaybe(promise_local->Reject(context_local, try_catch.Exception()));
					isolate->RunMicrotasks();
				}
			}
		};
		first_isolate_ref->ScheduleTask(std::make_unique<Phase3Success>(std::move(self), std::move(promise_persistent), std::move(context_persistent)), false, true);
		return;
	} catch (const js_runtime_error& cc_error) {
		// An error was caught while running Phase2(). Or perhaps first_isolate.ScheduleTask() threw.
		assert(try_catch.HasCaught());
		Context::Scope context_scope(second_isolate->DefaultContext());
		err = ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception());
	} catch (const js_fatal_error& cc_error) {
		// Bad news
		err = std::make_unique<ExternalCopyError>(ExternalCopyError::ErrorType::Error, "Isolate is disposed");
	}

	// Schedule a task to enter the first isolate so we can throw the error at the promise
	first_isolate_ref->ScheduleTask(std::make_unique<Phase3Failure>(std::move(self), std::move(promise_persistent), std::move(context_persistent), std::move(err)), false, true);
}

Local<Value> ThreePhaseTask::RunSync(IsolateHolder& second_isolate) {
	// Grab a reference to second isolate
	auto second_isolate_ref = second_isolate.GetIsolate();
	if (!second_isolate_ref) {
		throw js_generic_error("Isolated is disposed");
	}
	if (second_isolate_ref->GetIsolate() == Isolate::GetCurrent()) {
		// Shortcut when calling a sync method belonging to the currently entered isolate. This avoids
		// the deadlock protection below
		try {
			Phase2();
		} catch (const js_fatal_error& cc_error) {
			// TODO: This assert() is true for me in testing but I'm leaving it out for now because I'm
			// not sure if it's true in all cases.
			// assert(second_isolate_ref->GetIsolate()->IsExecutionTerminating());

			// Skip Phase3()
			return Undefined(Isolate::GetCurrent());
		}
	} else {
		// Deadlock protection
		if (!IsolateEnvironment::ExecutorLock::IsDefaultThread()) {
			throw js_generic_error(
				"Calling a synchronous isolated-vm function from within an asynchronous isolated-vm function is not allowed."
			);
		}
		// Run Phase2 and copy error to `error`
		unique_ptr<ExternalCopy> error;
		bool is_recursive = Locker::IsLocked(second_isolate_ref->GetIsolate());
		{
			IsolateEnvironment::ExecutorLock lock(*second_isolate_ref);
			TryCatch try_catch(*second_isolate_ref);
			try {
				Phase2();
				if (!is_recursive) {
					second_isolate_ref->TaskEpilogue();
				}
			} catch (const js_runtime_error& cc_error) {
				assert(try_catch.HasCaught());
				Context::Scope context_scope(second_isolate_ref->DefaultContext());
				error = ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception());
			} catch (const js_fatal_error& cc_error) {
				error = std::make_unique<ExternalCopyError>(ExternalCopyError::ErrorType::Error, "Isolate has exhausted v8 heap space.");
			}
		}
		// Check error
		if (error) {
			Isolate::GetCurrent()->ThrowException(error->CopyInto());
			return Undefined(Isolate::GetCurrent());
		}
	}
	// Final phase
	return Phase3();
}

} // namespace ivm
