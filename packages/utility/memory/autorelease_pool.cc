module;
#include <cassert>
#include <flat_set>
#include <memory>
#include <utility>
export module ivm.utility:memory.autorelease_pool;
import :memory.comparator;
import :utility;

namespace util {

// v8 may not invoke destructors on `External` objects, but we still want to give it owning
// references to resources. This provides an allocator to manage those resources. If an
// `autorelease_pool` goes out of scope then all objects allocated/created will be appropriately
// destroyed and deallocated.
//
// - Not thread safe!
// - Destroying a collected resourced after the group goes out of scope is UB.
// - Does not work with non-object types.
// - Does not work with arrays.
// - Does not work with anything that uses placement-new (`std::shared_ptr`, `std::optional`,
//   `std::vector`, actually probably a lot of things)

template <class Type, class Alloc>
class resource;

// Base class of an allocated resource which knows how to destroy itself at any point in the lifecycle.
template <class Alloc>
class releasable {
	protected:
		using dtor_type = auto (releasable::*)(Alloc&) -> void;

	private:
		using allocator_traits = std::allocator_traits<Alloc>;

		// Deleter for `std::unique_ptr` / `unique_pointer_type`
		class deleter : private Alloc {
			public:
				constexpr explicit deleter(const Alloc& alloc) :
						Alloc(alloc) {}

				constexpr auto operator()(releasable* pointer) -> void {
					pointer->release(*this);
				}
		};

	public:
		using unique_pointer_type = std::unique_ptr<releasable, deleter>;

		constexpr explicit releasable(dtor_type dtor) noexcept :
				dtor_{dtor} {}

		// allocate a new `resource<T>` and construct it as `releasable` (which is a base class)
		template <class Type>
		constexpr static auto allocate(Alloc& alloc) -> unique_pointer_type {
			using resource_type = resource<Type, Alloc>;
			using resource_alloc_type = std::allocator_traits<Alloc>::template rebind_alloc<resource_type>;
			using resource_alloc_traits = std::allocator_traits<resource_alloc_type>;
			resource_alloc_type resource_alloc{alloc};
			// dtor is initialized to destroy<releasable> + dealloc<resource>. `releasable()` ctor won't
			// throw so this is correct.
			releasable* ptr = resource_alloc_traits::allocate(resource_alloc, 1);
			dtor_type dtor = &releasable::destroy_and_deallocate<Type, releasable>;
			allocator_traits::construct(alloc, ptr, dtor);
			return unique_pointer_type{ptr, deleter{alloc}};
		}

		// can also be invoked from `autorelease_pool::allocator`
		template <class Type>
		constexpr auto construct(Alloc& alloc, auto&&... args)
			requires std::constructible_from<Type, decltype(args)...> {
			using resource_type = resource<Type, Alloc>;
			// "destroy" the releasable which should be trivial anyway
			allocator_traits::destroy(alloc, this);
			// construct resource on top of the existing allocated memory
			dtor_type dtor = &releasable::deallocate<Type>;
			auto* resource_ptr = static_cast<resource_type*>(this);
			// dtor is initialized first, so if `Type` throws then dtor will be deallocate<releasable>
			allocator_traits::construct(alloc, resource_ptr, dtor, std::forward<decltype(args)>(args)...);
			// update destructor *after* successful construction
			resource_ptr->dtor_ = &releasable::destroy_and_deallocate<Type, resource_type>;
		}

		// can also be invoked from `autorelease_pool::allocator`
		template <class Type, class ConstructedType>
		constexpr auto destroy(Alloc& alloc) -> void {
			dtor_ = &releasable::deallocate<Type>;
			allocator_traits::destroy(alloc, static_cast<ConstructedType*>(this));
		}

		template <class Type>
		constexpr auto get() -> Type* {
			using resource_type = resource<Type, Alloc>;
			return static_cast<resource_type*>(this)->get();
		}

	private:
		// dtor after `construct` is invoked
		template <class Type, class ConstructedType>
		constexpr auto destroy_and_deallocate(Alloc& alloc) -> void {
			destroy<Type, ConstructedType>(alloc);
			deallocate<Type>(alloc);
		}

		// dtor after `allocate` but before `construct` is invoked
		template <class Type>
		constexpr auto deallocate(Alloc& alloc) -> void {
			using resource_type = resource<Type, Alloc>;
			using resource_alloc_type = std::allocator_traits<Alloc>::template rebind_alloc<resource_type>;
			using resource_alloc_traits = std::allocator_traits<resource_alloc_type>;
			resource_alloc_type resource_alloc{alloc};
			dtor_ = nullptr;
			resource_alloc_traits::deallocate(resource_alloc, static_cast<resource_type*>(this), 1);
		}

		constexpr auto release(Alloc& alloc) -> void {
			(this->*dtor_)(alloc);
		}

