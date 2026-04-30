module;
#include "auto_js/export_tag.h"
export module isolated_vm:support.initialize_fwd;
import :support.environment_fwd;
import auto_js;
import std;

namespace isolated_vm {
export namespace detail {
using registration_signature = void(environment&, void*);
} // namespace detail

export class EXPORT addon {
	public:
		template <class Type>
		explicit addon(std::type_identity<Type> /*environment*/, auto callback);

	private:
		static auto register_addon(detail::registration_signature* initialize, void* data) -> void;
};

// ---

template <class Type>
addon::addon(std::type_identity<Type> /*environment*/, auto callback) {
	using callback_type = decltype(callback);
	static_assert(std::is_empty_v<callback_type>);
	auto hook = [](environment& env, void* /*data*/) {
		callback_type callback{};
		(*callback)(env);
	};
	register_addon(hook, &callback);
}

} // namespace isolated_vm
