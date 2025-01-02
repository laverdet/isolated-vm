module;
#include <cstdint>
module ivm.napi;
import :number;
import :utility;
import ivm.js;
import napi;

namespace ivm::js::napi {

auto factory<number_tag>::operator()(double number) const -> value<number_tag_of<double>> {
	return value<number_tag_of<double>>::from(js::napi::invoke(napi_create_double, env(), number));
}

auto factory<number_tag>::operator()(int32_t number) const -> value<number_tag_of<int32_t>> {
	return value<number_tag_of<int32_t>>::from(js::napi::invoke(napi_create_int32, env(), number));
}

auto factory<number_tag>::operator()(int64_t number) const -> value<number_tag_of<int64_t>> {
	return value<number_tag_of<int64_t>>::from(js::napi::invoke(napi_create_int64, env(), number));
}

auto factory<number_tag>::operator()(uint32_t number) const -> value<number_tag_of<uint32_t>> {
	return value<number_tag_of<uint32_t>>::from(js::napi::invoke(napi_create_uint32, env(), number));
}

auto factory<bigint_tag>::operator()(int64_t number) const -> value<bigint_tag_of<int64_t>> {
	return value<bigint_tag_of<int64_t>>::from(js::napi::invoke(napi_create_bigint_int64, env(), number));
}

auto factory<bigint_tag>::operator()(uint64_t number) const -> value<bigint_tag_of<uint64_t>> {
	return value<bigint_tag_of<uint64_t>>::from(js::napi::invoke(napi_create_bigint_uint64, env(), number));
}

} // namespace ivm::js::napi
