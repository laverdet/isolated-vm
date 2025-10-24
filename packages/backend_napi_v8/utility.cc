module;
#include <expected>
#include <string_view>
#include <utility>
export module backend_napi_v8:utility;
import :environment;
import isolated_js;
import ivm.utility;
import napi_js;
using namespace util::string_literals;

namespace backend_napi_v8 {
using namespace js;

template <util::string_literal Key>
auto make_property_key(environment& env) {
	auto& storage = env.global_storage(util::value_constant<Key>{});
	if (storage) {
		return napi_value{storage.get(env)};
	} else {
		auto value = napi::value<string_tag>::make_property_name(env, std::string_view{Key});
		storage.reset(env, value);
		return napi_value{value};
	}
}

export template <class Expect, class Unexpect>
auto make_completion_record(environment& env, std::expected<Expect, Unexpect> result) {
	auto* record = napi::invoke(napi_create_object, napi_env{env});
	auto* completed_key = make_property_key<"complete">(env);
	if (result) {
		js::napi::invoke0(napi_set_property, napi_env{env}, record, completed_key, js::napi::invoke(napi_get_boolean, napi_env{env}, true));
		auto* result_key = make_property_key<"result">(env);
		js::napi::invoke0(napi_set_property, napi_env{env}, record, result_key, js::transfer_in<napi_value>(*std::move(result), env));
	} else {
		js::napi::invoke0(napi_set_property, napi_env{env}, record, completed_key, js::napi::invoke(napi_get_boolean, napi_env{env}, false));
		auto* error_key = make_property_key<"error">(env);
		js::napi::invoke0(napi_set_property, napi_env{env}, record, error_key, js::transfer_in<napi_value>(std::move(result).error(), env));
	}
	return js::forward{record};
}

} // namespace backend_napi_v8
