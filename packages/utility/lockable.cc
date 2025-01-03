module;
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.utility:lockable;

namespace ivm::util {

template <class Type>
concept basic_lockable = requires(Type lock) {
	{ lock.lock() } -> std::same_as<void>;
	{ lock.unlock() } -> std::same_as<void>;
};

template <class Type>
concept shared_lockable =
	basic_lockable<Type> &&
	requires(Type lock) {
		{ lock.lock_shared() } -> std::same_as<void>;
		{ lock.unlock_shared() } -> std::same_as<void>;
	};

template <class Type>
concept notifiable = requires(Type cv) {
	{ cv.notify_one() } -> std::same_as<void>;
	{ cv.notify_all() } -> std::same_as<void>;
};

template <class Type, class Lock>
concept waitable = requires(Type cv, Lock lock) {
	{ cv.wait(lock) } -> std::same_as<void>;
};

// This is the return value of `read`, `write`, etc
template <class Resource, class Lock>
class locked : public Lock {
	private:
		using pointer = std::add_pointer_t<Resource>;
		using reference = Resource;

	public:
		explicit locked(reference resource, auto&&... lock_args) :
				Lock{std::forward<decltype(lock_args)>(lock_args)...},
				resource{&resource} {}

		auto operator*() -> reference { return *resource; }
		auto operator->() -> pointer { return resource; }

	private:
		pointer resource;
};

// Holds a lock and on destruction notifies the given condition variable
template <class Lock, notifiable Notifiable, std::predicate Predicate>
class lock_notify {
	public:
		lock_notify(Notifiable& cv, Predicate predicate, auto&&... lock_args) :
				predicate{std::move(predicate)},
				cv{&cv},
				lock{std::forward<decltype(lock_args)>(lock_args)...} {}

		lock_notify(const lock_notify&) = delete;
		lock_notify(lock_notify&& that) noexcept :
				cv{std::exchange(that.cv, nullptr)},
				lock{std::move(that.lock)} {};

		~lock_notify() {
			if (cv != nullptr && predicate()) {
				cv->notify_one();
			}
		}

		auto operator=(const lock_notify&) -> lock_notify& = delete;
		auto operator=(lock_notify&& that) noexcept -> lock_notify& {
			cv = std::exchange(that.cv, nullptr);
			lock = std::move(that.lock);
			return *this;
		}

	private:
		[[no_unique_address]] Predicate predicate;
		Notifiable* cv;
		Lock lock;
};

// Holds a lock with the option to wait on a condition variable
template <basic_lockable Lock, waitable<Lock> Waitable, std::predicate Predicate>
class lock_waitable {
	public:
		lock_waitable(Waitable& cv, Predicate predicate, auto&&... lock_args) :
				predicate{std::move(predicate)},
				cv{&cv},
				lock_{std::forward<decltype(lock_args)>(lock_args)...} {}

		auto lock() -> void {
			lock_.lock();
		}

		auto unlock() -> void {
			lock_.unlock();
		}

		auto wait() -> void {
			cv->wait(lock_, predicate);
		}

		auto wait(std::stop_token stop_token) -> bool
			requires std::same_as<Waitable, std::condition_variable_any> {
			if (!stop_token.stop_possible()) {
				throw std::logic_error{"stop_token is not stoppable"};
			}
			return cv->wait(lock_, stop_token, predicate) && !stop_token.stop_requested();
		}

	private:
		[[no_unique_address]] Predicate predicate;
		Waitable* cv;
		Lock lock_;
};

export template <class Resource, basic_lockable Mutex = std::mutex, class ConditionVariable = std::monostate>
class lockable {
	public:
		using mutex_type = Mutex;
		using reference = Resource&;
		using const_reference = const Resource&;

	private:
		using read_scope =
			std::conditional_t<shared_lockable<mutex_type>, std::shared_lock<mutex_type>, std::scoped_lock<mutex_type>>;
		using read_lock =
			std::conditional_t<shared_lockable<mutex_type>, std::shared_lock<mutex_type>, std::unique_lock<mutex_type>>;

		// Internal bound predicate type
		template <class Predicate>
		using predicate_type = decltype(std::bind(std::declval<Predicate>(), std::cref(std::declval<reference>())));

	public:
		using read_type = locked<const_reference, read_scope>;
		using write_type = locked<reference, std::scoped_lock<mutex_type>>;

		template <class Predicate>
		using read_waitable_type =
			locked<const_reference, lock_waitable<read_lock, ConditionVariable, predicate_type<Predicate>>>;
		template <class Predicate>
		using write_notify_type =
			locked<reference, lock_notify<std::unique_lock<mutex_type>, ConditionVariable, predicate_type<Predicate>>>;
		template <class Predicate>
		using write_waitable_type =
			locked<reference, lock_waitable<std::unique_lock<mutex_type>, ConditionVariable, predicate_type<Predicate>>>;

		explicit lockable(auto&&... args) :
				resource_{std::forward<decltype(args)>(args)...} {}

		auto read() -> read_type {
			return read_type{resource_, mutex_};
		}

		auto read_waitable(std::predicate<const_reference> auto predicate) -> read_waitable_type<decltype(predicate)>
			requires notifiable<ConditionVariable> {
			auto bound_predicate = std::bind(std::move(predicate), std::cref(resource_));
			return read_waitable_type<decltype(predicate)>{
				resource_, condition_variable_, std::move(bound_predicate), mutex_
			};
		}

		auto write() -> write_type {
			return write_type{resource_, mutex_};
		}

		auto write_notify()
			requires notifiable<ConditionVariable> {
			return write_notify([](auto&) { return true; });
		}

		auto write_notify(std::predicate<const_reference> auto predicate) -> write_notify_type<decltype(predicate)>
			requires notifiable<ConditionVariable> {
			auto bound_predicate = std::bind(std::move(predicate), std::cref(resource_));
			return write_notify_type<decltype(predicate)>{
				resource_, condition_variable_, std::move(bound_predicate), mutex_
			};
		}

		auto write_waitable(std::predicate<const_reference> auto predicate) -> write_waitable_type<decltype(predicate)>
			requires waitable<ConditionVariable, std::unique_lock<mutex_type>> {
			auto bound_predicate = std::bind(std::move(predicate), std::cref(resource_));
			return write_waitable_type<decltype(predicate)>{
				resource_, condition_variable_, std::move(bound_predicate), mutex_
			};
		}

	private:
		mutex_type mutex_;
		Resource resource_;
		[[no_unique_address]] ConditionVariable condition_variable_;
};

} // namespace ivm::util
