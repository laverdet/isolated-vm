module;
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>
export module v8_js:visit;
import :array;
import :boolean;
import :callback_info;
import :date;
import :external;
import :handle;
import :hash;
import :lock;
import :number;
import :object;
import :string;
import isolated_js;
import ivm.utility;
import v8;

namespace js {

// external
template <>
struct visit<void, v8::Local<v8::External>> {
		auto operator()(v8::Local<v8::External> value, const auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, external_tag{}, *this, iv8::external{value});
		}
};

// name (string + symbol)
template <>
struct visit<void, v8::Local<v8::Name>> {
	public:
		explicit visit(const js::iv8::isolate_lock_witness& lock) :
				isolate_lock_{lock} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return isolate_lock_; }

		auto operator()(v8::Local<v8::Name> value, const auto& accept) const -> decltype(auto) {
			if (value->IsString()) {
				return (*this)(value.As<v8::String>(), accept);
			} else {
				return (*this)(value.As<v8::Symbol>(), accept);
			}
		}

		auto operator()(v8::Local<v8::Symbol> value, const auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, symbol_tag{}, *this, value);
		}

		auto operator()(v8::Local<v8::String> value, const auto& accept) const -> decltype(auto) {
			auto string = iv8::string{lock_witness(), value};
			if (value->IsOneByte()) {
				return invoke_accept(accept, string_tag_of<char>{}, *this, string);
			} else {
				return invoke_accept(accept, string_tag_of<char16_t>{}, *this, string);
			}
		}

	private:
		iv8::isolate_lock_witness isolate_lock_;
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

// number
template <>
struct visit<void, v8::Local<v8::Number>> {
		auto operator()(v8::Local<v8::Number> value, const auto& accept) const -> decltype(auto) {
			auto number = iv8::number{value};
			if (value->IsInt32()) {
				return invoke_accept(accept, number_tag_of<int32_t>{}, *this, number);
			} else {
				return invoke_accept(accept, number_tag_of<double>{}, *this, number);
			}
		}
};

// bigint
template <>
struct visit<void, v8::Local<v8::BigInt>> {
		auto operator()(v8::Local<v8::BigInt> value, const auto& accept) const -> decltype(auto) {
			// We actually have to convert the bigint in order to see how big it is. So the acceptor is
			// invoked directly the underlying value, instead of a handle.
			bool lossless{};
			auto u64 = value->Uint64Value(&lossless);
			if (lossless) {
				return invoke_accept(accept, bigint_tag_of<uint64_t>{}, *this, u64);
			}
			js::bigint bigint_words;
			bigint_words.resize_and_overwrite(value->WordCount(), [ & ](auto* words, auto length) {
				if (length > 0) {
					auto int_length = static_cast<int>(length);
					value->ToWordsArray(&bigint_words.sign_bit(), &int_length, words);
				}
				return length;
			});
			return invoke_accept(accept, bigint_tag_of<bigint>{}, *this, std::move(bigint_words));
		}
};

// Primary visitor
template <>
struct visit<void, v8::Local<v8::Value>>
		: visit<void, v8::Local<v8::Name>>,
			visit<void, v8::Local<v8::External>>,
			visit<void, v8::Local<v8::Number>>,
			visit<void, v8::Local<v8::BigInt>> {
	public:
		using visit<void, v8::Local<v8::BigInt>>::operator();
		using visit<void, v8::Local<v8::External>>::operator();
		using visit<void, v8::Local<v8::Name>>::operator();
		using visit<void, v8::Local<v8::Number>>::operator();

		explicit visit(const iv8::context_lock_witness& lock) :
				visit<void, v8::Local<v8::Name>>{lock},
				context_lock_{lock} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return context_lock_; }

		auto operator()(v8::Local<v8::Value> value, const auto& accept) const -> decltype(auto) {
			if (value->IsObject()) {
				auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
				if (value->IsArray()) {
					return invoke_accept(accept, list_tag{}, visit_entry, iv8::object{lock_witness(), value.As<v8::Object>()});
				} else if (value->IsExternal()) {
					return (*this)(value.As<v8::External>(), accept);
				} else if (value->IsDate()) {
					return invoke_accept(accept, date_tag{}, *this, iv8::date{value.As<v8::Date>()});
				} else if (value->IsPromise()) {
					return invoke_accept(accept, promise_tag{}, *this, value);
				} else {
					return invoke_accept(accept, dictionary_tag{}, visit_entry, iv8::object{lock_witness(), value.As<v8::Object>()});
				}
			} else if (value->IsNullOrUndefined()) {
				if (value->IsNull()) {
					return invoke_accept(accept, null_tag{}, *this, nullptr);
				} else {
					return invoke_accept(accept, undefined_tag{}, *this, std::monostate{});
				}
			} else if (value->IsNumber()) {
				return (*this)(value.As<v8::Number>(), accept);
			} else if (value->IsString()) {
				return (*this)(value.As<v8::String>(), accept);
			} else if (value->IsBoolean()) {
				return invoke_accept(accept, boolean_tag{}, *this, iv8::boolean{value.As<v8::Boolean>()});
			} else if (value->IsBigInt()) {
				return (*this)(value.As<v8::BigInt>(), accept);
			} else {
				return invoke_accept(accept, value_tag{}, *this, std::type_identity<void>{});
			}
		}

	private:
		js::iv8::context_lock_witness context_lock_;
};

// `arguments` visitor
template <class Type>
struct visit_subject_for<v8::FunctionCallbackInfo<Type>> : visit_subject_for<v8::Local<Type>> {};

template <>
struct visit<void, v8::FunctionCallbackInfo<v8::Value>> : visit<void, v8::Local<v8::Value>> {
		using visit_type = visit<void, v8::Local<v8::Value>>;
		using visit_type::visit_type;

		using visit_type::operator();
		auto operator()(v8::FunctionCallbackInfo<v8::Value> info, const auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, arguments_tag{}, *this, iv8::callback_info{info});
		}
};

} // namespace js
