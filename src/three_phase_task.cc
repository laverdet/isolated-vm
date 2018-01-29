#include "three_phase_task.h"

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

ThreePhaseTask::Phase2Runner::Phase2Runner(
	unique_ptr<ThreePhaseTask> self,
	shared_ptr<ShareableIsolate> first_isolate_ref,
	shared_ptr<ShareableIsolate> second_isolate_ref,
	unique_ptr<Persistent<Promise::Resolver>> promise_persistent,
	unique_ptr<Persistent<Context>> context_persistent
) :
	self(std::move(self)),
	first_isolate_ref(std::move(first_isolate_ref)), second_isolate_ref(std::move(second_isolate_ref)),
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
				Unmaybe(promise_local->Reject(context_local, Exception::Error(v8_string("Isolate is disposed or disposing."))));
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
	ShareableIsolate& first_isolate = *first_isolate_ref;
	try {
		// Continue the task
		self->Phase2();
		second_isolate_ref->TaskWrapper(0); // TODO: this is filthy
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
				} catch (js_error_base& cc_error) {
					// An error was caught while running Phase3()
					if (ShareableIsolate::GetCurrent().IsNormalLifeCycle()) {
						assert(try_catch.HasCaught());
						// If Reject fails then I think that's bad..
						Unmaybe(promise_local->Reject(context_local, try_catch.Exception()));
						isolate->RunMicrotasks();
					}
				}
			}
		};
		first_isolate_ref->ScheduleTask(std::make_unique<Phase3Success>(std::move(self), std::move(promise_persistent), std::move(context_persistent)), false, true);
	} catch (js_error_base& cc_error) {
		// An error was caught while running Phase2(). Or perhaps first_isolate.ScheduleTask() threw.
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

		// Copy exception externally
		unique_ptr<ExternalCopy> err;
		if (try_catch.HasCaught()) {
			// Graceful JS exception
			Context::Scope context_scope(second_isolate_ref->DefaultContext());
			err = ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception());
		} else {
			// Isolate is probably in trouble
			assert(!second_isolate_ref->IsNormalLifeCycle());
			err = std::make_unique<ExternalCopyError>(ExternalCopyError::ErrorType::Error, "Isolate is disposed or disposing");
		}

		// Schedule a task to enter the first isolate so we can throw the error at the promise
		first_isolate.ScheduleTask(std::make_unique<Phase3Failure>(std::move(self), std::move(promise_persistent), std::move(context_persistent), std::move(err)), false, true);
	}
}

} // namespace ivm
