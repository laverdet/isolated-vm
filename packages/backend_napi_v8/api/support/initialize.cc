export module isolated_vm:support.initialize;
import :support.initialize_fwd;

namespace isolated_vm {

#if EXPORT_IS_EXPORT

inline thread_local detail::registration_signature* registration_callback = nullptr;
inline thread_local void* registration_data = nullptr;

export inline auto subscribe_registration(auto invoke) -> auto {
	auto result = invoke();
	auto callback = std::exchange(registration_callback, {});
	if (callback == nullptr) {
		throw js::runtime_error{u"isolated-vm 'NativeModule' did not register"};
	}
	return std::tuple{invoke(), std::tuple{callback, std::exchange(registration_data, nullptr)}};
}

auto addon::register_addon(detail::registration_signature* initialize, void* data) -> void {
	registration_callback = initialize;
	registration_data = data;
}

#endif

} // namespace isolated_vm
