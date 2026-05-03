module;
#include "auto_js/export_tag.h"
export module isolated_vm:support.initialize_fwd;
import :value;
import auto_js;
import std;
import util;

namespace isolated_vm {
export namespace detail {
using make_addon_names = auto() -> std::vector<std::u16string>;
using accept_addon_values = auto(std::span<value_of<prototype_tag>>) -> void;
using initialize_addon = auto(const basic_lock&, util::function_ref<accept_addon_values>) -> void;
} // namespace detail

export class EXPORT addon {
	public:
		template <class Type>
		explicit addon(std::type_identity<Type> /*environment*/, auto callback);

	private:
		static auto register_addon(detail::make_addon_names* names, detail::initialize_addon* initialize) -> void;
};

// ---

// Return a `std::tuple` of either the names or values of the module namespace descriptor, ignoring
// `std::in_place` if it exists.
template <std::size_t Index>
consteval auto get_descriptors(std::integral_constant<std::size_t, Index> /*index*/, auto ns) {
	return util::overloaded{
		[]<class... Types>(std::tuple<std::in_place_t, Types...> ns) {
			auto [... ii ] = util::sequence<sizeof...(Types)>;
			auto [... entries ] = std::make_tuple(std::get<ii + 1>(ns)...);
			return std::make_tuple(std::get<Index>(entries)...);
		},
		[](auto ns) {
			auto [... entries ] = ns;
			return std::tuple{std::get<Index>(entries)...};
		}
	}(ns);
};

template <class Type>
addon::addon(std::type_identity<Type> /*environment*/, auto callback) {
	using callback_type = decltype(callback);
	static_assert(std::is_empty_v<callback_type>);

	constexpr auto make_names = []() -> std::vector<std::u16string> {
		constexpr auto names = get_descriptors(std::integral_constant<std::size_t, 0>{}, callback_type{}());
		// Check for duplicates (only at compile time)
		static_assert([ names ]() {
			auto [... names_cw ] = names;
			auto names_vector = std::vector{js::transfer_strict<std::u16string>(names_cw)...};
			for (auto ii = names_vector.begin(); ii != names_vector.end(); ++ii) {
				auto remaining = std::ranges::subrange{std::next(ii), names_vector.end()};
				if (std::ranges::find(remaining, *ii) != remaining.end()) {
					throw std::runtime_error{"duplicate export name in isolated-vm addon"};
				}
			}
			return true;
		}());
		// Make `std::vector` of strings
		auto [... names_cw ] = names;
		return std::vector{js::transfer_strict<std::u16string>(names_cw)...};
	};
	constexpr auto make_exports = [](const basic_lock& lock, util::function_ref<detail::accept_addon_values> make) -> void {
		constexpr auto values = get_descriptors(std::integral_constant<std::size_t, 1>{}, callback_type{}());
		constexpr auto size = std::tuple_size_v<decltype(values)>;
		auto vm_values = js::transfer_in_strict<std::array<value_of<prototype_tag>, size>>(values, lock);
		make(std::span{vm_values});
	};
	register_addon(make_names, make_exports);
}

} // namespace isolated_vm
