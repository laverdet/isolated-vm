module;
#include <boost/intrusive/list.hpp>
#include <algorithm>
#include <cassert>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <latch>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
export module isolated_v8:scheduler;
import ivm.utility;

namespace scheduler {

using intrusive_no_size = boost::intrusive::constant_time_size<false>;
using intrusive_normal_mode = boost::intrusive::link_mode<boost::intrusive::normal_link>;
using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_normal_mode>;

// Intrusive member which belongs to a container.
template <class Type>
class member : util::non_moveable {
	private:
		friend Type;
		member() = default;

	public:
		auto self() -> Type& { return *static_cast<Type*>(this); }
		auto cself() const -> const Type& { return *static_cast<const Type*>(this); }

	private:
		intrusive_list_hook hook_;

	public:
		using hook_type = boost::intrusive::member_hook<member<Type>, intrusive_list_hook, &member<Type>::hook_>;
};

// Lockable intrusive container. Intrusive is used so that children can unlink themselves without
// searching the list for their own node.
template <class Type>
class container : util::non_moveable {
	public:
		using lockable_type = util::lockable<container, std::mutex, std::condition_variable>;
		using list_type = boost::intrusive::list<member<Type>, intrusive_no_size, typename member<Type>::hook_type>;

		~container() { assert(members_.empty()); }

		auto erase(Type& member) -> void {
			members_.erase(list_type::s_iterator_to(member));
		}

		auto insert(Type& member) -> void {
			assert(is_open());
			members_.push_back(member);
		}

		auto request_stop() {
			closed_ = true;
			for (auto& member : members_) {
				member.self().request_stop();
			}
		}

		[[nodiscard]] auto empty() const -> bool { return members_.empty(); }
		[[nodiscard]] auto is_open() const -> bool { return !closed_; }
		[[nodiscard]] auto members() -> list_type& { return members_; }

		static auto close(lockable_type& self) -> void {
			auto lock = self.write_waitable(&container::empty);
			lock->request_stop();
			lock.wait();
		}

		static auto write_notify(lockable_type& self) {
			return self.write_notify([](const auto& container) {
				return container.members_.size() <= 1;
			});
		}

	private:
		bool closed_{};
		list_type members_;
};

// Automatically links a member to a container when constructed and unlinks when destructed.
// `auto_unlink` can't be used since the lock is needed.
template <class Type>
class link : util::non_moveable {
	private:
		friend Type;

		// I already have a write lock
		link(auto&& lock, container<Type>::lockable_type& container) :
				container_{&container} {
			lock->insert(self());
		}

		// I don't already have a lock
		explicit link(container<Type>::lockable_type& container) :
				link{container.write(), container} {}

		~link() {
			container<Type>::write_notify(*container_)->erase(self());
		}

	public:
		// Doesn't play well with `modernize-use-equals-delete`
		// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
		link(const link&) = delete;
		auto operator=(const link&) -> link& = delete;

	private:
		auto self() -> Type& { return *static_cast<Type*>(this); }
		container<Type>::lockable_type* container_;
};

// Leaf node of the scheduler graph. Holds a running thread.
class thread : public member<thread>, public link<thread> {
	public:
		thread(
			container<thread>::lockable_type::write_type& lock,
			container<thread>::lockable_type& container
		) :
				link{lock, container},
				stop_source_{std::nostopstate} {}

		auto request_stop() -> void {
			assert(stop_source_.read()->stop_possible());
			stop_source_.write()->request_stop();
		}

		[[nodiscard]] auto thread_id() const -> std::thread::id { return thread_id_.load(std::memory_order_relaxed); }
		static auto launch(std::unique_ptr<thread> self, auto fn, auto&&... args) -> void;

	private:
		std::atomic<std::thread::id> thread_id_;
		std::latch launch_latch_{1};
		util::lockable<std::stop_source> stop_source_;
};

// Specify whether or not the layer expects a parent
export struct layer_traits {
		bool root = false;
};

export template <layer_traits Traits>
class layer;

export template <layer_traits Traits>
class runner;

export class handle;

struct layer_base : util::non_moveable {
	protected:
		~layer_base() = default;

	public:
		virtual auto request_stop() -> void = 0;
};

class layer_connected
		: public layer_base,
			public member<layer_connected>,
			public link<layer_connected> {
	protected:
		~layer_connected() = default;

	public:
		explicit layer_connected(container<layer_connected>::lockable_type& container) :
				link{container} {}
};

// Manages a graph of scheduler layers & runners. On destruction all children are closed before
// continuing.
template <layer_traits Traits = {}>
class layer final : public std::conditional_t<Traits.root, layer_base, layer_connected> {
	public:
		template <layer_traits> friend class layer;
		template <layer_traits> friend class runner;
		layer() = default;

