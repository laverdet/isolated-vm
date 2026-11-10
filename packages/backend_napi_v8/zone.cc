module backend_napi_v8;
import :agent_handle;
import :lock;
import :zone;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

using duration_type = zone_handle::timeout_type::duration_type;

// Convert JS double milliseconds to `duration_type`, saturating at the representable range. NaN
// saturates to the maximum.
auto to_duration(double timeout) -> duration_type {
	constexpr auto max = js::js_clock::duration{duration_type::max()}.count();
	constexpr auto min = js::js_clock::duration{duration_type::min()}.count();
	if (!(timeout < max)) {
		return duration_type::max();
	} else if (!(timeout > min)) {
		return duration_type::min();
	} else {
		return std::chrono::ceil<duration_type>(js::js_clock::duration{timeout});
	}
}

// Create a timeout object from a JS timeout, if any.
auto make_timeout(const realm_scope& lock, std::optional<double> timeout) -> std::shared_ptr<zone_handle::timeout_type> {
	constexpr auto limit = js::js_clock::duration{duration_type::max()}.count();
	if (!timeout || !(*timeout < limit)) {
		return {};
	}
	return std::make_shared<zone_handle::timeout_type>(lock->executor(), to_duration(*timeout));
}

// zone_handle
zone_handle::zone_handle(const realm_scope& lock, std::optional<double> timeout) :
		timeout_{make_timeout(lock, timeout)},
		data_{lock.isolate(), v8::Map::New(lock.isolate())} {}

zone_handle::zone_handle(js::iv8::isolate_lock_witness lock, std::shared_ptr<timeout_type> timeout, v8::Local<v8::Map> data) :
		timeout_{std::move(timeout)},
		data_{lock.isolate(), data} {}

auto zone_handle::clone(const realm_scope& lock) -> js::forward<v8::Local<v8::Object>> {
	auto* isolate = lock.isolate();
	auto snapshot = v8::Map::New(isolate);
	// `v8::Map` accessors require execution scope
	auto allow_js = v8::Isolate::AllowJavascriptExecutionScope{isolate};
	for (auto [ key, value ] : util::into_range(js::iv8::value_for_map{lock, data_.Get(isolate)})) {
		js::iv8::unmaybe(snapshot->Set(lock.context(), key, value));
	}
	return js::forward{class_template(lock)->construct(lock, util::slice(lock), timeout_, snapshot)};
}

auto zone_handle::data(const realm_scope& lock) -> js::forward<v8::Local<v8::Object>> {
	return js::forward{v8::Local<v8::Object>{data_.Get(lock.isolate())}};
}

auto zone_handle::run(const realm_scope& lock, js::iv8::this_object self, js::forward<v8::Local<js::iv8::Function>> callback) -> js::forward<v8::Local<v8::Value>> {
	auto continuation = js::iv8::continuation_scope{util::slice(lock), *self};
	auto timeout_scope = timeout_type::scope{lock->executor(), timeout_.get()};
	return js::forward{(*callback)->invoke(lock, std::span<v8::Local<v8::Value>>{})};
}

auto zone_handle::timeout() const noexcept -> std::optional<double> {
	return timeout_ ? std::optional{js::js_clock::duration{timeout_->remaining()}.count()} : std::nullopt;
}

auto zone_handle::adjust_timeout(double delta) -> bool {
	return timeout_ && timeout_->adjust(to_duration(delta));
}

auto zone_handle::apply_timeout(double timeout) -> bool {
	return timeout_ && timeout_->set(to_duration(timeout));
}

auto zone_handle::current(const realm_scope& lock) -> js::forward<v8::Local<v8::Value>> {
	auto data = lock.isolate()->GetContinuationPreservedEmbedderData();
	if (try_cast(util::slice(lock), data) != nullptr) {
		return js::forward{data.As<v8::Value>()};
	}
	return js::forward{v8::Local<v8::Value>{v8::Undefined(lock.isolate())}};
}

auto zone_handle::detach(const realm_scope& lock, js::forward<v8::Local<js::iv8::Function>> callback, zone_options options) -> void {
	auto agent = agent_handle{*lock};
	agent.schedule(
		[ realm = get_context_shared_remote(lock),
			callback = js::iv8::make_shared_remote(lock, *callback),
			timeout = options.timeout ](const agent_handle::lock& agent) -> void {
			// TODO: Errors thrown by a detached task have nowhere to go, and are dropped
			std::ignore = realm_scope_operation(agent, realm, [ & ](const realm_scope& lock) -> auto {
				auto frame = zone_frame{lock, timeout};
				return js::iv8::invoke_externalized_error_scope(lock, [ & ] -> std::monostate {
					std::ignore = callback->deref(lock)->invoke(lock, std::span<v8::Local<v8::Value>>{});
					return {};
				});
			});
		}
	);
}

auto zone_handle::try_cast(js::iv8::isolate_lock_witness lock, v8::Local<v8::Data> data) -> zone_handle* {
	if (!data.IsEmpty() && data->IsValue() && data.As<v8::Value>()->IsObject()) {
		return js::iv8::wrappable_of<zone_handle>::unwrap(lock, data.As<v8::Object>());
	}
	return nullptr;
}

auto zone_handle::class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<zone_handle>> {
	auto constructor = [](const realm_scope& lock, zone_options options) -> zone_handle {
		return zone_handle{lock, options.timeout};
	};
	return lock->environment().class_template(
		std::type_identity<zone_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Zone">, util::fn<+constructor>},
			js::class_getter{util::cw<"data">, util::fn<&zone_handle::data>},
			js::class_getter{util::cw<"timeout">, util::fn<&zone_handle::timeout>},
			js::class_method{util::cw<"adjustTimeout">, util::fn<&zone_handle::adjust_timeout>},
			js::class_method{util::cw<"applyTimeout">, util::fn<&zone_handle::apply_timeout>},
			js::class_method{util::cw<"clone">, util::fn<&zone_handle::clone>},
			js::class_method{util::cw<"run">, util::fn<&zone_handle::run>},
			js::class_static_getter{util::cw<"current">, util::fn<&zone_handle::current>},
			js::class_static{util::cw<"detach">, util::fn<&zone_handle::detach>},
		}
	);
}

// zone_frame
zone_frame::zone_frame(const realm_scope& lock, std::optional<double> timeout) :
		zone_frame{lock, make_timeout(lock, timeout)} {}

zone_frame::zone_frame(const realm_scope& lock, const std::shared_ptr<zone_handle::timeout_type>& budget) :
		continuation_{util::slice(lock), zone_handle::class_template(lock)->construct(lock, util::slice(lock), budget, v8::Map::New(lock.isolate()))},
		timeout_{lock->executor(), budget.get()} {}

auto zone_handle::promise_hook(v8::PromiseHookType type, v8::Local<v8::Promise> /*promise*/, v8::Local<v8::Value> /*parent*/) -> void {
	thread_local auto reaction_scope = std::optional<timeout_type::scope>{};
	switch (type) {
		case v8::PromiseHookType::kBefore:
			{
				auto* isolate = v8::Isolate::GetCurrent();
				auto& executor = js::iv8::isolated::agent_host::get_current(isolate).executor();
				auto lock = js::iv8::isolate_lock_witness::make_witness(isolate);
				auto* zone = try_cast(lock, isolate->GetContinuationPreservedEmbedderData());
				reaction_scope.emplace(executor, zone == nullptr ? nullptr : zone->timeout_timer());
				break;
			}
		case v8::PromiseHookType::kAfter:
			reaction_scope.reset();
			break;
		default:
			break;
	}
}

} // namespace backend_napi_v8
