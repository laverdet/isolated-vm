module;
#include "auto_js/export_tag.h"
export module isolated_vm:support.initialize_fwd;
import :support.environment_fwd;
import :value;
import auto_js;
import std;
import util;

namespace isolated_vm {
export namespace detail {
using make_addon_names = auto() -> std::vector<std::u16string>;
using accept_addon_values = auto(std::span<value_of<>>) -> void;
using initialize_addon = auto(const environment&, util::function_ref<accept_addon_values>) -> void;
} // namespace detail

export class EXPORT addon {
	public:
		template <class Type>
		explicit addon(std::type_identity<Type> /*environment*/, auto callback);

	private:
		static auto register_addon(detail::make_addon_names* names, detail::initialize_addon* initialize) -> void;
};

// ---

template <class Type>
addon::addon(std::type_identity<Type> /*environment*/, auto callback) {
	using callback_type = decltype(callback);
	static_assert(std::is_empty_v<callback_type>);
	auto names = []() -> std::vector<std::u16string> {
		constexpr auto callback = callback_type{};
		auto [... exports ] = callback();
		auto names = std::vector{js::transfer<std::u16string>(std::get<0>(exports))...};
		for (auto ii = names.begin(); ii != names.end(); ++ii) {
			auto remaining = std::ranges::subrange{std::next(ii), names.end()};
			if (std::ranges::find(remaining, *ii) != remaining.end()) {
				throw js::runtime_error{u"duplicate export name in isolated-vm addon"};
			}
		}
		return names;
	};
	auto exports = [](const environment& env, util::function_ref<detail::accept_addon_values> make) -> void {
		constexpr auto values = []() {
			constexpr auto callback = callback_type{};
			auto [... exports ] = callback();
			return std::tuple{std::get<1>(exports)...};
		}();
		constexpr auto size = std::tuple_size_v<decltype(values)>;
		auto vm_values = js::transfer_in<std::array<value_of<>, size>>(values, env);
		make(std::span{vm_values});
	};
	register_addon(names, exports);
}

} // namespace isolated_vm
