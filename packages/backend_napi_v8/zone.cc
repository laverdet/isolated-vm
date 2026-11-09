module backend_napi_v8;
import :agent_handle;
import :lock;
import :zone;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto zone_handle::run(const realm_scope& lock, js::iv8::this_object self, js::forward<v8::Local<js::iv8::Function>> callback) -> js::forward<v8::Local<v8::Value>> {
	auto scope = js::iv8::continuation_scope{util::slice(lock), *self};
	return js::forward{(*callback)->invoke(lock, std::span<v8::Local<v8::Value>>{})};
}

auto zone_handle::current(const realm_scope& lock) -> js::forward<v8::Local<v8::Value>> {
	auto* isolate = lock.isolate();
	auto data = isolate->GetContinuationPreservedEmbedderData();
	if (!data.IsEmpty() && data->IsValue() && data.As<v8::Value>()->IsObject()) {
		auto that = data.As<v8::Object>();
		if (js::iv8::wrappable_of<zone_handle>::unwrap(util::slice(lock), that) != nullptr) {
			return js::forward{that};
		}
	}
	return js::forward{v8::Local<v8::Value>{v8::Undefined(isolate)}};
}

auto zone_handle::class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<zone_handle>> {
	auto constructor = [](const realm_scope& /*lock*/) -> zone_handle { return zone_handle{}; };
	return lock->environment().class_template(
		std::type_identity<zone_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Zone">, util::fn<+constructor>},
			js::class_method{util::cw<"run">, util::fn<&zone_handle::run>},
			js::class_static_getter{util::cw<"current">, util::fn<&zone_handle::current>},
		}
	);
}

} // namespace backend_napi_v8
