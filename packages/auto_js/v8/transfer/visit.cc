export module v8_js:visit;
import :array_buffer;
import :array;
import :callback_info;
import :handle;
import :handle.value;
import :hash;
import :lock;
import :object;
import :primitive;
import :unmaybe;
import :value.tag;
import auto_js;
import std;
import v8;

namespace js::iv8 {

// Instantiated in the acceptor that corresponds to a v8 visitor
struct visit_reference_map_type {
		template <class Type>
		using type = std::unordered_map<v8::Local<v8::Data>, Type, address_hash>;
};

// Visitor adaptor which internalized property keys instead of values
template <class Visit>
struct visit_property_name {
	public:
		explicit visit_property_name(Visit& visit) : visit_{visit} {}

		template <class Accept>
		auto operator()(v8::Local<v8::Primitive> subject, const Accept& accept) -> accept_target_t<Accept> {
			return visit_.get().lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				auto accept_as = [ & ]<class Tag>(Tag tag) -> auto {
					auto value = value_of{witness(), subject.As<tag_to_v8<Tag>>()};
					return accept(tag, *this, value);
				};
				if (subject->IsNumber()) {
					return accept_as(number_tag_of<std::int32_t>{});
				} else if (subject->IsString()) {
					if (subject.As<v8::String>()->IsOneByte()) {
						return accept_as(string_tag_of<char>{});
					} else {
						return accept_as(string_tag_of<char16_t>{});
					}
				} else {
					return accept_as(symbol_tag{});
				}
			});
		}

		[[nodiscard]] auto environment() const -> auto& { return visit_.get().environment(); }
		[[nodiscard]] auto witness() const -> auto { return visit_.get().witness(); }

	private:
		std::reference_wrapper<Visit> visit_;
};

// Implements `Visit`'s non-caching `immediate()` function as a caching visit operation.
template <class Visit>
struct visit_cached_immediate : Visit {
		using Visit::immediate;
		using Visit::lookup_or_visit;
		using Visit::operator();
		using Visit::Visit;

		template <class Accept>
		auto operator()(auto subject, const Accept& accept) -> accept_target_t<Accept>
			// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124954
			requires requires { this->immediate(subject, accept); } {
			return lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				return immediate(subject, accept);
			});
		}
};

// Forwards `Visit`'s non-caching `immediate()` as a visit operation
template <class Visit>
struct visit_uncached_immediate : Visit {
		using Visit::immediate;
		using Visit::operator();
		using Visit::Visit;

		template <class Accept>
		auto operator()(auto subject, const Accept& accept) -> accept_target_t<Accept>
			// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124954
			requires requires { this->immediate(subject, accept); } {
			return immediate(subject, accept);
		}
};

// Primitive-ish value visitor. These only need an isolate, so they are separated from the other
// visitor. Also, none of these are recursive.
template <class Reference>
struct visit_flat_value;

template <class Meta>
using visit_flat_value_with = visit_cached_immediate<visit_flat_value<typename Meta::accept_reference_type>>;

template <class Meta>
using visit_uncached_flat_value_with = visit_uncached_immediate<visit_flat_value<void>>;

template <class Reference>
struct visit_flat_value : reference_map_t<Reference, visit_reference_map_type> {
	public:
		explicit visit_flat_value(auto* /*transfer*/, isolate_lock_witness lock) : isolate_lock_{lock} {}
		explicit visit_flat_value(auto* transfer, const auto& lock) :
				visit_flat_value{transfer, isolate_lock_witness{util::slice(lock)}} {}

