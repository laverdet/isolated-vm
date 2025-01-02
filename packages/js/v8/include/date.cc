module;
#include <type_traits>
export module ivm.iv8:date;
import ivm.value;
import v8;

namespace ivm::iv8 {

using value::js_clock;

export class date : public v8::Date {
	public:
		[[nodiscard]] auto materialize(std::type_identity<js_clock::time_point> tag) const -> js_clock::time_point;
		static auto Cast(v8::Value* data) -> date*;
		static auto make(v8::Local<v8::Context> context, js_clock::time_point date) -> v8::Local<v8::Date>;
};

auto date::Cast(v8::Value* data) -> date* {
	return reinterpret_cast<date*>(v8::Date::Cast(data));
}

auto date::make(v8::Local<v8::Context> context, js_clock::time_point date) -> v8::Local<v8::Date> {
	return v8::Date::New(context, date.time_since_epoch().count()).ToLocalChecked().As<v8::Date>();
}

auto date::materialize(std::type_identity<js_clock::time_point> /*tag*/) const -> js_clock::time_point {
	return js_clock::time_point{js_clock::duration{ValueOf()}};
}

} // namespace ivm::iv8
