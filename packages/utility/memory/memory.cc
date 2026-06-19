export module util:memory;
export import :memory.autorelease_pool;
export import :memory.comparator;
export import :memory.noinit_allocator;
import :platform.lockable;
import std;

namespace util {

// Deleter object for base class which does not have a virtual destructor
export template <class Type>
class derived_delete {
	public:
		template <std::derived_from<Type> As>
		explicit constexpr derived_delete(std::type_identity<As> /*type*/) noexcept :
				delete_{[](Type* pointer) noexcept -> void {
					delete static_cast<As*>(pointer);
				}} {}

		constexpr auto operator()(Type* value) const noexcept {
			delete_(value);
		}

	private:
		using function_type = auto(Type*) noexcept -> void;
		function_type* delete_;
};

export template <class Type, class Deleter>
auto move_pointer_operation(
	std::unique_ptr<Type, Deleter> unique,
	std::invocable<Type*, std::unique_ptr<Type, Deleter>> auto function
) -> decltype(auto) {
	auto* ptr = unique.get();
	return std::invoke(std::move(function), ptr, std::move(unique));
}

export template <class Type>
auto move_pointer_operation(
	std::shared_ptr<Type> shared,
	std::invocable<Type*, std::shared_ptr<Type>> auto function
) -> decltype(auto) {
	auto* ptr = shared.get();
	return std::invoke(std::move(function), ptr, std::move(shared));
}

export template <class To, std::derived_from<To> Type, class Deleter>
auto safe_pointer_upcast(std::unique_ptr<Type, Deleter> unique) {
	struct cast_deleter : Deleter {
			cast_deleter() = default;
			// NOLINTNEXTLINE(google-explicit-constructor)
			cast_deleter(Deleter deleter) noexcept : Deleter{std::move(deleter)} {}
			using Deleter::operator();
			constexpr auto operator()(To* ptr) const noexcept {
				(*this)(static_cast<Type*>(ptr));
			}
	};
	return std::unique_ptr<To, cast_deleter>{std::move(unique)};
}

// `std::atomic<std::shared_ptr<T>>` replacement when not available (macOS)
#if __cpp_lib_atomic_shared_ptr

export template <class Type>
using atomic_shared_ptr = std::atomic<std::shared_ptr<Type>>;

#else

export template <class Type>
class atomic_shared_ptr {
	public:
		auto load(auto /*order*/) const -> std::shared_ptr<Type> { return pointer_.read(); }
		auto store(auto value, auto /*order*/) -> void { *pointer_.write() = std::move(value); }

	private:
		util::lockable<std::shared_ptr<Type>> pointer_;
};

#endif

} // namespace util
