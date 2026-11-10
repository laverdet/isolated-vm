export module backend_napi_v8:zone;
import :lock;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// `Zone` constructor & `Zone.detach` options
struct zone_options : js::optional_constructible {
		using js::optional_constructible::optional_constructible;
		std::optional<double> timeout;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"timeout">, &zone_options::timeout},
		};
};

// Runtime support for promise-tracking context data and timeout budget
export class zone_handle {
	public:
		using timeout_type = js::iv8::isolated::executor::timeout;
		zone_handle(const realm_scope& lock, std::optional<double> timeout);
		zone_handle(js::iv8::isolate_lock_witness lock, std::shared_ptr<timeout_type> timeout, v8::Local<v8::Map> data);
		auto trace(cppgc::Visitor& visitor) const -> void { visitor.Trace(data_); }

		[[nodiscard]] auto timeout_timer() const -> timeout_type* { return timeout_.get(); }
		[[nodiscard]] auto timeout() const noexcept -> std::optional<double>;
		auto adjust_timeout(double delta) -> bool;
		auto apply_timeout(double timeout) -> bool;
		auto clone(const realm_scope& lock) -> js::forward<v8::Local<v8::Object>>;
		auto data(const realm_scope& lock) -> js::forward<v8::Local<v8::Object>>;
		auto run(const realm_scope& lock, js::iv8::this_object self, js::forward<v8::Local<js::iv8::Function>> callback) -> js::forward<v8::Local<v8::Value>>;
		static auto current(const realm_scope& lock) -> js::forward<v8::Local<v8::Value>>;
		static auto detach(const realm_scope& lock, js::forward<v8::Local<js::iv8::Function>> callback, zone_options options) -> void;
		static auto class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<zone_handle>>;

		static auto promise_hook(v8::PromiseHookType type, v8::Local<v8::Promise> promise, v8::Local<v8::Value> parent) -> void;
		static auto try_cast(js::iv8::isolate_lock_witness lock, v8::Local<v8::Data> data) -> zone_handle*;

	private:
		std::shared_ptr<timeout_type> timeout_;
		v8::TracedReference<v8::Map> data_;
};

// Initializes a new zone for the current scope
class zone_frame : util::non_moveable {
	public:
		zone_frame(const realm_scope& lock, std::optional<double> timeout);

	private:
		zone_frame(const realm_scope& lock, const std::shared_ptr<zone_handle::timeout_type>& budget);

		js::iv8::continuation_scope continuation_;
		zone_handle::timeout_type::scope timeout_;
};

} // namespace backend_napi_v8
