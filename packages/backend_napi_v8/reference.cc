module;
#include <utility>
#include <variant>
module backend_napi_v8;
import :agent;
import :environment;
import :lock;
import :reference;
import :utility;
import auto_js;
import napi_js;
import v8_js;

namespace backend_napi_v8 {

reference_handle::reference_handle(js::null_tag /*tag*/) :
		agent_{nullptr},
		typeof_{js::typeof_kind::null} {}

reference_handle::reference_handle(js::undefined_tag /*tag*/) :
		agent_{nullptr},
		typeof_{js::typeof_kind::undefined} {}

reference_handle::reference_handle(
	agent_handle agent,
	js::typeof_kind typeof,
	js::iv8::shared_remote<v8::Context> realm,
	js::iv8::shared_remote<v8::Value> value
) :
		agent_{std::move(agent)},
		realm_{std::move(realm)},
		value_{std::move(value)},
		typeof_{typeof} {}

reference_handle::reference_handle(const agent_handle::lock& lock, agent_handle agent, js::iv8::shared_remote<v8::Context> realm, v8::Local<v8::Value> value) :
		reference_handle{util::elide{[ & ]() -> reference_handle {
			if (value->IsObject()) {
				return reference_handle{lock, std::move(agent), std::move(realm), value.As<v8::Object>()};
			} else {
				const auto typeof = [ & ]() -> js::typeof_kind {
					if (value->IsNullOrUndefined()) {
						if (value->IsNull()) {
							return js::typeof_kind::null;
						} else {
							return js::typeof_kind::undefined;
						}
					} else if (value->IsNumber()) {
						return js::typeof_kind::number;
					} else if (value->IsName()) {
						if (value->IsString()) {
							return js::typeof_kind::string;
						} else {
							return js::typeof_kind::symbol;
						}
					} else if (value->IsBoolean()) {
						return js::typeof_kind::boolean;
					} else if (value->IsBigInt()) {
						return js::typeof_kind::bigint;
					} else {
						std::unreachable();
					}
				}();
				switch (typeof) {
					case js::typeof_kind::null:
						return reference_handle{js::null_tag{}};
					case js::typeof_kind::undefined:
						return reference_handle{js::undefined_tag{}};
					default:
						return reference_handle{std::move(agent), typeof, std::move(realm), js::iv8::make_shared_remote(lock, value)};
				}
			}
		}}} {}

reference_handle::reference_handle(const agent_handle::lock& lock, agent_handle agent, js::iv8::shared_remote<v8::Context> realm, v8::Local<v8::Object> value) :
		reference_handle{util::elide{[ & ]() -> reference_handle {
			const auto typeof = [ & ]() -> js::typeof_kind {
				if (value->IsFunction()) {
					return js::typeof_kind::function;
				} else {
					return js::typeof_kind::object;
				}
			}();
			return reference_handle{std::move(agent), typeof, std::move(realm), js::iv8::make_shared_remote(lock, value.As<v8::Value>())};
		}}} {}

auto reference_handle::copy(environment& env) -> js::forward<js::napi::value<>> {
	auto [ promise, dispatch ] = make_promise(env, [](environment& /*env*/, js::value_t value) -> auto {
		return value;
	});
	switch (typeof_) {
		case js::typeof_kind::null:
			dispatch(js::value_t{std::nullptr_t{}});
			break;

		case js::typeof_kind::undefined:
			dispatch(js::value_t{std::monostate{}});
			break;

		default:
			agent_.schedule(
				[ dispatch = std::move(dispatch),
					value = value_,
					realm = realm_ ](
					const agent_handle::lock& lock
				) -> void {
					auto transferred = context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& lock) -> js::value_t {
						return js::transfer_out<js::value_t>(value->deref(lock), lock);
					});
					dispatch(std::move(transferred));
				}
			);
			break;
	}
	return js::forward{promise};
}

auto reference_handle::get(environment& env, js::string_t name) -> js::forward<js::napi::value<>> {
	auto [ promise, dispatch ] = make_promise(env, [](environment& env, reference_handle reference) -> auto {
		return js::forward{reference_handle::class_template(env).construct(env, std::move(reference))};
	});
	switch (typeof_) {
		// Any property on these types is just `undefined`
		case js::typeof_kind::bigint:
		case js::typeof_kind::boolean:
		case js::typeof_kind::number:
		case js::typeof_kind::string:
		case js::typeof_kind::symbol:
			dispatch(reference_handle{js::undefined_tag{}});
			break;

		// `null` & `undefined` throw
		// TODO: They should throw from the promise
		case js::typeof_kind::undefined:
			throw js::type_error{u"Cannot read properties of undefined"};
		case js::typeof_kind::null:
			throw js::type_error{u"Cannot read properties of null"};

		// `get` only does something on these object types
		case js::typeof_kind::object:
		case js::typeof_kind::function:
			agent_.schedule(
				[ dispatch = std::move(dispatch), value = value_ ](
					const agent_handle::lock& agent_lock,
					js::string_t name,
					agent_handle agent,
					js::iv8::shared_remote<v8::Context> realm
				) -> void {
					auto name_local = js::transfer_in<v8::Local<v8::String>>(std::move(name), agent_lock);
					auto reference = context_scope_operation(agent_lock, realm->deref(agent_lock), [ & ](const realm_scope& lock) -> reference_handle {
						auto local = value->deref(lock).As<v8::Object>();
						if (js::iv8::unmaybe(local->HasRealNamedProperty(lock.context(), name_local))) {
							auto property = js::iv8::unmaybe(local->GetRealNamedProperty(lock.context(), name_local));
							return reference_handle{agent_lock, std::move(agent), std::move(realm), property};
						} else {
							return reference_handle{js::undefined_tag{}};
						}
					});
					dispatch(std::move(reference));
				},
				std::move(name),
				agent_,
				realm_
			);
			break;
	}

	return js::forward{promise};
}

auto reference_handle::class_template(environment& env) -> js::napi::value<class_tag_of<reference_handle>> {
	return env.class_template(
		std::type_identity<reference_handle>{},
		js::class_template{
			js::class_constructor{util::cw<u8"Reference">},
			js::class_method{util::cw<u8"copy">, util::fn<&reference_handle::copy>},
			js::class_method{util::cw<u8"get">, util::fn<&reference_handle::get>},
		}
	);
}

} // namespace backend_napi_v8
