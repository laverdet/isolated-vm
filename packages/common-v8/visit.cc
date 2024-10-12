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
import ivm.utility;
import ivm.value;
import v8;

// nb: These visitors are split out to share implementations with the napi visitors in the case
// where it is kind of safe to do so. For numbers, strings, most primitives, etc the ABI has been
// pretty stable for a long because v8 seems to have those figured out by now.
namespace ivm::value {

// boolean
template <>
struct visit<v8::Local<v8::Boolean>> {
		auto operator()(v8::Local<v8::Boolean> value, const auto& accept) const -> decltype(auto) {
			return accept(boolean_tag{}, iv8::handle_cast{value.As<iv8::boolean>()});
		}
};

// number
template <>
struct visit<v8::Local<v8::Number>> {
		auto operator()(v8::Local<v8::Number> value, const auto& accept) const -> decltype(auto) {
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
struct visit<v8::Local<v8::Date>> {
		auto operator()(v8::Local<v8::Date> value, const auto& accept) const -> decltype(auto) {
			return accept(date_tag{}, iv8::handle_cast{value.As<iv8::date>()});
		}
};

// external
template <>
struct visit<v8::Local<v8::External>> {
		auto operator()(v8::Local<v8::External> value, const auto& accept) const -> decltype(auto) {
			return accept(external_tag{}, iv8::handle_cast{value.As<iv8::external>()});
		}
};

// bigint
template <>
struct visit<v8::Local<v8::BigInt>> {
		auto operator()(v8::Local<v8::BigInt> value, const auto& accept) const -> decltype(auto) {
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
struct visit<v8::Local<v8::Value>>
		: visit<v8::Local<v8::Boolean>>,
			visit<v8::Local<v8::Number>>,
			visit<v8::Local<v8::Date>>,
			visit<v8::Local<v8::External>>,
			visit<v8::Local<v8::BigInt>> {
	public:
		using visit<v8::Local<v8::Boolean>>::operator();
		using visit<v8::Local<v8::Number>>::operator();
		using visit<v8::Local<v8::Date>>::operator();
		using visit<v8::Local<v8::External>>::operator();
		using visit<v8::Local<v8::BigInt>>::operator();

		visit(v8::Isolate* isolate, v8::Local<v8::Context> context) :
				isolate_{isolate},
				context_{context} {}
		// TODO: Remove this ASAP
		visit() :
				isolate_{v8::Isolate::GetCurrent()},
				context_{isolate_->GetCurrentContext()} {}

		auto operator()(v8::Local<v8::String> value, const auto& accept) const -> decltype(auto) {
			auto string = iv8::handle{value.As<iv8::string>(), {isolate_, context_}};
			if (value->IsOneByte()) {
				return accept(string_tag_of<std::string>{}, string);
			} else {
				return accept(string_tag_of<std::u16string>{}, string);
			}
		}

		auto operator()(v8::Local<v8::Value> value, const auto& accept) const -> decltype(auto) {
			if (value->IsObject()) {
				if (value->IsArray()) {
					return accept(list_tag{}, iv8::object_handle{value.As<iv8::object>(), {isolate_, context_}});
				} else if (value->IsExternal()) {
					return (*this)(value.As<v8::External>(), accept);
				} else if (value->IsDate()) {
					return (*this)(value.As<v8::Date>(), accept);
				} else if (value->IsPromise()) {
					return accept(promise_tag{}, value);
				}
				return accept(dictionary_tag{}, iv8::object_handle{value.As<iv8::object>(), {isolate_, context_}});
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

// string
template <>
struct visit<v8::Local<v8::String>> : visit<v8::Local<v8::Value>> {
		using visit<v8::Local<v8::Value>>::visit;
};

} // namespace ivm::value
