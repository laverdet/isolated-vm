module ivm.napi:handle;
import napi;

namespace ivm::js::napi {

export class handle {
	public:
		handle() = default;
		handle(napi_env env, napi_value value);

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const;
		auto operator*() const -> napi_value;
		auto operator==(const handle& right) const -> bool;
		[[nodiscard]] auto env() const -> napi_env;

	private:
		napi_env env_{};
		napi_value value_{};
};

} // namespace ivm::js::napi
