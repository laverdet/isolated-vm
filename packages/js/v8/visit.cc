module;
#include <cstdint>
#include <type_traits>
#include <utility>
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

namespace js {

// external
template <>
struct visit<void, v8::Local<v8::External>> {
		auto operator()(v8::Local<v8::External> value, const auto& accept) const -> decltype(auto) {
			return accept(external_tag{}, iv8::external{value});
		}
};

// name (string + symbol)
template <>
struct visit<void, v8::Local<v8::Name>> {
	public:
		explicit visit(const iv8::isolate_lock& lock) :
				lock_{&lock} {}

		auto operator()(v8::Local<v8::Name> value, const auto& accept) const -> decltype(auto) {
			if (value->IsString()) {
				return (*this)(value.As<v8::String>(), accept);
			} else {
				return (*this)(value.As<v8::Symbol>(), accept);
			}
		}

		auto operator()(v8::Local<v8::Symbol> value, const auto& accept) const -> decltype(auto) {
			return accept(symbol_tag{}, value);
		}

		auto operator()(v8::Local<v8::String> value, const auto& accept) const -> decltype(auto) {
			auto string = iv8::string{*lock_, value};
			if (value->IsOneByte()) {
				return accept(string_tag_of<std::byte>{}, string);
			} else {
				return accept(string_tag{}, string);
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
		: visit<void, v8::Local<v8::External>>,
			visit<void, v8::Local<v8::Name>> {
	public:
		using visit<void, v8::Local<v8::External>>::operator();
		using visit<void, v8::Local<v8::Name>>::operator();

		explicit visit(const iv8::context_lock& lock) :
				visit<void, v8::Local<v8::Name>>{lock},
				lock_{&lock} {}

		auto witness() const -> const iv8::context_lock& { return *lock_; }

		auto operator()(v8::Local<v8::Value> value, const auto& accept) const -> decltype(auto) {
			if (value->IsObject()) {
				auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
				if (value->IsArray()) {
					return accept(list_tag{}, iv8::object{*lock_, value.As<v8::Object>()}, visit_entry);
				} else if (value->IsExternal()) {
					return (*this)(value.As<v8::External>(), accept);
				} else if (value->IsDate()) {
					return accept(date_tag{}, iv8::date{value.As<v8::Date>()});
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
				return accept(boolean_tag{}, iv8::boolean{value.As<v8::Boolean>()});
			} else if (value->IsBigInt()) {
				return (*this)(value.As<v8::BigInt>(), accept);
			} else {
				return accept(value_tag{}, std::type_identity<void>{});
			}
		}

		// number
		auto operator()(v8::Local<v8::Number> value, const auto& accept) const -> decltype(auto) {
			auto number = iv8::number{value};
			if (value->IsInt32()) {
				return accept(number_tag_of<int32_t>{}, number);
			} else {
				return accept(number_tag{}, number);
			}
		}

		// bigint
		auto operator()(v8::Local<v8::BigInt> value, const auto& accept) const -> decltype(auto) {
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
			js::bigint bigint_words;
			bigint_words.resize_and_overwrite(value->WordCount(), [ & ](auto* words, auto length) {
				if (length > 0) {
					int int_length = static_cast<int>(length);
					value->ToWordsArray(&bigint_words.sign_bit(), &int_length, words);
				}
				return length;
			});
			return accept(bigint_tag{}, std::move(bigint_words));
		}

	private:
		const iv8::context_lock* lock_;
};

// `arguments` visitor
template <>
struct visit<void, v8::FunctionCallbackInfo<v8::Value>> : visit<void, v8::Local<v8::Value>> {
		using visit<void, v8::Local<v8::Value>>::visit;

		auto operator()(v8::FunctionCallbackInfo<v8::Value> info, const auto& accept) const -> decltype(auto) {
			const visit<void, v8::Local<v8::Value>>& visitor = *this;
			return accept(arguments_tag{}, iv8::callback_info{info}, visitor);
		}
};

} // namespace js
