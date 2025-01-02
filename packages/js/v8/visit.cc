module;
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
export module ivm.iv8:visit;
import :array;
import :boolean;
import :date;
import :external;
import :handle;
import :number;
import :object;
import :string;
import ivm.js;
import ivm.utility;
import v8;

// nb: These visitors are split out to share implementations with the napi visitors in the case
// where it is kind of safe to do so. For numbers, strings, most primitives, etc the ABI has been
// pretty stable for a long because v8 seems to have those figured out by now.
namespace ivm::js {

// boolean
template <>
struct visit<void, v8::Local<v8::Boolean>> {
		auto operator()(v8::Local<v8::Boolean> value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(boolean_tag{}, iv8::handle_cast{value.As<iv8::boolean>()});
		}
};

// number
template <>
struct visit<void, v8::Local<v8::Number>> {
		auto operator()(v8::Local<v8::Number> value, const auto_accept auto& accept) const -> decltype(auto) {
			auto number = iv8::handle_cast{value.As<iv8::number>()};
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
			return accept(date_tag{}, iv8::handle_cast{value.As<iv8::date>()});
		}
};

// external
template <>
struct visit<void, v8::Local<v8::External>> {
		auto operator()(v8::Local<v8::External> value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(external_tag{}, iv8::handle_cast{value.As<iv8::external>()});
		}
};

// bigint
template <>
struct visit<void, v8::Local<v8::BigInt>> {
		auto operator()(v8::Local<v8::BigInt> value, const auto_accept auto& accept) const -> decltype(auto) {
			// We actually have to convert the bigint in order to see how big it is. So the acceptor is
			// invoked directly the underlying value, instead of a `handle_cast`.
			bool lossless{};
			auto i64 = value->Int64Value(&lossless);
			if (lossless) {
				return accept(bigint_tag_of<int64_t>{}, i64);
			}
			auto u64 = value->Uint64Value(&lossless);
			if (lossless) {
				return accept(bigint_tag_of<uint64_t>{}, u64);
			}
			throw std::logic_error("Bigint is too big");
		}
};

// Primary visitor
template <>
struct visit<void, v8::Local<v8::Value>>
		: visit<void, v8::Local<v8::Boolean>>,
			visit<void, v8::Local<v8::Number>>,
			visit<void, v8::Local<v8::Date>>,
			visit<void, v8::Local<v8::External>>,
			visit<void, v8::Local<v8::BigInt>> {
	public:
		using visit<void, v8::Local<v8::Boolean>>::operator();
		using visit<void, v8::Local<v8::Number>>::operator();
		using visit<void, v8::Local<v8::Date>>::operator();
		using visit<void, v8::Local<v8::External>>::operator();
		using visit<void, v8::Local<v8::BigInt>>::operator();

		visit(v8::Isolate* isolate, v8::Local<v8::Context> context) :
				isolate_{isolate},
				context_{context} {}

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
			auto string = iv8::handle{value.As<iv8::string>(), {isolate_, context_}};
			if (value->IsOneByte()) {
				return accept(string_tag_of<std::string>{}, string);
			} else {
				return accept(string_tag_of<std::u16string>{}, string);
			}
		}

		auto operator()(v8::Local<v8::Value> value, const auto_accept auto& accept) const -> decltype(auto) {
			if (value->IsObject()) {
				auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
				if (value->IsArray()) {
					return accept(list_tag{}, iv8::object_handle{value.As<iv8::object>(), {isolate_, context_}}, visit_entry);
				} else if (value->IsExternal()) {
					return (*this)(value.As<v8::External>(), accept);
				} else if (value->IsDate()) {
					return (*this)(value.As<v8::Date>(), accept);
				} else if (value->IsPromise()) {
					return accept(promise_tag{}, value);
				}
				return accept(dictionary_tag{}, iv8::object_handle{value.As<iv8::object>(), {isolate_, context_}}, visit_entry);
			} else if (value->IsNullOrUndefined()) {
				if (value->IsNull()) {
					return accept(null_tag{}, std::nullptr_t{});
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
		v8::Isolate* isolate_;
		v8::Local<v8::Context> context_;
};

// name (string + symbol)
template <>
struct visit<void, v8::Local<v8::Name>> : visit<void, v8::Local<v8::Value>> {
		using visit<void, v8::Local<v8::Value>>::visit;
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

} // namespace ivm::js
