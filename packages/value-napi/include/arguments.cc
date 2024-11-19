module;
#include <compare>
#include <cstddef>
#include <ranges>
#include <vector>
export module ivm.napi:arguments;
import :container;
import ivm.utility;
import napi;

namespace ivm::napi {

export class arguments {
	public:
		explicit arguments(napi_env env, napi_callback_info info);

		[[nodiscard]] auto into_range() const -> std::vector<napi_value>;

	private:
		napi_env env_;
		napi_callback_info info_;
};

} // namespace ivm::napi
