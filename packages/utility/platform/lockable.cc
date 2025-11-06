module;
#include <cassert>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <stop_token>
#include <utility>
#include <variant>
export module ivm.utility:lockable;
import :type_traits;
import :utility;

namespace util {

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

// Predicate wrapper, binding to resource reference
template <class Resource, std::predicate<const Resource&> Predicate>
class resource_predicate {
	public:
		resource_predicate(const Resource& resource, Predicate predicate) :
				resource_{resource},
				predicate_{std::move(predicate)} {}

		auto operator()() const -> bool {
			// nb: `std::invoke` used so member function predicates work
			return std::invoke(predicate_, resource_.get());
		}

	private:
		std::reference_wrapper<const Resource> resource_;
		[[no_unique_address]] Predicate predicate_;
};

// This is the return value of `read`, `write`, etc
template <class Resource, class Lock>
class locked : public Lock, public pointer_facade {
	public:
		explicit locked(Resource& resource, auto&&... lock_args)
			requires std::constructible_from<Lock, decltype(lock_args)...> :
				Lock{std::forward<decltype(lock_args)>(lock_args)...},
				resource_{resource} {}

		auto operator*() const -> Resource& { return resource_.get(); }

	private:
		std::reference_wrapper<Resource> resource_;
};

// Holds a lock and on destruction notifies the given condition variable
template <std::predicate Predicate, class Lock, notifiable Notifiable>
class lock_notify {
	public:
		lock_notify(Predicate predicate, Notifiable& cv, auto&&... lock_args)
			requires std::constructible_from<Lock, decltype(lock_args)...> :
				predicate_{std::move(predicate)},
				cv_{&cv},
				lock_{std::forward<decltype(lock_args)>(lock_args)...} {}

		lock_notify(const lock_notify&) = delete;
		lock_notify(lock_notify&& that) noexcept :
				cv_{std::exchange(that.cv_, nullptr)},
				lock_{std::move(that.lock_)} {};

		~lock_notify() {
			if (cv_ != nullptr && predicate_()) {
				cv_->notify_one();
			}
		}

		auto operator=(const lock_notify&) = delete;
		auto operator=(lock_notify&& that) = delete;

	private:
		Predicate predicate_;
		Notifiable* cv_;
		Lock lock_;
};

// Holds a lock with the option to wait on a condition variable
template <std::predicate Predicate, basic_lockable Lock, waitable<Lock> Waitable>
class lock_waitable : public Lock {
	public:
		lock_waitable(Predicate predicate, Waitable& cv, auto&&... lock_args)
			requires std::constructible_from<Lock, decltype(lock_args)...> :
				Lock{std::forward<decltype(lock_args)>(lock_args)...},
				predicate_{std::move(predicate)},
				cv_{&cv} {}

		auto notify_all() -> void { cv_->notify_one(); }
		auto notify_one() -> void { cv_->notify_one(); }

		auto wait() -> void {
			cv_->wait(*this, predicate_);
		}

		template <class Rep, class Period>
		auto wait(std::chrono::duration<Rep, Period> duration) -> bool {
			return cv_->wait_for(*this, duration, predicate_);
		}

		template <class Rep, class Period>
		auto wait(std::chrono::time_point<Rep, Period> time_point) -> bool {
			return cv_->wait_until(*this, time_point, predicate_);
		}

		auto wait(std::stop_token stop_token) -> bool
			requires(type<Waitable> == type<std::condition_variable_any>) {
			assert(stop_token.stop_possible());
			return cv_->wait(*this, stop_token, predicate_);
		}

		template <class Rep, class Period>
		auto wait(std::stop_token stop_token, std::chrono::duration<Rep, Period> duration) -> bool
			requires(type<Waitable> == type<std::condition_variable_any>) {
			return cv_->wait_for(*this, stop_token, duration, predicate_);
		}

		template <class Rep, class Period>
		auto wait(std::stop_token stop_token, std::chrono::time_point<Rep, Period> time_point) -> bool
			requires(type<Waitable> == type<std::condition_variable_any>) {
			return cv_->wait_until(*this, stop_token, time_point, predicate_);
		}