		dtor_type dtor_;
};

// Fully initialized object holder. This is constructed on top of a pre-allocated `releasable`.
template <class Type, class Alloc>
class resource : public releasable<Alloc>, private Type {
	public:
		constexpr explicit resource(releasable<Alloc>::dtor_type dtor, auto&&... args) :
				releasable<Alloc>{dtor},
				Type(std::forward<decltype(args)>(args)...) {}

		constexpr auto get() -> Type* { return this; }
		constexpr static auto translate(Type* ptr) -> resource* {
			return static_cast<resource*>(ptr);
			// Making `Type` a member variable allows instantiation of non-object and `final` classes but raises
			// `-Wno-invalid-offsetof`.
			// auto* bytes = reinterpret_cast<std::byte*>(ptr);
			// // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			// return reinterpret_cast<resource*>(bytes - offsetof(resource, value_));
		}
};

// Container of allocated resources. On destruction of the pool, all resources will be released.
export class autorelease_pool : util::non_copyable {
	private:
		// Deleter for `std::unique_ptr`. This is different than the `releasable::deleter` in that it
		// simply erases the pointer from the set. That, in turn, invokes the `releasable` destructor.
		template <class Type>
		class deleter {
			public:
				constexpr explicit deleter(autorelease_pool& pool) :
						pool_{&pool} {}

				auto operator()(Type* pointer) const -> void {
					using resource_type = resource<Type, allocator_type>;
					pool_->erase(resource_type::translate(pointer));
				}

			private:
				autorelease_pool* pool_;
		};

		using allocator_type = std::allocator<void>;
		using releasable_type = releasable<allocator_type>;
		using container_type = std::flat_set<releasable_type::unique_pointer_type, pointer_less<releasable_type>>;

	public:
		template <class> class allocator;
		template <class Type>
		using unique_ptr = std::unique_ptr<Type, deleter<Type>>;

		autorelease_pool() = default;
		explicit autorelease_pool(const allocator_type& alloc) :
				allocator_{alloc} {}

		auto clear() -> void {
			releasables_.clear();
		}

		// Nope!
		// template <class Type>
		// constexpr auto make_shared(auto&&... args) -> std::shared_ptr<Type>
		// 	requires std::constructible_from<Type, decltype(args)...> {
		// 	return std::allocate_shared<Type>(allocator<Type>{*this}, std::forward<decltype(args)>(args)...);
		// }

		template <class Type>
		auto make_unique(auto&&... args) -> unique_ptr<Type>
			requires std::constructible_from<Type, decltype(args)...> {
			releasable_type* allocation = emplace<Type>();
			allocation->construct<Type>(allocator_, std::forward<decltype(args)>(args)...);
			return reacquire(allocation->get<Type>());
		}

		constexpr auto resource_alloc() -> allocator_type& {
			return allocator_;
		}

		template <class Type>
		constexpr auto reacquire(Type* pointer) -> unique_ptr<Type> {
			return unique_ptr<Type>{pointer, deleter<Type>{*this}};
		}

	protected:
		template <class> friend class allocator;

		template <class Type>
		auto emplace() -> releasable_type* {
			return releasables_.emplace(releasable_type::allocate<Type>(allocator_)).first->get();
		}

		auto erase(releasable_type* ptr) -> void {
			releasables_.erase(find(ptr));
		}

		auto find(releasable_type* ptr) const -> container_type::const_iterator {
			auto iterator = releasables_.find(ptr);
			assert(iterator != releasables_.end());
			return iterator;
		}

	private:
		container_type releasables_;
		[[no_unique_address]] allocator_type allocator_;
};

// Allocator which allocates in the underlying autorelease pool.
template <class Type>
class autorelease_pool::allocator {
	private:
		template <class> friend class allocator;
		using resource_type = resource<Type, allocator_type>;

	public:
		using value_type = Type;
		using pointer = value_type*;

		// Nope!
		template <class Other>
		struct rebind {
				using other = void;
		};

		// Initialize from pool
		constexpr explicit allocator(autorelease_pool& pool) :
				pool_{&pool} {}

		// Copy from other allocator
		template <class T>
		constexpr explicit allocator(const allocator<T>& alloc) :
				pool_{alloc.pool_} {}

		constexpr auto allocate(std::size_t count) -> pointer {
			if (count != 1) {
				throw std::bad_array_new_length{};
			}
			releasable_type* allocation = pool_->emplace<value_type>();
			return allocation->get<value_type>();
		}

		constexpr auto construct(pointer ptr, auto&&... args) -> void
			requires std::constructible_from<value_type, decltype(args)...> {
			auto* resource_ptr = resource_type::translate(ptr);
			resource_ptr->template construct<Type>(pool_->resource_alloc(), std::forward<decltype(args)>(args)...);
		}

		constexpr auto deallocate(pointer ptr, std::size_t /*count*/) -> void {
			pool_->erase(resource_type::translate(ptr));
		}

		constexpr auto destroy(pointer ptr) -> void {
			resource_type* resource = resource_type::translate(ptr);
			resource->template destroy<Type, resource_type>(pool_->resource_alloc());
		}

	private:
		autorelease_pool* pool_;
};

} // namespace util
