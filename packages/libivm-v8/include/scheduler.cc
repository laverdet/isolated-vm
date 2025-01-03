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

export class scheduler : util::non_moveable {
	private:
		using handle_list_mode = boost::intrusive::link_mode<boost::intrusive::auto_unlink>;
		using handle_list_hook = boost::intrusive::list_member_hook<handle_list_mode>;

	public:
		class handle : util::non_moveable {
			public:
				friend scheduler;

				explicit handle(scheduler& scheduler);
				~handle();

			private:
				scheduler& scheduler_;
				handle_list_hook scheduler_hook_;
				mutable util::lockable<std::stop_source> stop_source_;
		};

		~scheduler();

		auto run(auto fn, handle& handle, auto&&... args) -> void
			requires std::invocable<decltype(fn), std::stop_token, decltype(args)...>;

	private:
		using handle_list_csize = boost::intrusive::constant_time_size<false>;
		using handle_list_member = boost::intrusive::member_hook<handle, handle_list_hook, &handle::scheduler_hook_>;
		using handle_list = boost::intrusive::list<handle, handle_list_csize, handle_list_member>;

		util::lockable<handle_list, std::mutex, std::condition_variable> handles;
};

auto scheduler::run(auto fn, handle& handle, auto&&... args) -> void
	requires std::invocable<decltype(fn), std::stop_token, decltype(args)...> {
	auto stop_source_lock = handle.stop_source_.write();
	if (stop_source_lock->stop_possible()) {
		throw std::logic_error("Thread is already running");
	}
	std::jthread thread{
		[](std::stop_token stop_token, auto fn, auto&&... args) {
			fn(std::move(stop_token), std::forward<decltype(args)>(args)...);
		},
		std::move(fn),
		std::forward<decltype(args)>(args)...
	};
	*stop_source_lock = thread.get_stop_source();
	thread.detach();
}

} // namespace ivm
