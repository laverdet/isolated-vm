module;
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

namespace ivm::value {

// Specialized v8 number visitor
template <>
struct visit<v8::Local<v8::Number>> {
		auto operator()(v8::Local<v8::Number> value, auto accept) -> decltype(auto) {
			auto number = iv8::handle_cast{value.As<iv8::number>()};
			if (value->IsInt32()) {
				return accept(numeric_tag{}, make_only_cast<int32_t>(number));
			} else {
				return accept(numeric_tag{}, make_only_cast<double>(number));
			}
		}
};

// Specialized v8 string visitor
template <>
struct visit<iv8::handle<iv8::string>> {
		auto operator()(iv8::handle<iv8::string> value, auto accept) -> decltype(auto) {
			if (value->IsOneByte()) {
				return accept(string_tag{}, make_only_cast<std::string>(value));
			} else {
				return accept(string_tag{}, make_only_cast<std::u16string>(value));
			}
		}
};

// Specialized v8 object visitor
template <>
struct visit<iv8::object_handle> {
		auto operator()(iv8::object_handle value, auto&& accept) -> decltype(auto) {
			if (value->IsArray()) {
				return accept(list_tag{}, value);
			} else if (value->IsExternal()) {
				return invoke_visit(value.As<v8::External>(), std::forward<decltype(accept)>(accept));
			} else if (value->IsDate()) {
				return accept(date_tag{}, iv8::handle_cast{value.As<iv8::date>()});
			} else if (value->IsPromise()) {
				return accept(promise_tag{}, value);
			}
			return accept(dictionary_tag{}, value);
		}
};

// Specialized v8 external visitor
template <>
struct visit<v8::Local<v8::External>> {
		auto operator()(v8::Local<v8::External> value, auto accept) -> decltype(auto) {
			return accept(external_tag{}, iv8::handle_cast{value.As<iv8::external>()});
		}
};

// Visitor for any v8 value, `v8::Value`
template <>
struct visit<iv8::handle<v8::Value>> {
		auto operator()(iv8::handle<v8::Value> value, auto&& accept) -> decltype(auto) {
			if (value->IsObject()) {
				return invoke_visit(iv8::object_handle{value.to<iv8::object>()}, std::forward<decltype(accept)>(accept));
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
				return accept(boolean_tag{}, iv8::handle_cast{value.As<iv8::boolean>()});
			} else {
				return accept(value_tag{}, std::type_identity<void>{});
			}
		}
};

} // namespace ivm::value
