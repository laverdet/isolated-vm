export module v8_js:primitive;
import :handle;
import :lock;
import auto_js;
import std;
import v8;

namespace js::iv8 {

export class boolean : public v8::Local<v8::Boolean> {
	public:
		explicit boolean(v8::Local<v8::Boolean> handle) :
				v8::Local<v8::Boolean>{handle} {}

		[[nodiscard]] explicit operator bool() const;
};

export class number : public v8::Local<v8::Number> {
	public:
		explicit number(v8::Local<v8::Number> handle) :
				v8::Local<v8::Number>{handle} {}

		[[nodiscard]] explicit operator double() const;
		[[nodiscard]] explicit operator std::int32_t() const;
		[[nodiscard]] explicit operator std::int64_t() const;
		[[nodiscard]] explicit operator std::uint32_t() const;
};

export class bigint : public v8::Local<v8::BigInt> {
	public:
		explicit bigint(v8::Local<v8::BigInt> handle) :
				v8::Local<v8::BigInt>{handle} {}

		[[nodiscard]] explicit operator js::bigint() const;
};

export class bigint_u64 : public bigint {
	public:
		explicit bigint_u64(v8::Local<v8::BigInt> handle, std::uint64_t value) :
				bigint{handle},
				value_{value} {}

		[[nodiscard]] explicit operator std::uint64_t() const;

	private:
		std::uint64_t value_;
};

export class string
		: public v8::Local<v8::String>,
			public handle_with_isolate {
	public:
		explicit string(isolate_lock_witness lock, v8::Local<v8::String> handle) :
				v8::Local<v8::String>{handle},
				handle_with_isolate{lock} {}

		[[nodiscard]] explicit operator std::string() const;
		[[nodiscard]] explicit operator std::u8string() const;
		[[nodiscard]] explicit operator std::u16string() const;
};

export class date : public v8::Local<v8::Date> {
	public:
		explicit date(v8::Local<v8::Date> handle) :
				v8::Local<v8::Date>{handle} {}

		[[nodiscard]] explicit operator js_clock::time_point() const;
};

export class external : public v8::Local<v8::External> {
	public:
		explicit external(v8::Local<v8::External> handle) :
				v8::Local<v8::External>{handle} {}

		[[nodiscard]] explicit operator void*() const;
};

} // namespace js::iv8
