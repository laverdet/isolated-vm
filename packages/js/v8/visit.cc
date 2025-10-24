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

// Instantiated in the acceptor that corresponds to a v8 visitor
struct v8_reference_map_type {
		template <class Type>
		using type = std::unordered_map<v8::Local<v8::Value>, Type, iv8::address_hash>;
};

template <class Visit>
struct visit_v8_property_name {
	public:
		explicit visit_v8_property_name(Visit& visit) : visit_{visit} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return visit_.lock_witness(); }

		template <class Accept>
		auto operator()(v8::Local<v8::Primitive> subject, const Accept& accept) -> accept_target_t<Accept> {
			return visit_.get().lookup_or_visit(accept, subject, [ & ]() -> accept_target_t<Accept> {
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
		std::reference_wrapper<Visit> visit_;
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

		explicit visit_flat_v8_value(auto* /*transfer*/, const js::iv8::isolate_lock_witness& lock) :
				isolate_lock_{lock} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return isolate_lock_; }

		// If the protected `immediate` operation is defined: this public operation will first
		// perform a reference map lookup, then delegate to the protected operation if not found.
		template <class Accept>
		auto operator()(this auto& self, auto subject, const Accept& accept) -> accept_target_t<Accept>
			requires requires { self.immediate(subject, accept); } {
			return self.lookup_or_visit(accept, subject, [ & ]() -> accept_target_t<Accept> {
				return self.immediate(subject, accept);
			});
		}

		// numbers
		template <class Accept>
		auto operator()(v8::Local<v8::Number> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto number = iv8::number{subject};
			if (subject->IsInt32()) {
				return accept(number_tag_of<int32_t>{}, *this, number);
			} else {
				return accept(number_tag_of<double>{}, *this, number);
			}
		}

		// boolean
		template <class Accept>
		auto operator()(v8::Local<v8::Boolean> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(boolean_tag{}, *this, iv8::boolean{subject.As<v8::Boolean>()});
		}

	protected:
		// primitives
		template <class Accept>
		auto immediate(v8::Local<v8::Primitive> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsNullOrUndefined()) {
				if (subject->IsNull()) {
					null_ = subject;
					return accept(null_tag{}, *this, subject);
				} else {
					undefined_ = subject;
					return accept(undefined_tag{}, *this, subject);
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
		template <class Accept>
		auto immediate(v8::Local<v8::Name> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsString()) {
				return immediate(subject.As<v8::String>(), accept);
			} else {
				return immediate(subject.As<v8::Symbol>(), accept);
			}
		}

		template <class Accept>
		auto immediate(v8::Local<v8::String> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto string = iv8::string{lock_witness(), subject};
			if (subject->IsOneByte()) {
				return accept(string_tag_of<char>{}, *this, string);
			} else {
				return accept(string_tag_of<char16_t>{}, *this, string);
			}
		}

		template <class Accept>
		auto immediate(v8::Local<v8::Symbol> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(symbol_tag{}, *this, subject);
		}

		// bigint
		template <class Accept>
		auto immediate(v8::Local<v8::BigInt> subject, const Accept& accept) -> accept_target_t<Accept> {
			bool lossless{};
			auto u64 = subject->Uint64Value(&lossless);
			if (lossless) {
				return accept(bigint_tag_of<uint64_t>{}, *this, iv8::bigint_u64{subject, u64});
			} else {
				return accept(bigint_tag_of<bigint>{}, *this, iv8::bigint_n{subject});
			}
		}

		// date
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Date> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(date_tag{}, self, iv8::date{subject});
		}

		// external
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::External> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(external_tag{}, self, iv8::external{subject});
		}

		// promise
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Promise> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(promise_tag{}, self, subject);
		}

		[[nodiscard]] auto is_cached_null(v8::Local<v8::Value> value) const { return value == null_; }
		[[nodiscard]] auto is_cached_undefined(v8::Local<v8::Value> value) const { return value == undefined_; }

	private:
		iv8::isolate_lock_witness isolate_lock_;
		v8::Local<v8::Primitive> null_;
		v8::Local<v8::Primitive> undefined_;
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
		using visit_type::operator();

		explicit visit_v8_value(auto* transfer, const iv8::context_lock_witness& lock) :
				visit_type{transfer, lock},
				context_lock_{lock} {}

		[[nodiscard]] auto lock_witness() const -> auto& { return context_lock_; }

		template <class Accept>
		auto operator()(this auto& self, v8::Local<v8::Value> subject, const Accept& accept) -> accept_target_t<Accept> {
			// Check known address values before the map lookup
			if (self.is_cached_null(subject)) {
				return accept(null_tag{}, self, subject);
			} else if (self.is_cached_undefined(subject)) {
				return accept(undefined_tag{}, self, subject);
			}

			// Check the reference map, and check type
			return self.lookup_or_visit(accept, subject, [ & ]() -> accept_target_t<Accept> {
				if (subject->IsObject()) {
					return self.immediate(subject.As<v8::Object>(), accept);
				} else {
					return self.immediate(subject.As<v8::Primitive>(), accept);
				}
			});
		}

	protected:
		// object
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Object> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsArray()) {
				return self.immediate(subject.As<v8::Array>(), accept);
			} else if (subject->IsExternal()) {
				return self.immediate(subject.As<v8::External>(), accept);
			} else if (subject->IsDate()) {
				return self.immediate(subject.As<v8::Date>(), accept);
			} else if (subject->IsPromise()) {
				return self.immediate(subject.As<v8::Promise>(), accept);
			} else {
				auto visit_entry = visit_entry_pair<visit_v8_property_name<visit_v8_value>, visit_v8_value&>{self};
				return accept(dictionary_tag{}, visit_entry, iv8::object{self.lock_witness(), subject.As<v8::Object>()});
			}
		}

		// array
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Array> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto visit_entry = visit_entry_pair<visit_v8_property_name<visit_v8_value>, visit_v8_value&>{self};
			return accept(list_tag{}, visit_entry, iv8::object{self.lock_witness(), subject.As<v8::Object>()});
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

		template <class Accept>
		auto operator()(v8::FunctionCallbackInfo<v8::Value> info, const Accept& accept) -> accept_target_t<Accept> {
			return accept(arguments_tag{}, *this, iv8::callback_info{info});
		}
};

} // namespace js
