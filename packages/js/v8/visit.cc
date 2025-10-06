module;
#include <cstdint>
#include <utility>
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

template <class Visit>
struct visit_v8_property_name {
	public:
		explicit visit_v8_property_name(const Visit& visit) : visit_{visit} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return visit_.lock_witness(); }

		auto operator()(v8::Local<v8::Primitive> subject, auto& accept) const -> decltype(auto) {
			return lookup_or_visit(subject, accept, [ & ]() -> decltype(auto) {
				if (subject->IsNumber()) {
					return visit_(subject.As<v8::Number>(), accept);
				} else if (subject->IsName()) {
					// TODO: This looks up in the reference map twice. This needs to use an internalized
					// string factory.
					return visit_(subject.As<v8::Name>(), accept);
				} else {
					std::unreachable();
				}
			});
		}

		[[nodiscard]] auto environment() const -> auto& { return visit_.get().environment(); }

	private:
		std::reference_wrapper<const Visit> visit_;
};

// Primitive-ish value visitor. These only need an isolate, so they are separated from the other
// visitor. Also, none of these are recursive.
struct visit_flat_v8_value {
	protected:
		struct skip_lookup {};

	public:
		using reference_subject_type = v8::Local<v8::Value>;

		explicit visit_flat_v8_value(const js::iv8::isolate_lock_witness& lock) :
				isolate_lock_{lock} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return isolate_lock_; }

		// If the protected `skip_lookup` operation is defined: this public operation will first perform
		// a reference map lookup, then delegate to the protected operation if not found.
		auto operator()(this const auto& self, auto subject, auto& accept) -> decltype(auto)
			requires requires { self(skip_lookup{}, subject, accept); } {
			return lookup_or_visit(subject, accept, [ & ]() -> decltype(auto) {
				return self(skip_lookup{}, subject, accept);
			});
		}

		// numbers
		auto operator()(v8::Local<v8::Number> subject, auto& accept) const -> decltype(auto) {
			auto number = iv8::number{subject};
			if (subject->IsInt32()) {
				return invoke_accept(accept, number_tag_of<int32_t>{}, *this, number);
			} else {
				return invoke_accept(accept, number_tag_of<double>{}, *this, number);
			}
		}

		// boolean
		auto operator()(v8::Local<v8::Boolean> subject, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, boolean_tag{}, *this, iv8::boolean{subject.As<v8::Boolean>()});
		}

	protected:
		// primitives
		auto operator()(skip_lookup skip, v8::Local<v8::Primitive> subject, auto& accept) const -> decltype(auto) {
			if (subject->IsNullOrUndefined()) {
				if (subject->IsNull()) {
					null_ = subject;
					return invoke_accept(accept, null_tag{}, *this, subject);
				} else {
					undefined_ = subject;
					return invoke_accept(accept, undefined_tag{}, *this, subject);
				}
			} else if (subject->IsNumber()) {
				return (*this)(subject.As<v8::Number>(), accept);
			} else if (subject->IsName()) {
				return (*this)(skip, subject.As<v8::Name>(), accept);
			} else if (subject->IsBoolean()) {
				return (*this)(subject.As<v8::Boolean>(), accept);
			} else if (subject->IsBigInt()) {
				return (*this)(skip, subject.As<v8::BigInt>(), accept);
			} else {
				std::unreachable();
			}
		}

		// names
		auto operator()(skip_lookup skip, v8::Local<v8::Name> subject, auto& accept) const -> decltype(auto) {
			if (subject->IsString()) {
				return (*this)(skip, subject.As<v8::String>(), accept);
			} else {
				return (*this)(skip, subject.As<v8::Symbol>(), accept);
			}
		}

		auto operator()(skip_lookup /*skip*/, v8::Local<v8::String> subject, auto& accept) const -> decltype(auto) {
			auto string = iv8::string{lock_witness(), subject};
			if (subject->IsOneByte()) {
				return invoke_accept(accept, string_tag_of<char>{}, *this, string);
			} else {
				return invoke_accept(accept, string_tag_of<char16_t>{}, *this, string);
			}
		}

		auto operator()(skip_lookup /*skip*/, v8::Local<v8::Symbol> subject, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, symbol_tag{}, *this, subject);
		}

		// bigint
		auto operator()(skip_lookup /*skip*/, v8::Local<v8::BigInt> subject, auto& accept) const -> decltype(auto) {
			// We actually have to convert the bigint in order to see how big it is. So the acceptor is
			// invoked directly the underlying value, instead of a handle.
			// TODO: Actually, we only need to check for u64 type
			bool lossless{};
			auto u64 = subject->Uint64Value(&lossless);
			if (lossless) {
				return invoke_accept(accept, bigint_tag_of<uint64_t>{}, *this, u64);
			}
			js::bigint bigint_words;
			bigint_words.resize_and_overwrite(subject->WordCount(), [ & ](auto* words, auto length) {
				if (length > 0) {
					auto int_length = static_cast<int>(length);
					subject->ToWordsArray(&bigint_words.sign_bit(), &int_length, words);
				}
				return length;
			});
			return invoke_accept(accept, bigint_tag_of<bigint>{}, *this, std::move(bigint_words));
		}

		// date
		auto operator()(this const auto& self, skip_lookup /*skip*/, v8::Local<v8::Date> subject, auto& accept) -> decltype(auto) {
			return invoke_accept(accept, date_tag{}, self, iv8::date{subject});
		}

		// external
		auto operator()(this const auto& self, skip_lookup /*skip*/, v8::Local<v8::External> subject, auto& accept) -> decltype(auto) {
			return invoke_accept(accept, external_tag{}, self, iv8::external{subject});
		}

		// promise
		auto operator()(this const auto& self, skip_lookup /*skip*/, v8::Local<v8::Promise> subject, auto& accept) -> decltype(auto) {
			return invoke_accept(accept, promise_tag{}, self, subject);
		}

		auto null_value_cache() const { return null_; }
		auto undefined_value_cache() const { return undefined_; }

	private:
		iv8::isolate_lock_witness isolate_lock_;
		mutable v8::Local<v8::Primitive> null_;
		mutable v8::Local<v8::Primitive> undefined_;
};

