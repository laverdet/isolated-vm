module;
#include <boost/intrusive/list.hpp>
#include <concepts>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>
export module ivm.isolated_v8:scheduler;
import ivm.utility;

namespace ivm {

export class scheduler : non_moveable {
	private:
		using handle_list_mode = boost::intrusive::link_mode<boost::intrusive::auto_unlink>;
		using handle_list_hook = boost::intrusive::list_member_hook<handle_list_mode>;

	public:
		class handle : non_moveable {
			public:
				friend scheduler;

				explicit handle(scheduler& scheduler);
				~handle();

			private:
				scheduler& scheduler_;
				handle_list_hook scheduler_hook;
				mutable lockable<std::jthread> thread;
		};

		~scheduler();

		auto run(auto fn, handle& handle, auto&&... args) -> void
			requires std::invocable<decltype(fn), const std::stop_token&, decltype(args)...>;

	private:
		using handle_list_csize = boost::intrusive::constant_time_size<false>;
		using handle_list_member = boost::intrusive::member_hook<handle, handle_list_hook, &handle::scheduler_hook>;
		using handle_list = boost::intrusive::list<handle, handle_list_csize, handle_list_member>;

		lockable<handle_list, std::mutex, std::condition_variable> handles;
};

auto scheduler::run(auto fn, handle& handle, auto&&... args) -> void
	requires std::invocable<decltype(fn), const std::stop_token&, decltype(args)...> {
	auto thread_lock = handle.thread.write();
	if (thread_lock->joinable()) {
		throw std::logic_error("Thread is already running");
	}
	*thread_lock = std::jthread{
		[](const std::stop_token& stop_token, auto fn, auto&&... args) {
			fn(stop_token, std::forward<decltype(args)>(args)...);
		},
		std::move(fn),
		std::forward<decltype(args)>(args)...
	};
	thread_lock->detach();
}

} // namespace ivm
