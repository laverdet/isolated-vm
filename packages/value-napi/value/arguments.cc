module;
#include <vector>
module ivm.napi;
import :arguments;
import :utility;
import napi;

namespace ivm::napi {

constexpr size_t reserved_size = 8;

arguments::arguments(napi_env env, napi_callback_info info) :
		env_{env},
		info_{info} {}

auto arguments::into_range() const -> std::vector<napi_value> {
	std::vector<napi_value> arguments{reserved_size};
	auto load_into = [ & ]() {
		auto count = arguments.size();
		napi_check_result_of([ & ]() {
			return napi_get_cb_info(env_, info_, &count, arguments.data(), nullptr, nullptr);
		});
		return count;
	};
	auto count = load_into();
	if (count <= arguments.size()) {
		arguments.resize(count);
	} else {
		arguments.resize(count);
		load_into();
	}
	return arguments;
}

} // namespace ivm::napi