		template <layer_traits ParentTraits>
		explicit layer(layer<ParentTraits>& parent) :
				layer_connected{parent.container_} {}

		~layer() { container<layer_connected>::close(container_); }

		layer(const layer&) = delete;
		auto operator=(const layer&) -> layer& = delete;

		auto request_stop() -> void final { container_.write()->request_stop(); }

	private:
		container<layer_connected>::lockable_type container_;
};

// Dispatches and owns threads.
template <layer_traits Traits = {}>
class runner final : public layer_connected {
		static_assert(!Traits.root, "not implemented");

	public:
		friend handle;

		template <layer_traits ParentTraits>
		explicit runner(layer<ParentTraits>& parent) :
				layer_connected{parent.container_},
				container_{std::make_shared<container<thread>::lockable_type>()} {}

		~runner() { container<thread>::close(*container_); }

		runner(const runner&) = delete;
		auto operator=(const runner&) -> runner& = delete;

		auto operator()(auto fn, auto&&... args) -> void
			requires std::invocable<decltype(fn), std::stop_token, decltype(args)...> {
			auto lock = container_->write();
			auto instance = std::make_unique<thread>(lock, *container_);
			instance->launch(std::move(instance), std::move(fn), std::forward<decltype(args)>(args)...);
		}

		// Waits for all thread to drain, except for the current thread, if it is owned by this runner.
		auto close_threads() -> void {
			auto lock = container_->write_waitable([](const auto& container) -> bool {
				auto thread_id = std::this_thread::get_id();
				// The const version of the container can't iterate and I'm already fed up with my
				// implementation
				// NOLINT(cppcoreguidelines-pro-type-const-cast)
				auto& rwcontainer = const_cast<scheduler::container<thread>&>(container);
				return std::ranges::all_of(rwcontainer.members(), [ & ](const auto& thread) -> bool {
					return thread.cself().thread_id() == thread_id;
				});
			});
			lock->request_stop();
			lock.wait();
		}
		auto request_stop() -> void final { container_->write()->request_stop(); }

	private:
		std::shared_ptr<container<thread>::lockable_type> container_;
};

// Shareable handle to underlying runners. Can schedule jobs as long as the handle is alive.
class handle {
	public:
		template <layer_traits Traits>
		explicit handle(runner<Traits>& runner) :
				container_{runner.container_} {}

		auto operator()(auto fn, auto&&... args) -> void
			requires std::invocable<decltype(fn), std::stop_token, decltype(args)...>;

	private:
		std::shared_ptr<container<thread>::lockable_type> container_;
};

// ---

auto thread::launch(std::unique_ptr<thread> self, auto fn, auto&&... args) -> void {
	assert(!self->stop_source_.read()->stop_possible());
	auto* self_ptr = self.get();
	std::jthread thread_{
		[](
			std::stop_token stop_token,
			std::unique_ptr<thread> self,
			auto fn,
			auto&&... args
		) {
			auto run = [ & ]() {
				return std::invoke(std::move(fn), std::move(stop_token), std::forward<decltype(args)>(args)...);
			};
			auto run_with_releasable = [ & ]() {
				if constexpr (std::is_void_v<std::invoke_result_t<decltype(run)>>) {
					run();
					return std::tuple{};
				} else {
					return run();
				}
			};
			self->thread_id_.store(std::this_thread::get_id(), std::memory_order_relaxed);
			auto releasable = run_with_releasable();
			self->launch_latch_.wait();
			self.reset();
			(void)releasable;
		},
		std::move(self),
		std::move(fn),
		std::forward<decltype(args)>(args)...
	};
	self_ptr->thread_id_.store(thread_.get_id(), std::memory_order_relaxed);
	*self_ptr->stop_source_.write() = thread_.get_stop_source();
	self_ptr->launch_latch_.count_down();
	thread_.detach();
}

auto handle::operator()(auto fn, auto&&... args) -> void
	requires std::invocable<decltype(fn), std::stop_token, decltype(args)...> {
	auto instance = std::invoke([ & ]() -> std::unique_ptr<thread> {
		auto lock = container_->write();
		if (lock->is_open()) {
			return std::make_unique<thread>(lock, *container_);
		} else {
			return {};
		}
	});
	if (instance) {
		thread::launch(std::move(instance), std::move(fn), std::forward<decltype(args)>(args)...);
	}
}

} // namespace scheduler
