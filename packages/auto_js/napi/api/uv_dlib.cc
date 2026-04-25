module napi_js;
import :api.uv_dlib;
import util;

namespace js::napi {

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
uv_dlib::uv_dlib(const std::string& filename) {
	if (uv_dlopen(filename.c_str(), &lib_) != 0) {
		throw js::runtime_error{util::transcode_string<char16_t>(std::string_view{uv_dlerror(&lib_)})};
	}
}

uv_dlib::uv_dlib(uv_dlib&& right) noexcept : lib_{std::exchange(right.lib_, {})} {}

auto uv_dlib::operator=(uv_dlib&& right) noexcept -> uv_dlib& {
	lib_ = std::exchange(right.lib_, {});
	return *this;
}

uv_dlib::~uv_dlib() {
	if (lib_.handle != nullptr) {
		uv_dlclose(&lib_);
	}
}

} // namespace js::napi
