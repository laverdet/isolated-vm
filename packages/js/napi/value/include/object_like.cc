export module napi_js.object_like;
import napi_js.handle;
import nodejs;

namespace js::napi {

export class object_like : public handle {
	public:
		using handle::handle;

		[[nodiscard]] auto get(napi_value key) const -> napi_value;
		[[nodiscard]] auto has(napi_value key) const -> bool;
		auto set(napi_value key, napi_value value) -> void;
};

} // namespace js::napi
