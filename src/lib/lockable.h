#include <mutex>
#include <shared_mutex>

namespace ivm {
namespace detail {

template <class Type, class Mutex, class Lock>
class lock_t {
	public:
		lock_t(Type& resource, Mutex& mutex) : resource{resource}, lock{mutex} {}

		auto& operator*() { return resource; }
		auto& operator*() const { return resource; }
		auto operator->() { return &resource; }
		auto operator->() const { return &resource; }

	private:
		Type& resource;
		Lock lock;
};

} // namespace detail

template <class Type>
class lockable_t {
	private:
#if __cpp_lib_shared_mutex
		using mutex_t = std::shared_mutex;
		using shared_lock_t = std::shared_lock<mutex_t>;
#elif __cpp_lib_shared_timed_mutex
		using mutex_t = std::shared_timed_mutex;
		using shared_lock_t = std::shared_lock<mutex_t>;
#else
		using mutex_t = std::mutex;
		using shared_lock_t = std::unique_lock<mutex_t>;
#endif
		using exclusive_lock_t = std::unique_lock<mutex_t>;

	public:
		lockable_t() : resource{} {}
		template <class... Args>
		explicit lockable_t(Args&&... args) : resource{std::forward<Args>(args)...} {}

		// C++17 adds mandatory return value optimization which would enable use of std::lock_guard /
		// std::scoped_lock here.
		auto read() const { return detail::lock_t<const Type, mutex_t, exclusive_lock_t>{resource, mutex}; }
		auto write() { return detail::lock_t<Type, mutex_t, shared_lock_t>{resource, mutex}; }

	private:
		Type resource;
		mutable mutex_t mutex;
};

} // namespace ivm
