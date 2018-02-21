#pragma once
#include <v8.h>
#include "environment.h"
#include "holder.h"
#include <memory>
#include <tuple>
#include <utility>

namespace ivm {

/**
 * This holds a number of persistent handles to some values in a single isolate. It also holds a
 * handle to the isolate. When the destructor of this class is called it will run `Reset()` on each
 * handle in the context of the isolate. If the destructor of this class is called after the isolate
 * has been disposed then Reset() will not be called (but I don't think that causes a memory leak).
 */
template <typename ...Types>
class RemoteTuple {
	private:
		template <typename T>
		using HandleType = v8::Persistent<T, v8::NonCopyablePersistentTraits<T>>;

		// This uses unique_ptrs because the handles may outlive the RemoteTuple instance (we need to
		// transfer ownership to DisposalTask). It's a tuple of unique_ptrs and not the other way around
		// because there is no reasonable way to construct in place with tuple like there is with pair.
		using HandlesType = std::tuple<std::unique_ptr<HandleType<Types>>...>;

		// This calls Reset() on each handle.
		struct DisposalTask : public Runnable {
			HandlesType handles;
			DisposalTask(HandlesType&& handles) : handles(std::move(handles)) {}

			template <int> void Reset() {}

			template <int, typename T, typename ...Rest>
			void Reset() {
				std::get<sizeof...(Rest)>(handles)->Reset();
				Reset<0, Rest...>();
			}

			void Run() {
				Reset<0, Types...>();
			}
		};

		std::shared_ptr<IsolateHolder> isolate;
		HandlesType handles;

	public:
		RemoteTuple(v8::Local<Types>... handles) :
			isolate(IsolateEnvironment::GetCurrentHolder()),
			handles(std::make_unique<HandleType<Types>>(v8::Isolate::GetCurrent(), std::move(handles))...)
		{
			static_assert(!v8::NonCopyablePersistentTraits<v8::Value>::kResetInDestructor, "Do not reset in destructor");
		}

		~RemoteTuple() {
			isolate->ScheduleTask(std::make_unique<DisposalTask>(std::move(handles)), true, false);
		}

		RemoteTuple(const RemoteTuple&) = delete;
		RemoteTuple& operator= (const RemoteTuple&) = delete;

		template <size_t N>
		auto Deref() const {
			return v8::Local<typename std::tuple_element<N, std::tuple<Types...>>::type>::New(v8::Isolate::GetCurrent(), *std::get<N>(handles));
		}

		IsolateHolder* GetIsolateHolder() {
			return isolate.get();
		}
};

/**
 * Convenient when you only need 1 handle
 */
template <typename T>
class RemoteHandle {
	private:
		RemoteTuple<T> handle;

	public:
		RemoteHandle(v8::Local<T> handle) : handle(handle) {}

		auto Deref() const {
			return handle.template Deref<0>();
		}
};

} // namespace ivm
