export module napi_js:api.uv_dlib;
import nodejs;
import std;

namespace js::napi {

export class uv_dlib {
	public:
		// Constructor must take `std::string` because it needs to be null-terminated.
		explicit uv_dlib(const std::string& filename);
		uv_dlib(const uv_dlib&) = delete;
		uv_dlib(uv_dlib&&) noexcept;
		auto operator=(const uv_dlib&) -> uv_dlib& = delete;
		auto operator=(uv_dlib&&) noexcept -> uv_dlib&;
		~uv_dlib();

	private:
		uv_lib_t lib_;
};

} // namespace js::napi
