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
import :tag;
import :visit;

namespace ivm::value {

// You override this with an object property descriptor for each accepted type
export template <class Type>
struct object_properties;

// Key & `required` flag for object property
struct property_name {
		// TODO: default constructor should be removed after `prehashed_string_map` no longer requires
		// temporary defaults for initialization
		property_name() = default;
		constexpr property_name(const std::string& name, bool required = true) :
				name{name},
				required{required} {}

		std::string name;
		bool required;
};

// Used to extract the type of complementary getter / setter functions, ensuring that they are the
// same and at least one is non-void.
template <class Left, class Right>
struct symmetric_parameter {
		using type = std::conditional_t<std::is_same_v<Left, void>, Right, Left>;
		static_assert(!std::is_same_v<type, void>);
		static_assert(std::is_same_v<type, Right> || std::is_same_v<void, Right>);
};

template <class Left, class Right>
using symmetric_parameter_t = symmetric_parameter<Left, Right>::type;

// Getter helper, which extracts the type of a getter
template <auto Getter>
struct getter;

template <auto Getter>
using getter_t = getter<Getter>::type;

template <>
struct getter<nullptr> {
		using subject = void;
		using type = void;
};

template <class Type, class Subject, const Type& (Subject::* Getter)()>
struct getter<Getter> {
		using subject = Subject;
		using type = Type;
};

template <class Type, class Subject, Type (Subject::* Getter)()>
struct getter<Getter> {
		static_assert(std::is_same_v<Type, std::remove_cvref_t<Type>>);
		static_assert(!std::is_same_v<Type, void>);
		using subject = Subject;
		using type = Type;
};

// Setter helper, which extracts the type of a setter
template <auto Setter>
struct setter;

template <auto Setter>
using setter_t = setter<Setter>::type;

template <>
struct setter<nullptr> {
		using subject = void;
		using type = void;
};

template <class Type, class Subject, void (Subject::* Setter)(const Type&)>
struct setter<Setter> {
		using subject = Subject;
		using type = Type;
};

template <class Type, class Subject, void (Subject::* Setter)(Type&&)>
struct setter<Setter> {
		using subject = Subject;
		using type = Type;
};

template <class Type, class Subject, void (Subject::* Setter)(Type)>
struct setter<Setter> {
		static_assert(std::is_same_v<Type, std::remove_cvref_t<Type>>);
		using subject = Subject;
		using type = Type;
};

// Helper which delegates to a getter & setter. Ensures same usage as a plain property access.
template <auto Getter, auto Setter>
struct accessor_delegate {
	private:
		using subject = symmetric_parameter_t<typename getter<Getter>::subject, typename setter<Setter>::subject>;

	public:
		using type = symmetric_parameter_t<getter_t<Getter>, setter_t<Setter>>;

		constexpr auto get(const subject& subject) const -> decltype(auto)
			requires std::negation_v<std::is_same<getter_t<Getter>, void>> {
			return (&subject->*Getter)();
		}

