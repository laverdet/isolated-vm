module;
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
export module v8_js.visit;
import isolated_js;
import ivm.utility;
import v8_js.array;
import v8_js.boolean;
import v8_js.callback_info;
import v8_js.date;
import v8_js.external;
import v8_js.handle;
import v8_js.lock;
import v8_js.number;
import v8_js.object;
import v8_js.string;
import v8;

// nb: These visitors are split out to share implementations with the napi visitors in the case
// where it is kind of safe to do so. For numbers, strings, most primitives, etc the ABI has been
// pretty stable for a long because v8 seems to have those figured out by now.
namespace js {

// boolean
template <>
struct visit<void, v8::Local<v8::Boolean>> {
		auto operator()(v8::Local<v8::Boolean> value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(boolean_tag{}, iv8::boolean{value});
		}
};

// number
template <>
struct visit<void, v8::Local<v8::Number>> {
		auto operator()(v8::Local<v8::Number> value, const auto_accept auto& accept) const -> decltype(auto) {
			auto number = iv8::number{value};
			if (value->IsInt32()) {
				return accept(number_tag_of<int32_t>{}, number);
			} else {
				return accept(number_tag_of<double>{}, number);
			}
		}
};

// date
template <>
struct visit<void, v8::Local<v8::Date>> {
		auto operator()(v8::Local<v8::Date> value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(date_tag{}, iv8::date{value});
		}
};

// external
template <>
struct visit<void, v8::Local<v8::External>> {
		auto operator()(v8::Local<v8::External> value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(external_tag{}, iv8::external{value});
		}
};

// bigint
template <>
struct visit<void, v8::Local<v8::BigInt>> {
		auto operator()(v8::Local<v8::BigInt> value, const auto_accept auto& accept) const -> decltype(auto) {
			// We actually have to convert the bigint in order to see how big it is. So the acceptor is
			// invoked directly the underlying value, instead of a handle.
			bool lossless{};
			auto i64 = value->Int64Value(&lossless);
			if (lossless) {
				return accept(bigint_tag_of<int64_t>{}, i64);
			}
			auto u64 = value->Uint64Value(&lossless);
			if (lossless) {
				return accept(bigint_tag_of<uint64_t>{}, u64);
			}
			throw std::logic_error{"Bigint is too big"};
		}
};

// name (string + symbol)
template <>
struct visit<void, v8::Local<v8::Name>> {
	public:
		visit(const iv8::isolate_lock& lock) :
				lock_{&lock} {}

		auto witness() const -> const iv8::isolate_lock& { return *lock_; }

		auto operator()(v8::Local<v8::Name> value, const auto_accept auto& accept) const -> decltype(auto) {
			if (value->IsString()) {
				return (*this)(value.As<v8::String>(), accept);
			} else {
				return (*this)(value.As<v8::Symbol>(), accept);
			}
		}

		auto operator()(v8::Local<v8::Symbol> value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(symbol_tag{}, value);
		}

		auto operator()(v8::Local<v8::String> value, const auto_accept auto& accept) const -> decltype(auto) {
			auto string = iv8::string{*lock_, value};
			if (value->IsOneByte()) {
				return accept(string_tag_of<char>{}, string);
			} else {
				return accept(string_tag_of<char16_t>{}, string);
			}
		}

	private:
		const iv8::isolate_lock* lock_;
};

// symbol
template <>
struct visit<void, v8::Local<v8::Symbol>> : visit<void, v8::Local<v8::Name>> {
		using visit<void, v8::Local<v8::Name>>::visit;
};

// string
template <>
struct visit<void, v8::Local<v8::String>> : visit<void, v8::Local<v8::Name>> {
		using visit<void, v8::Local<v8::Name>>::visit;
};

// Primary visitor
template <>
struct visit<void, v8::Local<v8::Value>>
		: visit<void, v8::Local<v8::Boolean>>,
			visit<void, v8::Local<v8::Number>>,
			visit<void, v8::Local<v8::Date>>,
			visit<void, v8::Local<v8::External>>,
			visit<void, v8::Local<v8::BigInt>>,
			visit<void, v8::Local<v8::Name>> {
	public:
		using visit<void, v8::Local<v8::Boolean>>::operator();
		using visit<void, v8::Local<v8::Number>>::operator();
		using visit<void, v8::Local<v8::Date>>::operator();
		using visit<void, v8::Local<v8::External>>::operator();
		using visit<void, v8::Local<v8::BigInt>>::operator();
		using visit<void, v8::Local<v8::Name>>::operator();

		visit(const iv8::context_lock& lock) :
				visit<void, v8::Local<v8::Name>>{lock},
				lock_{&lock} {}

		auto witness() const -> const iv8::context_lock& { return *lock_; }

		auto operator()(v8::Local<v8::Value> value, const auto_accept auto& accept) const -> decltype(auto) {
			if (value->IsObject()) {
				auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
				if (value->IsArray()) {
					return accept(list_tag{}, iv8::object{*lock_, value.As<v8::Object>()}, visit_entry);
				} else if (value->IsExternal()) {
					return (*this)(value.As<v8::External>(), accept);
				} else if (value->IsDate()) {
					return (*this)(value.As<v8::Date>(), accept);
				} else if (value->IsPromise()) {
					return accept(promise_tag{}, value);
				}
				return accept(dictionary_tag{}, iv8::object{*lock_, value.As<v8::Object>()}, visit_entry);
			} else if (value->IsNullOrUndefined()) {
				if (value->IsNull()) {
					return accept(null_tag{}, nullptr);
				} else {
					return accept(undefined_tag{}, std::monostate{});
				}
			} else if (value->IsNumber()) {
				return (*this)(value.As<v8::Number>(), accept);
			} else if (value->IsString()) {
				return (*this)(value.As<v8::String>(), accept);
			} else if (value->IsBoolean()) {
				return (*this)(value.As<v8::Boolean>(), accept);
			} else {
				return accept(value_tag{}, std::type_identity<void>{});
			}
		}

	private:
		const iv8::context_lock* lock_;
};

// `arguments` visitor
template <>
struct visit<void, v8::FunctionCallbackInfo<v8::Value>> : visit<void, v8::Local<v8::Value>> {
		using visit<void, v8::Local<v8::Value>>::visit;

		auto operator()(v8::FunctionCallbackInfo<v8::Value> info, const auto_accept auto& accept) const -> decltype(auto) {
			const visit<void, v8::Local<v8::Value>>& visit = *this;
			return accept(arguments_tag{}, iv8::callback_info{info}, visit);
		}
};

} // namespace js
