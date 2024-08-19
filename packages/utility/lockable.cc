module;
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.utility:lockable;

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
	public:
		explicit locked(Resource& resource, auto&&... lock_args) :
				Lock{std::forward<decltype(lock_args)>(lock_args)...},
				resource{&resource} {}

		auto operator*() -> Resource& { return *resource; }
		auto operator->() -> Resource* { return resource; }

	private:
		Resource* resource;
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

		auto wait(std::stop_token stop_token) -> bool {
			return cv->wait(lock_, stop_token, predicate);
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

		explicit lockable(auto&&... args) :
				resource{std::forward<decltype(args)>(args)...} {}

		auto read() {
			using lock_type = std::conditional_t<shared_lockable<Mutex>, std::shared_lock<Mutex>, std::scoped_lock<Mutex>>;
			using readable = locked<const Resource, lock_type>;
			return readable{resource, mutex};
		}

		auto read_waitable(std::predicate<const Resource&> auto predicate)
			requires notifiable<ConditionVariable> {
			auto bound_predicate = std::bind(std::move(predicate), std::cref(resource));
			using lock_type = std::conditional_t<shared_lockable<Mutex>, std::shared_lock<Mutex>, std::unique_lock<Mutex>>;
			using readable = locked<const Resource, lock_waitable<lock_type, ConditionVariable, decltype(bound_predicate)>>;
			return readable{resource, condition_variable, std::move(bound_predicate), mutex};
		}

		auto write() {
			using writable = locked<Resource, std::scoped_lock<Mutex>>;
			return writable{resource, mutex};
		}

		auto write_notify()
			requires notifiable<ConditionVariable> {
			return write_notify([](auto&) { return true; });
		}

		auto write_notify(std::predicate<const Resource&> auto predicate)
			requires notifiable<ConditionVariable> {
			auto bound_predicate = std::bind(std::move(predicate), std::cref(resource));
			using writable = locked<Resource, lock_notify<std::scoped_lock<Mutex>, ConditionVariable, decltype(bound_predicate)>>;
			return writable{resource, condition_variable, std::move(bound_predicate), mutex};
		}

		auto write_waitable(std::predicate<const Resource&> auto predicate)
			requires waitable<ConditionVariable, std::unique_lock<Mutex>> {
			auto bound_predicate = std::bind(std::move(predicate), std::cref(resource));
			using writable = locked<Resource, lock_waitable<std::unique_lock<Mutex>, ConditionVariable, decltype(bound_predicate)>>;
			return writable{resource, condition_variable, std::move(bound_predicate), mutex};
		}

	private:
		Mutex mutex;
		Resource resource;
		[[no_unique_address]] ConditionVariable condition_variable;
};
