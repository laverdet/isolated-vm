#pragma once
#include <condition_variable>
#include <mutex>
#include <shared_mutex>

namespace ivm {
namespace detail {

// Holds the lock and provides pointer semantics
template <class Type, class Mutex, class Lock>
class lock_t {
	template <class> friend class condition_variable_impl_t;
	public:
		lock_t(Type& resource, Mutex& mutex) : resource{resource}, lock{mutex} {}

		auto operator*() -> auto& { return resource; }
		auto operator*() const -> auto& { return resource; }
		auto operator->() { return &resource; }
		auto operator->() const { return &resource; }

	private:
		Type& resource;
		Lock lock;
};

// Detects which condition variable is needed
template <class ConditionVariable>
class condition_variable_impl_t : public ConditionVariable {
	public:
		template <class Type, class Mutex, class Lock, class... Args>
		auto wait(lock_t<Type, Mutex, Lock>& lock, Args&&... args) {
			return ConditionVariable::wait(lock.lock, std::forward<Args>(args)...);
		}
		using impl = condition_variable_impl_t;
};

template <class Mutex>
struct condition_variable_t : condition_variable_impl_t<std::condition_variable_any> {};

template <>
struct condition_variable_t<std::mutex> : condition_variable_impl_t<std::condition_variable> {};

// Holds resource and mutex
template <class Type, class Traits>
class lockable_impl_t {
	private:
		using Mutex = typename Traits::Mutex;
		using Read = typename Traits::Read;
		using Write = typename Traits::Write;

	public:
		lockable_impl_t() = default;
		template <class... Args>
		explicit lockable_impl_t(Args&&... args) : resource{std::forward<Args>(args)...} {}
		lockable_impl_t(const lockable_impl_t&) = delete;
		~lockable_impl_t() = default;
		auto operator=(const lockable_impl_t&) = delete;

		auto read() const { return detail::lock_t<const Type, Mutex, Read>{resource, mutex}; }
		auto write() { return detail::lock_t<Type, Mutex, Write>{resource, mutex}; }

		using condition_variable_t = detail::condition_variable_t<Mutex>;

	private:
		Type resource{};
		mutable Mutex mutex;
};

// C++17 adds mandatory return value optimization which enables this
#if __cplusplus > 201700 && __cpp_lib_scoped_lock
	template <class Mutex>
	using scoped_lock = std::scoped_lock<Mutex>;
#else
	template <class Mutex>
	using scoped_lock = std::unique_lock<Mutex>;
#endif

// Detect best capable mutex
template <bool Shared>
struct mutex_traits_t;

template <>
struct mutex_traits_t<false> {
	using Mutex = std::mutex;
};

template <>
struct mutex_traits_t<true> {
#if __cpp_lib_shared_mutex
	using Mutex = std::shared_mutex;
	static constexpr bool shared = true;
#elif __cpp_lib_shared_timed_mutex
	using Mutex = std::shared_timed_mutex;
	static constexpr bool shared = true;
#else
	using Mutex = std::mutex;
	static constexpr bool shared = false;
#endif
};

// Detect best capable lock
template <class MutexTraits, bool Waitable = false>
struct lock_traits_t;

template <class MutexTraits>
struct lock_traits_t<MutexTraits, false> {
	using Read = scoped_lock<typename MutexTraits::Mutex>;
	using Write = scoped_lock<typename MutexTraits::Mutex>;
};

template <class MutexTraits>
struct lock_traits_t<MutexTraits, true> {
	template <class Mutex> using Read = typename std::conditional<MutexTraits::shared,
		std::shared_lock<typename MutexTraits::Mutex>,
		std::unique_lock<typename MutexTraits::Mutex>>::type;
	template <class Mutex> using Write = std::unique_lock<typename MutexTraits::Mutexutex>;
};

// Combines mutex_traits_t and lockable_traits_t
template <bool Shared, bool Waitable>
struct lockable_traits_t : mutex_traits_t<Shared>, lock_traits_t<mutex_traits_t<Shared>, Waitable> {};

} // namespace detail

template <class Type, bool Shared = false, bool Waitable = false>
using lockable_t = detail::lockable_impl_t<Type, detail::lockable_traits_t<Shared, Waitable>>;

} // namespace ivm