// Primary visitor w/ `context_lock_witness`
struct visit_v8_value : visit_flat_v8_value {
	public:
		using visit_flat_v8_value::operator();

		explicit visit_v8_value(const iv8::context_lock_witness& lock) :
				visit_flat_v8_value{lock},
				context_lock_{lock} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return context_lock_; }

		auto operator()(this const auto& self, v8::Local<v8::Value> subject, auto& accept) -> decltype(auto) {
			// Check known address values before the map lookup
			if (subject == self.null_value_cache()) {
				return invoke_accept(accept, null_tag{}, self, subject);
			} else if (subject == self.undefined_value_cache()) {
				return invoke_accept(accept, undefined_tag{}, self, subject);
			}

			// Check the reference map, and check type
			return lookup_or_visit(subject, accept, [ & ]() -> decltype(auto) {
				if (subject->IsObject()) {
					return self(skip_lookup{}, subject.As<v8::Object>(), accept);
				} else {
					return self(skip_lookup{}, subject.As<v8::Primitive>(), accept);
				}
			});
		}

	protected:
		// object
		auto operator()(this const auto& self, skip_lookup skip, v8::Local<v8::Object> subject, auto& accept) -> decltype(auto) {
			if (subject->IsArray()) {
				return self(skip, subject.As<v8::Array>(), accept);
			} else if (subject->IsExternal()) {
				return self(skip, subject.As<v8::External>(), accept);
			} else if (subject->IsDate()) {
				return self(skip, subject.As<v8::Date>(), accept);
			} else if (subject->IsPromise()) {
				return self(skip, subject.As<v8::Promise>(), accept);
			} else {
				auto visit_entry = std::pair<visit_v8_property_name<visit_v8_value>, const visit_v8_value&>{self, self};
				return invoke_accept(accept, dictionary_tag{}, visit_entry, iv8::object{self.lock_witness(), subject.As<v8::Object>()});
			}
		}

		// array
		auto operator()(this const auto& self, skip_lookup /*skip*/, v8::Local<v8::Array> subject, auto& accept) -> decltype(auto) {
			auto visit_entry = std::pair<visit_v8_property_name<visit_v8_value>, const visit_v8_value&>{self, self};
			return invoke_accept(accept, list_tag{}, visit_entry, iv8::object{self.lock_witness(), subject.As<v8::Object>()});
		}

	private:
		js::iv8::context_lock_witness context_lock_;
};

// Name visitor (string + symbol)
template <>
struct visit<void, v8::Local<v8::Name>> : visit_flat_v8_value {
		using visit_flat_v8_value::visit_flat_v8_value;
};

// String visitor
template <>
struct visit<void, v8::Local<v8::String>> : visit_flat_v8_value {
		using visit_flat_v8_value::visit_flat_v8_value;
};

// Generic visitor
template <>
struct visit<void, v8::Local<v8::Value>> : visit_v8_value {
		using visit_v8_value::visit_v8_value;
};

// `arguments` visitor
template <class Type>
struct visit_subject_for<v8::FunctionCallbackInfo<Type>> : visit_subject_for<v8::Local<Type>> {};

template <>
struct visit<void, v8::FunctionCallbackInfo<v8::Value>> : visit<void, v8::Local<v8::Value>> {
		using visit_type = visit<void, v8::Local<v8::Value>>;
		using visit_type::visit_type;

		using visit_type::operator();
		auto operator()(v8::FunctionCallbackInfo<v8::Value> info, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, arguments_tag{}, *this, iv8::callback_info{info});
		}
};

} // namespace js
