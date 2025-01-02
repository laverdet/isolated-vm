module;
#include <cstddef>
#include <variant>
module ivm.napi;
import :value;
import ivm.js;
import napi;

namespace ivm::js::napi {

auto factory<value_tag>::operator()(std::monostate /*undefined*/) const -> value<undefined_tag> {
	return value<undefined_tag>::from(js::napi::invoke(napi_get_undefined, env()));
}

auto factory<value_tag>::operator()(std::nullptr_t /*null*/) const -> value<null_tag> {
	return value<null_tag>::from(js::napi::invoke(napi_get_null, env()));
}

auto factory<undefined_tag>::operator()() const -> value<undefined_tag> {
	return (*this)(std::monostate{});
}

auto factory<null_tag>::operator()() const -> value<null_tag> {
	return (*this)(nullptr);
}

} // namespace ivm::js::napi
