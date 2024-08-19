module;
#include <boost/container/flat_set.hpp>
#include <memory>
#include <utility>
export module ivm.utility:collection_group;
import :type_traits;

// v8 may not invoke destructors on `External` objects, but we still want to give it owning
// references to resources. This provides an allocator-like interface to manage those resources. If
// a `collection_group` goes out of scope then all objects allocated/created will be appropriately
// destroyed and deallocated.
//
// Not thread safe! Also, destroying a collected resourced after the group goes out of scope is UB.
export class collection_group {
	private:
		class holder;

	public:
		auto collect(auto* ptr) -> void;

		template <class Type>
		auto make_ptr(auto&&... args)
			requires std::constructible_from<Type, decltype(args)...>;

	private:
		template <class Type>
		auto insert() -> holder&;

		boost::container::flat_set<holder> objects;
};

class collection_group::holder : non_copyable {
	public:
		holder() = default;

		template <class Type>
		explicit holder(std::type_identity<Type> /*type*/) :
				ptr{std::allocator<Type>{}.allocate(1)},
				dtor{&holder::deallocate<Type>} {}

		// Non-owning reference
		explicit holder(void* ptr) :
				ptr{ptr} {};

		holder(holder&& that) noexcept :
				ptr{std::exchange(that.ptr, nullptr)},
				dtor{std::exchange(that.dtor, nullptr)} {}

		~holder() {
			if (dtor != nullptr) {
				(this->*dtor)();
			}
		}

		auto operator=(holder&& that) noexcept -> holder& {
			ptr = std::exchange(that.ptr, nullptr);
			dtor = std::exchange(that.dtor, nullptr);
			return *this;
		};

		template <class Type>
		void construct(auto&&... args) {
			std::construct_at(static_cast<Type*>(ptr), std::forward<decltype(args)>(args)...);
			dtor = &holder::destroy_and_deallocate<Type>;
		}

		template <class Type>
		auto deallocate() -> void {
			dtor = nullptr;
			std::allocator<Type>{}.deallocate(static_cast<Type*>(ptr), 1);
		}

		template <class Type>
		void destroy() {
			dtor = &holder::deallocate<Type>;
			std::destroy_at(static_cast<Type*>(ptr));
		}

		template <class Type>
		[[nodiscard]] auto get() const -> Type* { return static_cast<Type*>(ptr); }

		auto operator<=>(const holder& other) const { return ptr <=> other.ptr; };
		auto operator==(const holder& other) const { return ptr == other.ptr; };

	private:
		template <class Type>
		auto destroy_and_deallocate() -> void {
			destroy<Type>();
			deallocate<Type>();
		}

		void* ptr{};
		void (holder::* dtor)(){};
};

auto collection_group::collect(auto* ptr) -> void {
	objects.erase(holder{static_cast<void*>(ptr)});
}

template <class Type>
auto collection_group::insert() -> holder& {
	return *objects.emplace(std::type_identity<Type>{}).first;
}

template <class Type>
auto collection_group::make_ptr(auto&&... args)
	requires std::constructible_from<Type, decltype(args)...> {
	auto& resource = insert<Type>();
	resource.template construct<Type>(std::forward<decltype(args)>(args)...);
	auto destructor = [ this ](Type* ptr) { collect(ptr); };
	return std::unique_ptr<Type, decltype(destructor)>{resource.template get<Type>(), destructor};
}
