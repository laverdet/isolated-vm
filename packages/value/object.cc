module;
#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
export module ivm.value:object;
import ivm.utility;
import :accept;
import :tag;

namespace ivm::value {

// You override this with an object property descriptor for each accepted type
export template <class Meta, class Type>
struct object_map;

// Each object property descriptor should inherit from this
export template <class Meta, class Type>
struct object_properties;

// Specialization to extract tuple types
export template <class Meta, class Value, class Type>
struct object_properties<std::tuple<Meta, Value>, Type> {
		using subject_accept = accept<Meta, Type>;
		using acceptor_type = void (*)(Type&, const Value&, const subject_accept&);

		// Invokes `accept` for the templated member property & runtime value. The key string has
		// already been checked at this point.
		template <class Result, Result Type::* Member>
		constexpr static auto accept(Type& subject, const Value& value, const subject_accept& accept) {
			// Skip accept for `std::nullopt_t`, which is instantiated in the `object_map` acceptor
			// requirement
			if constexpr (!std::is_same_v<Value, std::nullopt_t>) {
				subject.*Member = invoke_visit(value, make_accept<Result>(accept));
			}
		}

		// Helper consumed by subclasses, defines each property and matches to a statically addressable
		// acceptor function.
		template <auto Member>
		struct property;

		template <class Result, Result Type::* Member>
		struct property<Member> {
				constexpr static acceptor_type accept = &object_properties::accept<Result, Member>;
		};
};

// Property declaration for an object that expects no properties
export template <class Meta, class Type>
struct object_no_properties : object_properties<Meta, Type> {
		using acceptor_type = object_properties<Meta, Type>::acceptor_type;
		constexpr static auto properties = std::array<std::tuple<bool, const char*, acceptor_type>, 0>{};
};

// Acceptor function for C++ object types
template <class Meta, class Type>
	requires std::destructible<object_map<std::tuple<Meta, std::nullopt_t>, Type>>
struct accept<Meta, Type> {
		using hash_type = uint32_t;

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary) const -> Type {
			using range_type = decltype(util::into_range(dictionary));
			using value_type = std::ranges::range_value_t<range_type>::second_type;
			hash_type checksum{};
			Type subject;
			auto accept_key = make_accept<std::string>(*this);
			auto [ expected_checksum, property_map ] = make_property_map<value_type>();
			for (auto&& [ key, value ] : util::into_range(dictionary)) {
				auto descriptor = property_map.get(invoke_visit(std::forward<decltype(key)>(key), accept_key));
				if (descriptor != nullptr) {
					checksum ^= descriptor->first;
					(descriptor->second)(subject, std::forward<decltype(value)>(value), *this);
				}
			}
			if (checksum != expected_checksum) {
				throw std::logic_error(std::format("Checksum mismatch: {} != {}", checksum, expected_checksum));
			}
			return subject;
		}

		// Returns: `std::pair{checksum, std::tuple{optional_hash, property_hash, acceptor}}`
		template <class Value>
		consteval static auto make_property_map() {
			using descriptor_type = object_map<std::tuple<Meta, Value>, Type>;
			using acceptor_type = descriptor_type::acceptor_type;

			// Initial map calculates hash values
			auto properties = descriptor_type::properties;
			using initial_mapped_type = std::pair<bool, acceptor_type>;
			auto initial_property_map = util::prehashed_string_map<initial_mapped_type, properties.size()>{
				properties |
				std::views::transform([](const auto& entry) {
					auto is_required = std::get<0>(entry);
					auto key = std::get<1>(entry);
					auto acceptor = std::get<2>(entry);
					return std::pair{key, std::pair{is_required, acceptor}};
				})
			};

			// Save hash value for non-optional properties
			// value_type -> `std::pair{hash, std::pair{optional_hash, acceptor}}`
			auto property_map = initial_property_map.transform([](const auto& entry) {
				auto key = entry.first;
				auto value = entry.second;
				auto is_required = value.first;
				auto acceptor = value.second;
				auto optional_hash = is_required ? key : 0;
				return std::pair{optional_hash, acceptor};
			});

			// Calculate expected object hash
			auto non_optional_hashes =
				util::into_range(property_map) |
				std::views::transform([](const auto& entry) constexpr { return entry.second.first; });
			auto checksum = std::ranges::fold_left(non_optional_hashes, 0, std::bit_xor{});
			return std::pair{checksum, property_map};
		}
};

} // namespace ivm::value