		// numbers
		template <class Accept>
		auto operator()(v8::Local<v8::Number> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsInt32()) {
				return immediate(subject.As<v8::Int32>(), accept);
			} else {
				return immediate(subject.As<iv8::Double>(), accept);
			}
		}

		// boolean
		template <class Accept>
		auto operator()(v8::Local<v8::Boolean> subject, const Accept& accept) -> accept_target_t<Accept> {
			return immediate(subject, accept);
		}

		// extras
		[[nodiscard]] auto witness() const -> auto { return isolate_lock_; }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

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

		template <class Accept, class Type>
			requires std::is_base_of_v<primitive_tag, iv8::v8_to_tag<Type>>
		auto immediate(v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
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
			if (subject->IsOneByte()) {
				return immediate(subject.As<iv8::StringOneByte>(), accept);
			} else {
				return immediate(subject.As<iv8::StringTwoByte>(), accept);
			}
		}

		// bigint
		template <class Accept>
		auto immediate(v8::Local<v8::BigInt> subject, const Accept& accept) -> accept_target_t<Accept> {
			bool lossless{};
			auto i64 = subject->Int64Value(&lossless);
			if (lossless) {
				return accept(bigint_tag_of<std::int64_t>{}, *this, value_of{witness(), subject.As<iv8::BigInt64>(), i64});
			} else {
				return immediate(subject.As<iv8::BigIntWords>(), accept);
			}
		}

		// date
		template <class Accept>
		auto immediate(v8::Local<v8::Date> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// data blocks
		template <class Accept>
		auto immediate(v8::Local<iv8::DataBlock> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsSharedArrayBuffer()) {
				return immediate(subject.As<v8::SharedArrayBuffer>(), accept);
			} else {
				return immediate(subject.As<v8::ArrayBuffer>(), accept);
			}
		}

		template <class Accept, class Type>
			requires std::is_base_of_v<data_block_tag, iv8::v8_to_tag<Type>>
		auto immediate(v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// external
		template <class Accept>
		auto immediate(v8::Local<v8::External> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// promise (cannot be accepted)
		template <class Accept>
		auto immediate(v8::Local<v8::Promise> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(promise_tag{}, *this, subject);
		}

		// function (cannot be accepted)
		template <class Accept>
		auto immediate(v8::Local<v8::Function> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(function_tag{}, *this, subject);
		}

		// Convenience function which wraps in `iv8::value_of` and invokes `accept`.
		template <class Type, class Accept>
		[[nodiscard]] auto accept_tagged(v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(iv8::v8_to_tag<Type>{}, *this, value_of{witness(), subject});
		}

		[[nodiscard]] auto is_cached_null(v8::Local<v8::Value> value) const { return value == null_; }
		[[nodiscard]] auto is_cached_undefined(v8::Local<v8::Value> value) const { return value == undefined_; }

	private:
		isolate_lock_witness isolate_lock_;
		v8::Local<v8::Primitive> null_;
		v8::Local<v8::Primitive> undefined_;
};

// Primary visitor w/ `context_lock_witness`
template <class Target>
struct visit_value;

template <class Meta>
using visit_value_with = visit_cached_immediate<visit_value<typename Meta::accept_reference_type>>;

template <class Target>
struct visit_value : visit_flat_value<Target> {
	public:
		friend struct visit_flat_value<Target>;
		using visit_type = visit_flat_value<Target>;
		using visit_type::immediate;
		using visit_type::is_cached_null;
		using visit_type::is_cached_undefined;
		using visit_type::lookup_or_visit;
		using visit_type::witness;
		using visit_type::operator();

		explicit visit_value(auto* transfer, context_lock_witness lock) :
				visit_type{transfer, lock},
				context_lock_{lock} {}
		explicit visit_value(auto* transfer, const auto& lock) :
				visit_value{transfer, context_lock_witness{util::slice(lock)}} {}

		template <class Accept>
		auto operator()(v8::Local<v8::Value> subject, const Accept& accept) -> accept_target_t<Accept> {
			// Check known address values before the map lookup
			if (is_cached_null(subject)) {
				return accept(null_tag{}, *this, subject);
			} else if (is_cached_undefined(subject)) {
				return accept(undefined_tag{}, *this, subject);
			}

			// Check the reference map, and check type
			return lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				if (subject->IsObject()) {
					return immediate(subject.As<v8::Object>(), accept);
				} else {
					return immediate(subject.As<v8::Primitive>(), accept);
				}
			});
		}

		template <class Accept>
		auto operator()(v8::Local<iv8::DataBlock> subject, const Accept& accept) -> accept_target_t<Accept> {
			return lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				return immediate(subject, accept);
			});
		}

		// extras
		[[nodiscard]] auto witness() const -> auto& { return context_lock_; }

	protected:
		// object
		template <class Accept>
		auto immediate(v8::Local<v8::Object> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsArray()) {
				return immediate(subject.As<v8::Array>(), accept);
			} else if (subject->IsExternal()) {
				return immediate(subject.As<v8::External>(), accept);
			} else if (subject->IsDate()) {
				return immediate(subject.As<v8::Date>(), accept);
			} else if (subject->IsArrayBuffer()) {
				return immediate(subject.As<v8::ArrayBuffer>(), accept);
			} else if (subject->IsSharedArrayBuffer()) {
				return immediate(subject.As<v8::SharedArrayBuffer>(), accept);
			} else if (subject->IsArrayBufferView()) {
				return immediate(subject.As<v8::ArrayBufferView>(), accept);
			} else if (subject->IsPromise()) {
				return immediate(subject.As<v8::Promise>(), accept);
			} else if (subject->IsFunction()) {
				return immediate(subject.As<v8::Function>(), accept);
			} else {
				auto visit_entry = visit_entry_pair<visit_property_name<visit_value>, visit_value&>{*this};
				return accept(dictionary_tag{}, visit_entry, value_of{witness(), subject.As<v8::Object>()});
			}
		}

		// array
		template <class Accept>
		auto immediate(v8::Local<v8::Array> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto visit_entry = visit_entry_pair<visit_property_name<visit_value>, visit_value&>{*this};
			return accept(list_tag{}, visit_entry, value_of{witness(), subject.As<v8::Object>()});
		}

		// function template
		template <class Accept>
		auto immediate(v8::Local<v8::FunctionTemplate> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(function_tag{}, *this, unmaybe(subject->GetFunction(context_lock_.context())));
		}

		// array buffer views (typed arrays, data view)
		template <class Accept>
		auto immediate(v8::Local<v8::ArrayBufferView> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsUint8Array()) {
				return immediate(subject.As<v8::Uint8Array>(), accept);
			} else if (subject->IsDataView()) {
				return immediate(subject.As<v8::DataView>(), accept);
			} else if (subject->IsInt8Array()) {
				return immediate(subject.As<v8::Int8Array>(), accept);
			} else if (subject->IsUint16Array()) {
				return immediate(subject.As<v8::Uint16Array>(), accept);
			} else if (subject->IsInt16Array()) {
				return immediate(subject.As<v8::Int16Array>(), accept);
			} else if (subject->IsUint32Array()) {
				return immediate(subject.As<v8::Uint32Array>(), accept);
			} else if (subject->IsInt32Array()) {
				return immediate(subject.As<v8::Int32Array>(), accept);
			} else if (subject->IsFloat32Array()) {
				return immediate(subject.As<v8::Float32Array>(), accept);
			} else if (subject->IsFloat64Array()) {
				return immediate(subject.As<v8::Float64Array>(), accept);
			} else if (subject->IsBigInt64Array()) {
				return immediate(subject.As<v8::BigInt64Array>(), accept);
			} else if (subject->IsBigUint64Array()) {
				return immediate(subject.As<v8::BigUint64Array>(), accept);
			} else if (subject->IsUint8ClampedArray()) {
				return immediate(subject.As<v8::Uint8ClampedArray>(), accept);
			} else {
				throw js::type_error{u"Received exotic v8 'ArrayBufferView'"};
			}
		}

		template <class Accept, class Type>
			requires std::is_base_of_v<array_buffer_view_tag, v8_to_tag<Type>>
		auto immediate(v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(v8_to_tag<Type>{}, *this, value_of{witness(), subject});
		}

	private:
		context_lock_witness context_lock_;
};

