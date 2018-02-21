#include "three_phase_task.h"
#include "stack_trace.h"
#include "../external_copy.h"

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * Phase2Runner implementation
 */
ThreePhaseTask::Phase2Runner::Phase2Runner(
	unique_ptr<ThreePhaseTask> self,
	unique_ptr<CalleeInfo> info
) :
	self(std::move(self)),
	info(std::move(info))	{}

ThreePhaseTask::Phase2Runner::~Phase2Runner() {
	if (!did_run) {
		// The task never got to run
		struct Phase3Orphan : public Runnable {
			unique_ptr<ThreePhaseTask> self;
			unique_ptr<CalleeInfo> info;

			Phase3Orphan(
				unique_ptr<ThreePhaseTask> self,
				unique_ptr<CalleeInfo> info
			) :
				self(std::move(self)),
				info(std::move(info)) {}

			void Run() final {
				// Revive our persistent handles
				Isolate* isolate = Isolate::GetCurrent();
				auto context_local = info->Deref<1>();
				Context::Scope context_scope(context_local);
				auto promise_local = info->Deref<0>();
				// Throw from promise
				Unmaybe(promise_local->Reject(context_local,
					StackTraceHolder::AttachStack(Exception::Error(v8_string("Isolate is disposed")), info->Deref<2>())
				));
				isolate->RunMicrotasks();
			}
		};
		// Schedule a throw task back in first isolate
		auto holder = info->GetIsolateHolder();
		holder->ScheduleTask(
			std::make_unique<Phase3Orphan>(
				std::move(self),
				std::move(info)
			), false, true
		);
	}
}

void ThreePhaseTask::Phase2Runner::Run() {
	did_run = true;
	TryCatch try_catch(Isolate::GetCurrent());
	auto second_isolate = IsolateEnvironment::GetCurrent();
	// This class will be used if Phase2() throws an error
	struct Phase3Failure : public Runnable {
		unique_ptr<ThreePhaseTask> self;
		unique_ptr<CalleeInfo> info;
		unique_ptr<ExternalCopy> err;

		Phase3Failure(
			unique_ptr<ThreePhaseTask> self,
			unique_ptr<CalleeInfo> info,
			unique_ptr<ExternalCopy> err
		) :
			self(std::move(self)),
			info(std::move(info)),
			err(std::move(err)) {}

		void Run() final {
			// Revive our persistent handles
			Isolate* isolate = Isolate::GetCurrent();
			auto context_local = info->Deref<1>();
			Context::Scope context_scope(context_local);
			auto promise_local = info->Deref<0>();
			Local<Value> rejection;
			if (err) {
				rejection = err->CopyInto();
			} else {
				rejection = Exception::Error(v8_string("An exception was thrown. Sorry I don't know more."));
			}
			// If Reject fails then I think that's bad..
			Unmaybe(promise_local->Reject(context_local,
				StackTraceHolder::ChainStack(rejection, info->Deref<2>())
			));
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
			unique_ptr<CalleeInfo> info;

			Phase3Success(
				unique_ptr<ThreePhaseTask> self,
				unique_ptr<CalleeInfo> info
			) :
				self(std::move(self)),
				info(std::move(info)) {}

			void Run() final {
				Isolate* isolate = Isolate::GetCurrent();
				auto context_local = info->Deref<1>();
				Context::Scope context_scope(context_local);
				auto promise_local = info->Deref<0>();
				TryCatch try_catch(isolate);
				try {
					// Final callback
					Unmaybe(promise_local->Resolve(context_local, self->Phase3()));
					isolate->RunMicrotasks();
				} catch (const js_runtime_error& cc_error) {
					// An error was caught while running Phase3()
					assert(try_catch.HasCaught());
					// If Reject fails then I think that's bad..
					Unmaybe(promise_local->Reject(context_local,
						StackTraceHolder::AttachStack(try_catch.Exception(), info->Deref<2>())
					));
					isolate->RunMicrotasks();
				} catch (const js_fatal_error& cc_error) {
					// The invoking isolate is out of memory..
				}
			}
		};
		auto holder = info->GetIsolateHolder();
		holder->ScheduleTask(std::make_unique<Phase3Success>(std::move(self), std::move(info)), false, true);
		return;
	} catch (const js_runtime_error& cc_error) {
		// An error was caught while running Phase2(). Or perhaps holder->ScheduleTask() threw.
		assert(try_catch.HasCaught());
		Context::Scope context_scope(second_isolate->DefaultContext());
		err = ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception());
	} catch (const js_fatal_error& cc_error) {
		// Bad news
		err = std::make_unique<ExternalCopyError>(ExternalCopyError::ErrorType::Error, "Isolate is disposed");
	}

	// Schedule a task to enter the first isolate so we can throw the error at the promise
	auto holder = info->GetIsolateHolder();
	holder->ScheduleTask(std::make_unique<Phase3Failure>(
		std::move(self),
		std::move(info),
		std::move(err)
	), false, true);
}

/**
 * Phase2RunnerIgnored implementation
 */
ThreePhaseTask::Phase2RunnerIgnored::Phase2RunnerIgnored(unique_ptr<ThreePhaseTask> self) : self(std::move(self)) {}

void ThreePhaseTask::Phase2RunnerIgnored::Run() {
	TryCatch try_catch(Isolate::GetCurrent());
	try {
		self->Phase2();
		IsolateEnvironment::GetCurrent()->TaskEpilogue();
	} catch (const js_runtime_error& cc_error) {
	} catch (const js_fatal_error& cc_error) {}
}

/**
 * RunSync implementation
 */
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
				if (!error) {
					error = std::make_unique<ExternalCopyError>(ExternalCopyError::ErrorType::Error,
						"An object was thrown from supplied code within isolated-vm, but that object was not an instance of `Error`."
					);
				}
			} catch (const js_fatal_error& cc_error) {
				error = std::make_unique<ExternalCopyError>(ExternalCopyError::ErrorType::Error, "Isolate has exhausted v8 heap space.");
			}
		}
		// Check error
		if (error) {
			Isolate* isolate = Isolate::GetCurrent();
			Local<Value> error_copy = error->CopyInto();
			if (error_copy->IsObject()) {
				isolate->ThrowException(StackTraceHolder::ChainStack(error_copy, StackTrace::CurrentStackTrace(isolate, 10)));
			} else {
				// Good luck on tracking this error lmbo
				isolate->ThrowException(error_copy);
			}
			return Undefined(isolate);
		}
	}
	// Final phase
	try {
		return Phase3();
	} catch (const js_fatal_error& cc_error) {
		return Undefined(Isolate::GetCurrent());
	}
}

} // namespace ivm
