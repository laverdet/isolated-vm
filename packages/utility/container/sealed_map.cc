module;
#include <algorithm>
#include <concepts>
#include <format>
#include <functional>
#include <type_traits>
#include <utility>
export module ivm.utility:sealed_map;
import :comparator;
import :utility;

namespace util {

struct sorted_equivalent_t {};
struct sorted_unique_t {};
struct unsorted_t {};

export template <class Key, class Type, size_t Size>
class sealed_map {
	public:
		using key_type = Key;
		using value_type = std::pair<const key_type, Type>;
		using container_type = std::array<value_type, Size>;

	private:
		// Holder for consteval'd key into the map
		struct key_for {
				using value_type = size_t;
				constexpr key_for() :
						index_{std::numeric_limits<value_type>::max()} {}
				explicit constexpr key_for(auto index) :
						index_{static_cast<value_type>(index)} {}

				constexpr auto operator*() const { return index_; }
				explicit constexpr operator bool() const { return index_ != std::numeric_limits<value_type>::max(); }

			private:
				size_t index_;
		};

		explicit consteval sealed_map(sorted_unique_t /*tag*/, container_type values) :
				values_{std::invoke(
					[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
						return container_type{invoke(std::integral_constant<size_t, Index>{})...};
					},
					[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
						return value_type{std::move(values.at(Index))};
					},
					std::make_index_sequence<Size>{}
				)} {}

		// Check for duplicates
		explicit consteval sealed_map(sorted_equivalent_t /*tag*/, container_type values) :
				sealed_map{
					sorted_unique_t{},
					std::invoke([ & ]() constexpr {
						auto map = [](const auto& entry) constexpr { return entry.first; };
						auto duplicate = std::adjacent_find(
							values.begin(),
							values.end(),
							mapped_comparator{std::equal_to{}, map}
						);
						if (duplicate != values.end()) {
							// This error message can't actually render in constexpr but it's the thought that counts.
							throw std::logic_error{std::format("Duplicate key {}", map(*duplicate))};
						}
						// nb: It's not a pessimization because `values` is a capture
						return std::move(values);
					})
				} {}

		// Sort inputs
		explicit consteval sealed_map(unsorted_t /*tag*/, container_type values) :
				sealed_map{
					sorted_equivalent_t{},
					std::invoke([ & ]() constexpr {
						// Sort without invoking `operator()=` which might not exist on the value type
						std::array<typename container_type::iterator, Size> sortable;
						auto inserter = sortable.begin();
						for (auto ii = values.begin(); ii != values.end(); ++ii) {
							*(inserter++) = ii;
						};
						auto map = [](const auto& ii) constexpr { return ii->first; };
						std::sort(sortable.begin(), sortable.end(), mapped_comparator{std::less{}, map});
						return std::invoke(
							[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
								return container_type{invoke(std::integral_constant<size_t, Index>{})...};
							},
							[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
								return value_type{std::move(*sortable.at(Index))};
							},
							std::make_index_sequence<Size>{}
						);
					})
				} {}

	public:
		// Default constructed values
		explicit consteval sealed_map(std::type_identity<Type> /*default_construct*/, auto... keys)
			requires std::default_initializable<Type> :
				sealed_map{unsorted_t{}, container_type{std::pair{keys, Type{}}...}} {}

		// Keys & values provided
		explicit consteval sealed_map(std::in_place_t /*tag*/, auto&&... values) :
				sealed_map{unsorted_t{}, container_type{std::forward<decltype(values)>(values)...}} {}

		[[nodiscard]] constexpr auto begin(this auto&& self) { return self.values_.begin(); }
		[[nodiscard]] constexpr auto end(this auto&& self) { return self.values_.end(); }

		// Returns a reference to the value at the key previously found by `lookup`.
		[[nodiscard]] constexpr auto at(this auto&& self, key_for index) -> auto& {
			if (!index) {
				throw std::out_of_range{"Key not found"};
			}
			return *std::next(self.values_.begin(), *index);
		}

		// Find the given key in the map and return a nullable pointer to the value.
		[[nodiscard]] constexpr auto find(this auto&& self, const auto& key) {
			const auto index = self.lookup(key);
			return index ? std::next(self.values_.begin(), *index) : nullptr;
		}

		// Return an opaque index type which can be used with `at`. Generally should be used in a
		// consteval context.
		[[nodiscard]] constexpr auto lookup(const auto& key) const {
			auto less = overloaded{
				[](const value_type& left, const auto& right) constexpr { return left.first < right; },
				[](const auto& left, const value_type& right) constexpr { return left < right.first; },
			};
			auto range = std::equal_range(values_.begin(), values_.end(), key, less);
			return range.first == range.second ? key_for{} : key_for{std::distance(values_.begin(), range.first)};
		}

	private:
		container_type values_;
};

template <class Key, class Type, class... Rest>
sealed_map(std::type_identity<Type>, Key, Rest...) -> sealed_map<Key, Type, sizeof...(Rest) + 1>;

template <class Key, class Type, class... Rest>
sealed_map(std::in_place_t, std::pair<Key, Type>, Rest...) -> sealed_map<Key, Type, sizeof...(Rest) + 1>;

} // namespace util
