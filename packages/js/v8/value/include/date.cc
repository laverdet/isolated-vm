export module v8_js:date;
import :handle;
import isolated_js;
import v8;

namespace js::iv8 {

export class date : public v8::Local<v8::Date> {
	public:
		explicit date(v8::Local<v8::Date> handle) :
				v8::Local<v8::Date>{handle} {}

		[[nodiscard]] explicit operator js_clock::time_point() const;
		static auto make(v8::Local<v8::Context> context, js_clock::time_point date) -> v8::Local<v8::Date>;
};

// ---

auto date::make(v8::Local<v8::Context> context, js_clock::time_point date) -> v8::Local<v8::Date> {
	return v8::Date::New(context, date.time_since_epoch().count()).ToLocalChecked().As<v8::Date>();
}

date::operator js_clock::time_point() const {
	return js_clock::time_point{js_clock::duration{(*this)->ValueOf()}};
}

} // namespace js::iv8
