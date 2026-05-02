export module v8_js:primitive;
import :handle;
import auto_js;
import std;
import v8;

namespace js::iv8 {

class value_for_boolean : public handle_without_lock<v8::Boolean> {
	public:
		using handle_without_lock<v8::Boolean>::handle_without_lock;
		[[nodiscard]] explicit operator bool() const;
};

class value_for_number : public handle_without_lock<v8::Number> {
	public:
		using handle_without_lock<v8::Number>::handle_without_lock;
		[[nodiscard]] explicit operator double() const;
		[[nodiscard]] explicit operator std::int32_t() const;
		[[nodiscard]] explicit operator std::int64_t() const;
		[[nodiscard]] explicit operator std::uint32_t() const;
};

class value_for_bigint : public handle_without_lock<v8::BigInt> {
	public:
		using handle_without_lock<v8::BigInt>::handle_without_lock;
		[[nodiscard]] explicit operator js::bigint() const;
};

class value_for_bigint_i64 : public value_for_bigint {
	public:
		explicit value_for_bigint_i64(null_lock_witness lock, v8::Local<v8::BigInt> handle, std::int64_t value) :
				value_for_bigint{lock, handle},
				value_{value} {}

		[[nodiscard]] explicit operator std::int64_t() const;

	private:
		std::int64_t value_;
};

class value_for_string : public handle_with_isolate<v8::String> {
	public:
		using handle_with_isolate<v8::String>::handle_with_isolate;
		[[nodiscard]] explicit operator std::string() const;
		[[nodiscard]] explicit operator std::u8string() const;
		[[nodiscard]] explicit operator std::u16string() const;
};

class value_for_symbol : public handle_without_lock<v8::Symbol> {
	public:
		using handle_without_lock<v8::Symbol>::handle_without_lock;
};

class value_for_date : public v8::Local<v8::Date> {
	public:
		explicit value_for_date(null_lock_witness /*witness*/, v8::Local<v8::Date> handle) :
				v8::Local<v8::Date>{handle} {}

		[[nodiscard]] explicit operator js_clock::time_point() const;
};

class value_for_external : public v8::Local<v8::External> {
	public:
		explicit value_for_external(null_lock_witness /*witness*/, v8::Local<v8::External> handle) :
				v8::Local<v8::External>{handle} {}

		[[nodiscard]] explicit operator void*() const;
};

} // namespace js::iv8
