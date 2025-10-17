module;
#include <cassert>
#include <utility>
#include <vector>
export module isolated_js:reference_of;
import ivm.utility;

namespace js {

// Reference to a transferred object which is stored in a `reference_storage_of` container
export template <class Type>
class reference_of {
	public:
		constexpr explicit reference_of(unsigned id) noexcept : id_{id} {}
		[[nodiscard]] constexpr auto id() const -> unsigned { return id_; }

	private:
		unsigned id_;
};

// Storage for `reference_of` underlying instances
export template <class Type>
class reference_storage_of {
	public:
		using reference_type = reference_of<Type>;

		reference_storage_of() = default;
		constexpr explicit reference_storage_of(Type value) :
				references_{std::move(value)} {}

		constexpr auto at(reference_type reference) const& -> const Type& { return references_.at(reference.id()); }
		constexpr auto at(reference_type reference) && -> Type { return std::move(references_).at(reference.id()); }

		constexpr auto allocate() -> reference_type {
			references_.emplace_back();
			return reference_type{static_cast<unsigned>(references_.size() - 1)};
		}

		constexpr auto place_at(reference_type reference, auto&& value) -> void {
			references_.at(reference.id()) = std::forward<decltype(value)>(value);
		}

	private:
		std::vector<Type> references_;
};

// Aggregated reference storage for many types
export template <class... Types>
class reference_storage : public reference_storage_of<Types>... {
	public:
		reference_storage() = default;

		// Move from individual storages
		constexpr explicit reference_storage(reference_storage_of<Types>&&... storage)
			requires(sizeof...(Types) > 0)
				: reference_storage_of<Types>{std::move(storage)}... {}

		// Initialize from a single value
		// Note: This constructor is really cool.
		template <class Value>
			requires(... || (type<Value> == type<Types>))
		constexpr explicit reference_storage(Value value) :
				reference_storage_of<Types>{util::elide{[ & ]() -> reference_storage_of<Types> {
					if constexpr (type<Value> == type<Types>) {
						return reference_storage_of<Types>{std::move(value)};
					} else {
						return reference_storage_of<Types>{};
					}
				}}}... {}

		using reference_storage_of<Types>::at...;
};

} // namespace js