		constexpr auto set(subject& subject, auto&& value) const -> void
			requires std::negation_v<std::is_same<setter_t<Setter>, void>> {
			(&subject->*Setter)(std::forward<decltype(value)>(value));
		}
};

// Helper which passes through plain object property access
template <auto Member>
struct member_delegate;

template <class Subject, class Type, Type Subject::* Member>
struct member_delegate<Member> {
		using type = Type;
		static_assert(std::is_same_v<Type, std::remove_cvref_t<Type>>);
		constexpr auto get(const Subject& subject) const -> const Type& { return subject.*Member; }
		constexpr auto set(Subject& subject, const Type& value) const -> void { subject.*Member = std::move(value); }
};

// Descriptor for object getter and/or setter
export template <auto Getter, auto Setter>
struct accessor : property_name {
		using property_name::property_name;
		using type = symmetric_parameter_t<getter_t<Getter>, setter_t<Setter>>;
};

// Descriptor for object member
export template <auto Member>
struct property;

template <class Type, class Subject, Type Subject::* Member>
struct property<Member> : property_name {
		using property_name::property_name;
		using type = Type;
};

// Unwrap type from `accessor` or `property`
template <class Type>
struct member;

template <auto Getter, auto Setter>
struct member<accessor<Getter, Setter>> {
		using type = accessor_delegate<Getter, Setter>;
};

template <auto Member>
struct member<property<Member>> {
		using type = member_delegate<Member>;
};

template <class Type>
using member_t = member<Type>::type;

// Acceptor types for each member of an object
template <class Meta, class Type>
struct property_acceptors : property_acceptors<Meta, std::decay_t<decltype(object_properties<Type>::properties)>> {};

template <class Meta, class... Types>
struct property_acceptors<Meta, std::tuple<Types...>> {
		using type = std::tuple<accept_next<Meta, typename member_t<Types>::type>...>;
};

template <class Meta, class Type>
using property_acceptors_t = property_acceptors<Meta, Type>::type;

// Acceptor function for C++ object types
template <class Meta, class Type>
	requires std::destructible<object_properties<Type>>
struct accept<Meta, Type> : accept<Meta, void> {
	public:
		using hash_type = uint32_t;
		using descriptor_type = object_properties<Type>;
		using accept<Meta, void>::accept;

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> Type {
			using range_type = decltype(util::into_range(dictionary));
			using value_type = std::ranges::range_value_t<range_type>::second_type;
			using visit_type = std::decay_t<decltype(visit)>;
			hash_type checksum{};
			Type subject;
			if constexpr (std::tuple_size_v<decltype(descriptor_type::properties)> != 0) {
				auto [ expected_checksum, property_map ] = make_property_map<value_type, visit_type>();
				for (auto&& [ key, value ] : util::into_range(dictionary)) {
					auto descriptor = property_map.get(visit.first(std::forward<decltype(key)>(key), first));
					if (descriptor != nullptr) {
						checksum ^= descriptor->first;
						(descriptor->second)(*this, subject, std::forward<decltype(value)>(value), visit);
					}
				}
				if (checksum != expected_checksum) {
					throw std::logic_error(std::format("Checksum mismatch: {} != {}", checksum, expected_checksum));
				}
			}
			return subject;
		}

		// Returns: `std::pair{checksum, std::tuple{optional_hash, property_hash, acceptor}}`
		template <class Value, class Visit>
		consteval static auto make_property_map() {
			using acceptor_type = void (*)(const accept&, Type&, const Value&, const Visit&);

			// Make acceptor map w/ property information
			auto initial_property_map = util::prehashed_string_map{std::invoke(
				[]<size_t... Index>(std::index_sequence<Index...> /*indices*/) consteval {
					return std::array{std::invoke(
						[](const auto& property) {
							acceptor_type acceptor = [](const accept& self, Type& subject, const Value& value, const Visit& visit) -> void {
								using delegate_type = member_t<std::decay_t<decltype(property)>>;
								delegate_type delegate{};
								const auto& accept = std::get<Index>(self.second);
								delegate.set(subject, visit.second(std::move(value), accept));
							};
							return std::pair{property.name, std::pair{static_cast<const property_name&>(property), acceptor}};
						},
						std::get<Index>(descriptor_type::properties)
					)...};
				},
				std::make_index_sequence<std::tuple_size_v<decltype(descriptor_type::properties)>>{}
			)};

			// Extract acceptors from initial property map
			// value_type -> `std::pair{hash, std::pair{optional_hash, acceptor}}`
			auto property_map = initial_property_map.transform([](const auto& entry) {
				auto [ key, value ] = entry;
				auto [ property, acceptor ] = value;
				auto optional_hash = property.required ? key : 0;
				return std::pair{optional_hash, acceptor};
			});

			// Calculate expected object hash
			auto non_optional_hashes =
				property_map |
				std::views::transform([](const auto& entry) constexpr { return entry.second.first; });
			auto checksum = std::ranges::fold_left(non_optional_hashes, 0, std::bit_xor{});

			return std::pair{checksum, property_map};
		}

	private:
		accept_next<Meta, std::string> first;
		property_acceptors_t<Meta, Type> second;
};

} // namespace ivm::value
