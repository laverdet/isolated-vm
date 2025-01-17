module;
#include <type_traits>
export module v8_js.date;
import isolated_js;
import v8_js.handle;
import v8;

namespace js::iv8 {

export class date
		: public v8::Local<v8::Date>,
			public handle_materializable<date> {
	public:
		explicit date(v8::Local<v8::Date> handle) :
				Local{handle} {}

		[[nodiscard]] auto materialize(std::type_identity<js_clock::time_point> tag) const -> js_clock::time_point;
		static auto make(v8::Local<v8::Context> context, js_clock::time_point date) -> v8::Local<v8::Date>;
};

// ---

auto date::make(v8::Local<v8::Context> context, js_clock::time_point date) -> v8::Local<v8::Date> {
	return v8::Date::New(context, date.time_since_epoch().count()).ToLocalChecked().As<v8::Date>();
}

auto date::materialize(std::type_identity<js_clock::time_point> /*tag*/) const -> js_clock::time_point {
	return js_clock::time_point{js_clock::duration{(*this)->ValueOf()}};
}

} // namespace js::iv8
