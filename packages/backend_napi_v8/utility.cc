module;
#include <expected>
#include <string_view>
#include <tuple>
#include <utility>
export module backend_napi_v8:utility;
import :environment;
import isolated_js;
import ivm.utility;
import napi_js;
using namespace util::string_literals;

namespace backend_napi_v8 {
using namespace js;

export using expected_value = std::expected<napi_value, napi_value>;

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
	auto completed_key = make_property_key<"complete">(env);
	if (result) {
		js::napi::invoke0(napi_set_property, napi_env{env}, record, completed_key, js::napi::invoke(napi_get_boolean, napi_env{env}, true));
		auto result_key = make_property_key<"result">(env);
		js::napi::invoke0(napi_set_property, napi_env{env}, record, result_key, js::transfer_in<napi_value>(*std::move(result), env));
	} else {
		js::napi::invoke0(napi_set_property, napi_env{env}, record, completed_key, js::napi::invoke(napi_get_boolean, napi_env{env}, false));
		auto error_key = make_property_key<"error">(env);
		js::napi::invoke0(napi_set_property, napi_env{env}, record, error_key, js::transfer_in<napi_value>(*std::move(result), env));
	}
	return record;
}

export template <class Accept>
auto make_promise(environment& env, Accept accept) {
	// Make nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	auto* promise = js::napi::invoke(napi_create_promise, napi_env{env}, &deferred);

	// Invoked in napi environment and resolves the deferred
	auto resolve =
		[ accept = std::move(accept),
			env_ptr = napi_env{env},
			deferred ](
			auto&&... args
		) mutable {
			auto& env = environment::get(env_ptr);
			js::napi::handle_scope scope{napi_env{env}};
			env.scheduler().decrement_ref();
			if (auto result = accept(env, std::forward<decltype(args)>(args)...)) {
				js::napi::invoke0(napi_resolve_deferred, napi_env{env}, deferred, std::move(result).value());
			} else {
				js::napi::invoke0(napi_reject_deferred, napi_env{env}, deferred, std::move(result).error());
			}
		};

	// Dispatcher can be invoked in any thread, and schedules the resolution on the node thread.
	auto scheduler = env.scheduler();
	scheduler.increment_ref();
	auto dispatch =
		[ resolve = std::move(resolve),
			scheduler = std::move(scheduler) ](
			auto&&... args
		) mutable
		requires std::invocable<Accept&, environment&, decltype(args)...> {
			scheduler(
				util::make_indirect_moveable_function(
					[ resolve = std::move(resolve),
						... args = std::forward<decltype(args)>(args) ]() mutable {
						resolve(std::forward<decltype(args)>(args)...);
					}
				)
			);
		};

	// `[ dispatch, promise ]`
	return std::make_tuple(std::move(dispatch), promise);
}

} // namespace backend_napi_v8
