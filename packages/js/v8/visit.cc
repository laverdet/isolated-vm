module;
#include <cstdint>
#include <unordered_map>
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

// Instantiated in the acceptor that corresponds to a napi visitor
struct v8_reference_map_type {
		template <class Type>
		using type = std::unordered_map<v8::Local<v8::Value>, Type, iv8::address_hash>;
};

template <class Visit>
struct visit_v8_property_name {
	public:
		explicit visit_v8_property_name(const Visit& visit) : visit_{visit} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return visit_.lock_witness(); }

		auto operator()(v8::Local<v8::Primitive> subject, auto& accept) const -> decltype(auto) {
			return visit_.get().lookup_or_visit(accept, subject, [ & ]() -> decltype(auto) {
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
template <class Target>
struct visit_flat_v8_value;

template <class Meta>
using visit_flat_v8_value_with = visit_flat_v8_value<typename Meta::accept_reference_type>;

template <class Target>
struct visit_flat_v8_value : reference_map_t<Target, v8_reference_map_type> {
	public:
		using reference_map_type = reference_map_t<Target, v8_reference_map_type>;
		using reference_map_type::lookup_or_visit;
		using reference_map_type::try_emplace;

		explicit visit_flat_v8_value(auto* /*transfer*/, const js::iv8::isolate_lock_witness& lock) :
				isolate_lock_{lock} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return isolate_lock_; }

		// If the protected `immediate` operation is defined: this public operation will first
		// perform a reference map lookup, then delegate to the protected operation if not found.
		auto operator()(this const auto& self, auto subject, auto& accept) -> decltype(auto)
			requires requires { self.immediate(subject, accept); } {
			return self.lookup_or_visit(accept, subject, [ & ]() -> decltype(auto) {
				return self.immediate(subject, accept);
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
		auto immediate(v8::Local<v8::Primitive> subject, auto& accept) const -> decltype(auto) {
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
				return immediate(subject.As<v8::Name>(), accept);
			} else if (subject->IsBoolean()) {
				return (*this)(subject.As<v8::Boolean>(), accept);
			} else if (subject->IsBigInt()) {
				return immediate(subject.As<v8::BigInt>(), accept);
			} else {
				std::unreachable();
			}
		}

		// names
		auto immediate(v8::Local<v8::Name> subject, auto& accept) const -> decltype(auto) {
			if (subject->IsString()) {
				return immediate(subject.As<v8::String>(), accept);
			} else {
				return immediate(subject.As<v8::Symbol>(), accept);
			}
		}

		auto immediate(v8::Local<v8::String> subject, auto& accept) const -> decltype(auto) {
			auto string = iv8::string{lock_witness(), subject};
			if (subject->IsOneByte()) {
				return try_emplace(accept, string_tag_of<char>{}, *this, string);
			} else {
				return try_emplace(accept, string_tag_of<char16_t>{}, *this, string);
			}
		}

		auto immediate(v8::Local<v8::Symbol> subject, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, symbol_tag{}, *this, subject);
		}

		// bigint
		auto immediate(v8::Local<v8::BigInt> subject, auto& accept) const -> decltype(auto) {
			bool lossless{};
			auto u64 = subject->Uint64Value(&lossless);
			if (lossless) {
				return try_emplace(accept, bigint_tag_of<uint64_t>{}, *this, iv8::bigint_u64{subject, u64});
			} else {
				return try_emplace(accept, bigint_tag_of<bigint>{}, *this, iv8::bigint_n{subject});
			}
		}

		// date
		auto immediate(this const auto& self, v8::Local<v8::Date> subject, auto& accept) -> decltype(auto) {
			return self.try_emplace(accept, date_tag{}, self, iv8::date{subject});
		}

		// external
		auto immediate(this const auto& self, v8::Local<v8::External> subject, auto& accept) -> decltype(auto) {
			return self.try_emplace(accept, external_tag{}, self, iv8::external{subject});
		}

		// promise
		auto immediate(this const auto& self, v8::Local<v8::Promise> subject, auto& accept) -> decltype(auto) {
			return self.try_emplace(accept, promise_tag{}, self, subject);
		}

		auto null_value_cache() const { return null_; }
		auto undefined_value_cache() const { return undefined_; }

	private:
		iv8::isolate_lock_witness isolate_lock_;
		mutable v8::Local<v8::Primitive> null_;
		mutable v8::Local<v8::Primitive> undefined_;
};

// Primary visitor w/ `context_lock_witness`
template <class Target>
struct visit_v8_value;

template <class Meta>
using visit_v8_value_with = visit_v8_value<typename Meta::accept_reference_type>;

template <class Target>
struct visit_v8_value : visit_flat_v8_value<Target> {
	public:
		using visit_type = visit_flat_v8_value<Target>;
		using visit_type::immediate;
		using visit_type::lookup_or_visit;
		using visit_type::operator();

		explicit visit_v8_value(auto* transfer, const iv8::context_lock_witness& lock) :
				visit_type{transfer, lock},
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
			return self.lookup_or_visit(accept, subject, [ & ]() -> decltype(auto) {
				if (subject->IsObject()) {
					return self.immediate(subject.As<v8::Object>(), accept);
				} else {
					return self.immediate(subject.As<v8::Primitive>(), accept);
				}
			});
		}

	protected:
		// object
		auto immediate(this const auto& self, v8::Local<v8::Object> subject, auto& accept) -> decltype(auto) {
			if (subject->IsArray()) {
				return self.immediate(subject.As<v8::Array>(), accept);
			} else if (subject->IsExternal()) {
				return self.immediate(subject.As<v8::External>(), accept);
			} else if (subject->IsDate()) {
				return self.immediate(subject.As<v8::Date>(), accept);
			} else if (subject->IsPromise()) {
				return self.immediate(subject.As<v8::Promise>(), accept);
			} else {
				auto visit_entry = std::pair<visit_v8_property_name<visit_v8_value>, const visit_v8_value&>{self, self};
				return self.try_emplace(accept, dictionary_tag{}, visit_entry, iv8::object{self.lock_witness(), subject.As<v8::Object>()});
			}
		}

		// array
		auto immediate(this const auto& self, v8::Local<v8::Array> subject, auto& accept) -> decltype(auto) {
			auto visit_entry = std::pair<visit_v8_property_name<visit_v8_value>, const visit_v8_value&>{self, self};
			return self.try_emplace(accept, list_tag{}, visit_entry, iv8::object{self.lock_witness(), subject.As<v8::Object>()});
		}

	private:
		js::iv8::context_lock_witness context_lock_;
};

// Name visitor (string + symbol)
template <class Meta>
struct visit<Meta, v8::Local<v8::Name>> : visit_flat_v8_value_with<Meta> {
		using visit_flat_v8_value_with<Meta>::visit_flat_v8_value_with;
};

// String visitor
template <class Meta>
struct visit<Meta, v8::Local<v8::String>> : visit_flat_v8_value_with<Meta> {
		using visit_flat_v8_value_with<Meta>::visit_flat_v8_value_with;
};

// Generic visitor
template <class Meta>
struct visit<Meta, v8::Local<v8::Value>> : visit_v8_value_with<Meta> {
		using visit_v8_value_with<Meta>::visit_v8_value_with;
};

// `arguments` visitor
template <class Type>
struct visit_subject_for<v8::FunctionCallbackInfo<Type>> : visit_subject_for<v8::Local<Type>> {};

template <class Meta>
struct visit<Meta, v8::FunctionCallbackInfo<v8::Value>> : visit<Meta, v8::Local<v8::Value>> {
		using visit_type = visit<Meta, v8::Local<v8::Value>>;
		using visit_type::visit_type;

		using visit_type::operator();
		auto operator()(v8::FunctionCallbackInfo<v8::Value> info, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, arguments_tag{}, *this, iv8::callback_info{info});
		}
};

} // namespace js