// Template visitor needs no lock. The acceptor will instantiate values.
struct visit_template {
		explicit visit_template(auto* /*transfer*/) {}

		template <class Accept>
		auto operator()(v8::Local<v8::FunctionTemplate> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(function_prototype_tag{}, *this, subject);
		}

		template <class Accept>
		auto operator()(v8::Local<v8::ObjectTemplate> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(object_prototype_tag{}, *this, subject);
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

} // namespace js::iv8

namespace js {

// Name visitor (string + symbol)
template <class Meta, class Type>
	requires std::is_base_of_v<v8::Primitive, Type>
struct visit<Meta, v8::Local<Type>> : iv8::visit_uncached_flat_value_with<Meta> {
		using iv8::visit_uncached_flat_value_with<Meta>::visit_uncached_flat_value_with;
};

// Value visitor
template <class Meta, class Type>
struct visit<Meta, v8::Local<Type>> : iv8::visit_value_with<Meta> {
		using iv8::visit_value_with<Meta>::visit_value_with;
};

// Template visitor
template <class Meta, class Type>
	requires std::is_base_of_v<v8::Template, Type>
struct visit<Meta, v8::Local<Type>> : iv8::visit_template {
		using iv8::visit_template::visit_template;
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
			return accept(vector_tag{}, *this, iv8::callback_info{info});
		}
};

} // namespace js
