#pragma once
#include <v8.h>
#include "environment.h"
#include "holder.h"
#include <memory>
#include <tuple>
#include <utility>

namespace ivm {

template <size_t Index, class Type>
struct RemoteTupleElement {
	RemoteTupleElement(v8::Isolate* isolate, v8::Local<Type> local) : persistent{isolate, local} {}
	v8::Persistent<Type, v8::NonCopyablePersistentTraits<Type>> persistent;
};

/**
 * This holds a number of persistent handles to some values in a single isolate. It also holds a
 * handle to the isolate. When the destructor of this class is called it will run `Reset()` on each
 * handle in the context of the isolate. If the destructor of this class is called after the isolate
 * has been disposed then Reset() will not be called (but I don't think that causes a memory leak).
 */
template <class ...Types>
class RemoteTuple {
	private:
		/**
		 * This is basically a tuple that can be constructed in place in a way that v8::Persistent<> will
		 * accept.
		 */
		template <class Indices>
		struct Holder;

		template <size_t ...Indices>
		struct Holder<std::index_sequence<Indices...>> : RemoteTupleElement<Indices, Types>... {
			template <class... Args>
			Holder(v8::Isolate* isolate, v8::Local<Types>... locals) :
					RemoteTupleElement<Indices, Types>{isolate, locals}... {
			}

			template <size_t Index>
			auto& get() {
				return RemoteTupleElement<Index, std::tuple_element_t<Index, std::tuple<Types...>>>::persistent;
			}
		};

		// This uses a unique_ptr because the handles may outlive the RemoteTuple instance (we need to
		// transfer ownership to DisposalTask).
		using TupleType = Holder<std::make_index_sequence<sizeof...(Types)>>;
		using HandlesType = std::unique_ptr<TupleType>;

		// This calls Reset() on each handle.
		struct DisposalTask : public Runnable {
			HandlesType handles;
			explicit DisposalTask(HandlesType&& handles) : handles{std::move(handles)} {}

			template <int> void Reset() {}

			template <int, typename T, typename ...Rest>
			void Reset() {
				handles->template get<sizeof...(Rest)>().Reset();
				Reset<0, Rest...>();
			}

			void Run() final {
				Reset<0, Types...>();
				IsolateEnvironment::GetCurrent()->remotes_count.fetch_sub(sizeof...(Types));
			}
		};

		std::shared_ptr<IsolateHolder> isolate;
		HandlesType handles;

	public:
		explicit RemoteTuple(v8::Local<Types>... handles) :
			isolate{IsolateEnvironment::GetCurrentHolder()},
			handles{std::make_unique<TupleType>(v8::Isolate::GetCurrent(), handles...)}
		{
			IsolateEnvironment::GetCurrent()->remotes_count.fetch_add(sizeof...(Types));
			static_assert(!v8::NonCopyablePersistentTraits<v8::Value>::kResetInDestructor, "Do not reset in destructor");
		}

		~RemoteTuple() {
			isolate->ScheduleTask(std::make_unique<DisposalTask>(std::move(handles)), true, false, true);
		}

		RemoteTuple(const RemoteTuple&) = delete;
		RemoteTuple& operator= (const RemoteTuple&) = delete;

		template <size_t N>
		auto Deref() const {
			return v8::Local<std::tuple_element_t<N, std::tuple<Types...>>>::New(v8::Isolate::GetCurrent(), handles->template get<N>());
		}

		IsolateHolder* GetIsolateHolder() {
			return isolate.get();
		}

		std::shared_ptr<IsolateHolder> GetSharedIsolateHolder() {
			return isolate;
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
		explicit RemoteHandle(v8::Local<T> handle) : handle(handle) {}

		auto Deref() const {
			return handle.template Deref<0>();
		}

		IsolateHolder* GetIsolateHolder() {
			return handle.GetIsolateHolder();
		}

		std::shared_ptr<IsolateHolder> GetSharedIsolateHolder() {
			return handle.GetSharedIsolateHolder();
		}
};

} // namespace ivm
