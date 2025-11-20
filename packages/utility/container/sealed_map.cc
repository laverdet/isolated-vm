module;
#include <algorithm>
#include <concepts>
#include <format>
#include <ranges>
#include <type_traits>
#include <utility>
export module util:container.sealed_map;
import :functional;

namespace util {

struct sorted_equivalent_t {};

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

				constexpr auto operator*() const -> std::size_t { return index_; }
				explicit constexpr operator bool() const { return index_ != std::numeric_limits<value_type>::max(); }

			private:
				size_t index_;
		};

		// Check for duplicates
		explicit consteval sealed_map(sorted_equivalent_t /*tag*/, container_type values) :
				values_{std::move(values)} {
			const auto projection = [](const auto& entry) { return entry.first; };
			const auto duplicate = std::ranges::adjacent_find(values, std::equal_to{}, projection);
			if (duplicate != values.end()) {
				constexpr auto render = util::overloaded{
					[](const auto& /*value*/) -> std::string { return "[??]"; },
					[](std::string value) -> std::string { return value; },
				};
				// This error message can't actually render in constexpr but it's the thought that counts.
				throw std::logic_error{std::format("Duplicate key {}", render(projection(*duplicate)))};
			}
		}

	public:
		// Default constructed values
		explicit consteval sealed_map(std::type_identity<Type> /*default_construct*/, auto... keys)
			requires std::default_initializable<Type> :
				sealed_map{
					sorted_equivalent_t{},
					util::elide{[ & ]() -> container_type {
						auto keys_array = std::array<key_type, Size>{std::move(keys)...};
						std::ranges::sort(keys_array);
						auto [... sorted_keys ] = std::move(keys_array);
						return {value_type{std::move(sorted_keys), Type{}}...};
					}}
				} {}

		// Keys & values provided
		explicit consteval sealed_map(std::in_place_t /*tag*/, auto... pairs) :
				sealed_map{
					sorted_equivalent_t{},
					util::elide{[ & ]() -> container_type {
						// Sort without invoking `operator()=`, which might not exist on the value type
						auto entries = std::array<value_type, Size>{std::move(pairs)...};
						auto indices = std::array<size_t, Size>{};
						std::ranges::copy(std::ranges::iota_view{size_t{0}, Size}, indices.begin());
						const auto projection = [ & ](size_t ii) -> const auto& { return entries.at(ii).first; };
						std::ranges::sort(indices, std::less{}, projection);
						const auto [... sorted_indices ] = indices;
						return {value_type(std::move(entries.at(sorted_indices)))...};
					}}
				} {}

		[[nodiscard]] constexpr auto begin(this auto& self) { return self.values_.begin(); }
		[[nodiscard]] constexpr auto end(this auto& self) { return self.values_.end(); }

		// Returns a reference to the value at the key previously found by `lookup`.
		[[nodiscard]] constexpr auto at(this auto& self, key_for index) -> auto& {
			if (!index) {
				throw std::out_of_range{"Key not found"};
			}
			return *std::next(self.values_.begin(), *index);
		}

		// Find the given key in the map and return a nullable pointer to the value.
		[[nodiscard]] constexpr auto find(const auto& key) const -> auto* {
			const auto index = lookup(key);
			return index ? std::addressof(at(index)) : nullptr;
		}

		// Return an opaque index type which can be used with `at`. Generally should be used in a
		// consteval context.
		[[nodiscard]] constexpr auto lookup(const auto& key) const -> key_for {
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
