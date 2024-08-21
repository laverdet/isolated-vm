module;
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.utility:prehashed_string_map;
import :comparator;
import :type_traits;
import :utility;

// `constexpr` hash for property lookup
constexpr auto djb2_hash(std::ranges::range auto&& view) -> uint32_t {
	uint32_t hash = 5'381;
	for (auto character : view) {
		hash = ((hash << 5U) + hash) + character;
	}
	return hash;
}

// Precomputed map of hashed string values to a value type
export template <class Type, size_t Size>
class prehashed_string_map {
	public:
		using hash_type = uint32_t;
		using mapped_type = Type;
		using value_type = std::pair<hash_type, mapped_type>;
		using container_type = std::array<value_type, Size>;
		using iterator = container_type::const_iterator;
		using const_iterator = container_type::const_iterator;

		consteval explicit prehashed_string_map(const std::ranges::range auto& range) :
				hashed_values_{std::invoke([ & ]() {
					// Calculate hash values
					auto hashed_values = container_type{};
					std::transform(range.begin(), range.end(), hashed_values.begin(), [](const auto& entry) {
						auto hash = djb2_hash(std::string_view{entry.first});
						auto value = entry.second;
						return std::tuple{hash, value};
					});
					// Sort by hash
					auto map = [](const auto& entry) constexpr { return entry.first; };
					std::sort(hashed_values.begin(), hashed_values.end(), mapped_comparator{std::less{}, map});
					// Check for duplicates
					auto duplicate = std::adjacent_find(
						hashed_values.begin(),
						hashed_values.end(),
						mapped_comparator{std::equal_to{}, map}
					);
					if (duplicate != hashed_values.end()) {
						// This error message can't actually render in constexpr but it's the thought that
						// counts.
						throw std::logic_error(std::format("Duplicate hash {}", map(*duplicate)));
					}
					return hashed_values;
				})} {}

		// Creates a new map from prehashed values
		consteval prehashed_string_map(
			std::type_identity<prehashed_string_map> /*tag*/,
			const std::ranges::range auto& prehashed_values
		) {
			std::copy(std::begin(prehashed_values), std::end(prehashed_values), std::begin(hashed_values_));
		}

		constexpr auto begin() const -> const_iterator { return hashed_values_.begin(); }
		constexpr auto end() const -> const_iterator { return hashed_values_.end(); }

		constexpr auto get(std::ranges::range auto&& string_view) const -> const mapped_type* {
			// Lookup property by key
			auto map = overloaded{
				[](hash_type hash) constexpr { return hash; },
				[](const auto& entry) constexpr { return entry.first; },
			};
			auto string_hash = djb2_hash(std::forward<decltype(string_view)>(string_view));
			auto range = std::equal_range(
				hashed_values_.begin(),
				hashed_values_.end(),
				string_hash,
				mapped_comparator{std::less{}, map}
			);
			// Check if key was found
			if (range.first == range.second) {
				return nullptr;
			}
			return &range.first->second;
		}

		[[nodiscard]] consteval auto size() const -> size_t {
			return hashed_values_.size();
		}

		// `transformer` expects a `value_type` and returns a `mapped_type`
		constexpr auto transform(const auto& transformer) const {
			using next_mapped_type = std::invoke_result_t<decltype(transformer), const value_type&>;
			using next_prehashed_type = prehashed_string_map<next_mapped_type, Size>;
			auto entries =
				hashed_values_ |
				std::views::transform([ & ](const auto& entry) {
					return std::pair{entry.first, transformer(entry)};
				});
			return next_prehashed_type{std::type_identity<next_prehashed_type>{}, entries};
		}

	private:
		container_type hashed_values_;
};
