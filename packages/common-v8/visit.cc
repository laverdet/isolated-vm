module;
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
export module ivm.v8:visit;
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

// string
template <>
struct visit<iv8::handle<iv8::string>> {
		auto operator()(iv8::handle<iv8::string> value, const auto& accept) const -> decltype(auto) {
			if (value->IsOneByte()) {
				return accept(string_tag_of<std::string>{}, value);
			} else {
				return accept(string_tag_of<std::u16string>{}, value);
			}
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

// external
template <>
struct visit<v8::Local<v8::External>> {
		auto operator()(v8::Local<v8::External> value, const auto& accept) const -> decltype(auto) {
			return accept(external_tag{}, iv8::handle_cast{value.As<iv8::external>()});
		}
};

// Primary visitor
template <>
struct visit<iv8::handle<v8::Value>> {
		auto operator()(iv8::handle<v8::Value> value, auto&& accept) const -> decltype(auto) {
			if (value->IsObject()) {
				if (value->IsArray()) {
					return accept(list_tag{}, iv8::object_handle{value.to<iv8::object>()});
				} else if (value->IsExternal()) {
					return invoke_visit(value.As<v8::External>(), std::forward<decltype(accept)>(accept));
				} else if (value->IsDate()) {
					return invoke_visit(value.As<v8::Date>(), std::forward<decltype(accept)>(accept));
				} else if (value->IsPromise()) {
					return accept(promise_tag{}, value);
				}
				return accept(dictionary_tag{}, iv8::object_handle{value.to<iv8::object>()});
			} else if (value->IsNullOrUndefined()) {
				if (value->IsNull()) {
					return accept(null_tag{}, std::nullptr_t{});
				} else {
					return accept(undefined_tag{}, std::monostate{});
				}
			} else if (value->IsNumber()) {
				return invoke_visit(value.As<v8::Number>(), std::forward<decltype(accept)>(accept));
			} else if (value->IsString()) {
				return invoke_visit(value.to<iv8::string>(), std::forward<decltype(accept)>(accept));
			} else if (value->IsBoolean()) {
				return invoke_visit(value.As<v8::Boolean>(), std::forward<decltype(accept)>(accept));
			} else {
				return accept(value_tag{}, std::type_identity<void>{});
			}
		}
};

} // namespace ivm::value
