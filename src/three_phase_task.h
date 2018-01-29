#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "util.h"
#include <memory>

namespace ivm {

/**
 * Most operations in this library can be decomposed into three phases.
 *
 * - Phase 1 [Isolate 1]: copy data out of current isolate
 * - Phase 2 [Isolate 2]: copy data into new isolate, run work, copy data out of isolate
 * - Phase 3 [Isolate 1]: copy results from phase 2 into the original isolate
 *
 * This class handles the locking and thread synchronization for either synchronous or
 * asynchronous functions. That way the same code can be used for both versions of each function.
 *
 * When `async=true` a promise is return which will be resolved after all the work is done
 * When `async=false` this will run in the calling thread until completion
 */
class ThreePhaseTask {
	private:
		struct Phase2Runner : public Runnable {
			std::unique_ptr<ThreePhaseTask> self;
			std::shared_ptr<ShareableIsolate> first_isolate_ref;
			std::shared_ptr<ShareableIsolate> second_isolate_ref;
			std::unique_ptr<v8::Persistent<v8::Promise::Resolver>> promise_persistent;
			std::unique_ptr<v8::Persistent<v8::Context>> context_persistent;
			bool did_run = false;

			Phase2Runner(
				std::unique_ptr<ThreePhaseTask> self,
				std::shared_ptr<ShareableIsolate> first_isolate_ref,
				std::shared_ptr<ShareableIsolate> second_isolate_ref,
				std::unique_ptr<v8::Persistent<v8::Promise::Resolver>> promise_persistent,
				std::unique_ptr<v8::Persistent<v8::Context>> context_persistent
			);
			~Phase2Runner();
			void Run() final;
		};

	public:
		ThreePhaseTask() = default;
		ThreePhaseTask(const ThreePhaseTask&) = delete;
		ThreePhaseTask& operator= (const ThreePhaseTask&) = delete;
		virtual ~ThreePhaseTask() = default;

		virtual void Phase2() = 0;
		virtual v8::Local<v8::Value> Phase3() = 0;

		template <bool async, typename T, typename ...Args>
		static v8::Local<v8::Value> Run(ShareableIsolate& second_isolate, Args&&... args) {

			if (async) {
				// Build a promise for outer isolate
				ShareableIsolate& first_isolate = ShareableIsolate::GetCurrent();
				auto context_local = first_isolate->GetCurrentContext();
				auto promise_local = v8::Promise::Resolver::New(first_isolate);
				v8::TryCatch try_catch(first_isolate);
				try {
					// Schedule Phase2 async
					second_isolate.ScheduleTask(
						std::make_unique<Phase2Runner>(
							std::make_unique<T>(std::forward<Args>(args)...), // <-- Phase1 / ctor called here
							first_isolate.GetShared(),
							second_isolate.GetShared(),
							std::make_unique<v8::Persistent<v8::Promise::Resolver>>(first_isolate, promise_local),
							std::make_unique<v8::Persistent<v8::Context>>(first_isolate, context_local)
						), false, true
					);
				} catch (js_error_base& cc_error) {
					// An error was caught while running ctor (phase 1)
					assert(try_catch.HasCaught());
					Maybe<bool> ret = promise_local->Reject(context_local, try_catch.Exception());
					try_catch.Reset();
					Unmaybe(ret);
				}
				return promise_local->GetPromise();
			} else {
				// The sync case is a lot simpler, most of the work is done in second_isolate.Locker()
				std::unique_ptr<ThreePhaseTask> self = std::make_unique<T>(std::forward<Args>(args)...);
				if (&ShareableIsolate::GetCurrent() == &second_isolate) {
					// Shortcut when calling a sync method belonging to the currently entered isolate. This isn't an
					// optimization, it's used to bypass the deadlock prevention check in Locker()
					self->Phase2();
				} else {
					second_isolate.Locker([&self]() {
						self->Phase2();
						return 0;
					});
				}
				return self->Phase3();
			}
		}
};

} // namespace ivm
