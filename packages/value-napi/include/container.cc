module ivm.napi:container;
import napi;

namespace ivm::napi {

export class container {
	public:
		container() = default;
		container(napi_env env, napi_value value); // NOLINT(google-explicit-constructor)

		operator napi_value() const; // NOLINT(google-explicit-constructor)
		auto operator*() const -> napi_value;
		auto operator==(const container& right) const -> bool;
		[[nodiscard]] auto env() const -> napi_env;

	private:
		napi_env env_{};
		napi_value value_{};
};

} // namespace ivm::napi
