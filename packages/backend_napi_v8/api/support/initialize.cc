export module isolated_vm:support.initialize;
import :support.initialize_fwd;

namespace isolated_vm {

#if EXPORT_IS_EXPORT

inline thread_local detail::make_addon_names* names_registration_callback = nullptr;
inline thread_local detail::initialize_addon* exports_registration_callback = nullptr;

export inline auto subscribe_registration(auto invoke) -> auto {
	auto result = invoke();
	auto names_callback = std::exchange(names_registration_callback, {});
	auto exports_callback = std::exchange(exports_registration_callback, {});
	if (names_callback == nullptr || exports_callback == nullptr) {
		throw js::runtime_error{u"isolated-vm 'NativeModule' did not register"};
	} else {
		return std::tuple{invoke(), names_callback, exports_callback};
	}
}

auto addon::register_addon(detail::make_addon_names* names, detail::initialize_addon* initialize) -> void {
	names_registration_callback = names;
	exports_registration_callback = initialize;
}

#endif

} // namespace isolated_vm
