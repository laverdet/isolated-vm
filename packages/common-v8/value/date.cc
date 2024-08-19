module;
#include <type_traits>
export module ivm.v8:date;
import :utility;
import ivm.value;
import v8;

namespace ivm::iv8 {

export class date : public v8::Date {
	public:
		[[nodiscard]] auto materialize(std::type_identity<value::date_t> tag) const -> value::date_t;
		static auto Cast(v8::Value* data) -> date*;
		static auto make(v8::Local<v8::Context> context, value::date_t date) -> v8::Local<v8::Date>;
};

auto date::Cast(v8::Value* data) -> date* {
	return reinterpret_cast<date*>(v8::Date::Cast(data));
}

auto date::make(v8::Local<v8::Context> context, value::date_t date) -> v8::Local<v8::Date> {
	return unmaybe(v8::Date::New(context, date.count())).As<v8::Date>();
}

auto date::materialize(std::type_identity<value::date_t> /*tag*/) const -> value::date_t {
	return value::date_t{ValueOf()};
}

} // namespace ivm::iv8