	private:
		Predicate predicate_;
		Waitable* cv_;
};

// Lockable resource with configurable mutex and condition variable types
export template <class Resource, basic_lockable Mutex = std::mutex, class ConditionVariable = std::monostate>
class lockable {
	public:
		using mutex_type = Mutex;
		using resource_type = Resource;

	private:
		using scoped_read_lock =
			std::conditional_t<shared_lockable<mutex_type>, std::shared_lock<mutex_type>, std::scoped_lock<mutex_type>>;
		using unique_read_lock =
			std::conditional_t<shared_lockable<mutex_type>, std::shared_lock<mutex_type>, std::unique_lock<mutex_type>>;

		using scoped_write_lock = std::scoped_lock<mutex_type>;
		using unique_write_lock = std::unique_lock<mutex_type>;

		template <class Lock, class Predicate>
		using notify_type = lock_notify<resource_predicate<resource_type, Predicate>, Lock, ConditionVariable>;

		template <class Lock, class Predicate>
		using waitable_type = lock_waitable<resource_predicate<resource_type, Predicate>, Lock, ConditionVariable>;

	public:
		explicit lockable(auto&&... args) :
				resource_{std::forward<decltype(args)>(args)...} {}

		// `read`
		using read_type = locked<const resource_type, scoped_read_lock>;

		auto read() -> read_type {
			return read_type{resource_, mutex_};
		}

		// `read_waitable`
		template <class Predicate>
		using read_waitable_type = locked<const resource_type, waitable_type<unique_read_lock, Predicate>>;

		template <std::predicate<const resource_type&> Predicate>
		auto read_waitable(Predicate predicate) -> read_waitable_type<Predicate> {
			return read_waitable_type<Predicate>{resource_, resource_predicate{resource_, std::move(predicate)}, condition_variable_, mutex_};
		}

		// `write`
		using write_type = locked<resource_type, scoped_write_lock>;

		auto write() -> write_type {
			return write_type{resource_, mutex_};
		}

		// `write_notify`
		template <class Predicate>
		using write_notify_type = locked<resource_type, notify_type<unique_write_lock, Predicate>>;

		auto write_notify() {
			return write_notify([](const auto&) -> bool { return true; });
		}

		template <std::predicate<const resource_type&> Predicate>
		auto write_notify(Predicate predicate) -> write_notify_type<Predicate> {
			return write_notify_type<Predicate>{resource_, resource_predicate{resource_, std::move(predicate)}, condition_variable_, mutex_};
		}

		// `write_waitable`
		template <class Predicate>
		using write_waitable_type = locked<resource_type, waitable_type<unique_write_lock, Predicate>>;

		template <std::predicate<const resource_type&> Predicate>
		auto write_waitable(Predicate predicate) -> write_waitable_type<Predicate> {
			return write_waitable_type<Predicate>{resource_, resource_predicate{resource_, std::move(predicate)}, condition_variable_, mutex_};
		}

	private:
		mutex_type mutex_;
		Resource resource_;
		[[no_unique_address]] ConditionVariable condition_variable_;
};

// Preset lockable types
struct lockable_traits {
		bool interuptable{};
		bool notifiable{};
		bool shared{};
};

export template <class Resource, lockable_traits Traits = {}>
using lockable_with = type_t<[]() consteval {
	using mutex_type = type_t<[]() {
		if constexpr (Traits.shared) {
			return type<std::shared_mutex>;
		} else {
			return type<std::mutex>;
		}
	}()>;
	using cv_type = type_t<[]() {
		if constexpr (Traits.interuptable) {
			return type<std::condition_variable_any>;
		} else if constexpr (Traits.notifiable) {
			return type<std::condition_variable>;
		} else {
			return type<std::monostate>;
		}
	}()>;
	return type<lockable<Resource, mutex_type, cv_type>>;
}()>;

} // namespace util
