export module v8_js:visit;
import :array_buffer;
import :callback_info;
import :handle.value;
import :hash;
import :unmaybe;
import :value;
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
			return visit_.get().lookup_or_visit(subject, [ & ] -> accept_target_t<Accept> {
				auto accept_as = [ & ]<class Tag>(Tag tag) -> auto {
					auto value = value_of{witness(), subject.As<tag_to_v8<Tag>>()};
					return accept(tag, *this, value);
				};
				if (subject->IsString()) {
					if (subject.As<v8::String>()->IsOneByte()) {
						return accept_as(string_tag_of<char>{});
					} else {
						return accept_as(string_tag_of<char16_t>{});
					}
				} else if (subject->IsNumber()) {
					return accept_as(number_tag_of<std::int32_t>{});
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

// `visit_key_literal` instantiation for v8
template <auto Key>
struct visit_v8_key_literal {
		explicit constexpr visit_v8_key_literal(auto* /*transfer*/) {}

		auto operator()(const auto& /*anything*/, const auto& accept_or_visit) -> v8::Local<v8::Name> {
			if (local_key_.IsEmpty()) {
				constexpr auto key = util::make_consteval_string_view(Key);
				local_key_ = transfer_in<v8::Local<v8::Name>>(key, accept_or_visit.witness());
			}
			return local_key_;
		}

	private:
		v8::Local<v8::Name> local_key_;
};

// Implements `Visit`'s non-caching `immediate()` function as a caching visit operation.
template <class Visit>
struct visit_cached_immediate : Visit {
		using Visit::immediate;
		using Visit::lookup_or_visit;
		using Visit::operator();
		using Visit::Visit;

		template <class Accept>
		auto operator()(this auto& self, auto subject, const Accept& accept) -> accept_target_t<Accept>
			// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124954
			requires requires { self.immediate(subject, accept); } {
			return self.lookup_or_visit(subject, [ & ] -> accept_target_t<Accept> {
				return self.immediate(subject, accept);
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
		auto operator()(this auto& self, auto subject, const Accept& accept) -> accept_target_t<Accept>
			// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124954
			requires requires { self.immediate(subject, accept); } {
			return self.immediate(subject, accept);
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
		auto operator()(this auto& self, v8::Local<v8::Number> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsInt32()) {
				return self.immediate(subject.As<v8::Int32>(), accept);
			} else {
				return self.immediate(subject.As<iv8::Double>(), accept);
			}
		}

		// boolean
		template <class Accept>
		auto operator()(this auto& self, v8::Local<v8::Boolean> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.immediate(subject, accept);
		}

		// extras
		[[nodiscard]] auto witness() const -> auto { return isolate_lock_; }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	protected:
		// primitives
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Primitive> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsUndefined()) {
				return accept(undefined_tag{}, self, subject);
			} else if (subject->IsNull()) {
				return accept(null_tag{}, self, subject);
			} else if (subject->IsNumber()) {
				return self(subject.As<v8::Number>(), accept);
			} else if (subject->IsName()) {
				return self.immediate(subject.As<v8::Name>(), accept);
			} else if (subject->IsBoolean()) {
				return self(subject.As<v8::Boolean>(), accept);
			} else if (subject->IsBigInt()) {
				return self.immediate(subject.As<v8::BigInt>(), accept);
			} else {
				std::unreachable();
			}
		}

		template <class Accept, class Type>
			requires std::is_convertible_v<Type, v8::Primitive>
		auto immediate(this auto& self, v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.accept_tagged(subject, accept);
		}

		// names
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Name> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsString()) {
				return self.immediate(subject.As<v8::String>(), accept);
			} else {
				return self.immediate(subject.As<v8::Symbol>(), accept);
			}
		}

		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::String> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsOneByte()) {
				return self.immediate(subject.As<iv8::StringOneByte>(), accept);
			} else {
				return self.immediate(subject.As<iv8::StringTwoByte>(), accept);
			}
		}

		// bigint
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::BigInt> subject, const Accept& accept) -> accept_target_t<Accept> {
			bool lossless{};
			auto i64 = subject->Int64Value(&lossless);
			if (lossless) {
				return accept(bigint_tag_of<std::int64_t>{}, self, value_of{self.witness(), subject.As<iv8::BigInt64>(), i64});
			} else {
				return self.immediate(subject.As<iv8::BigIntWords>(), accept);
			}
		}

		// date
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Date> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.accept_tagged(subject, accept);
		}

		// data blocks
		template <class Accept>
		auto immediate(this auto& self, v8::Local<iv8::DataBlock> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsSharedArrayBuffer()) {
				return self.immediate(subject.As<v8::SharedArrayBuffer>(), accept);
			} else {
				return self.immediate(subject.As<v8::ArrayBuffer>(), accept);
			}
		}

		template <class Accept, class Type>
			requires std::is_convertible_v<iv8::v8_to_tag<Type>, data_block_tag>
		auto immediate(this auto& self, v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.accept_tagged(subject, accept);
		}

		// external
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::External> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.accept_tagged(subject, accept);
		}

		// promise (maybe could be forwarded)
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::Promise> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(promise_tag{}, self, subject);
		}

		// function (can be forwarded)
		template <class Type, class Accept>
			requires std::is_convertible_v<Type, v8::Function>
		auto immediate(this auto& self, v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(function_tag{}, self, subject);
		}

		// Convenience function which wraps in `iv8::value_of` and invokes `accept`.
		template <class Type, class Accept>
		[[nodiscard]] auto accept_tagged(this auto& self, v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(iv8::v8_to_tag<Type>{}, self, value_of{self.witness(), subject});
		}

	private:
		isolate_lock_witness isolate_lock_;
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
		using visit_type::accept_tagged;
		using visit_type::immediate;
		using visit_type::lookup_or_visit;
		using visit_type::witness;
		using visit_type::operator();

		explicit visit_value(auto* transfer, context_lock_witness lock) :
				visit_type{transfer, lock},
				context_{lock.context()} {}
		explicit visit_value(auto* transfer, const auto& lock) :
				visit_value{transfer, context_lock_witness{util::slice(lock)}} {}

		template <class Accept>
		auto operator()(this auto& self, v8::Local<v8::Value> subject, const Accept& accept) -> accept_target_t<Accept> {

			// Check the reference map, and check type
			return self.lookup_or_visit(subject, [ & ] -> accept_target_t<Accept> {
				return util::template_traverse(
					accept_tags_of_v<Accept>,
					util::overloaded{
						// Fast paths
						[ & ](undefined_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsUndefined() ? accept(undefined_tag{}, self, subject.As<iv8::Undefined>()) : next();
						},
						[ & ](null_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsNull() ? accept(null_tag{}, self, subject.As<iv8::Null>()) : next();
						},
						[ & ](boolean_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsBoolean() ? self(subject.As<v8::Boolean>(), accept) : next();
						},
						[ & ](number_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsNumber() ? self(subject.As<v8::Number>(), accept) : next();
						},
						[ & ](string_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsString() ? self.immediate(subject.As<v8::String>(), accept) : next();
						},
						[ & ](bigint_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsBigInt() ? self.immediate(subject.As<v8::BigInt>(), accept) : next();
						},
						[ & ](date_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsDate() ? self.immediate(subject.As<v8::Date>(), accept) : next();
						},
						[ & ](list_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsArray() ? self.immediate(subject.As<v8::Array>(), accept) : next();
						},
						[ & ](function_tag /*tag*/, auto next) -> accept_target_t<Accept> {
							return subject->IsFunction() ? self.immediate(subject.As<iv8::Function>(), accept) : next();
						},

						// Unknown tag
						[](auto /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },

						// Slow path
						[ & ] -> accept_target_t<Accept> {
							if (subject->IsObject()) {
								return self.immediate(subject.As<v8::Object>(), accept);
							} else {
								return self.immediate(subject.As<v8::Primitive>(), accept);
							}
						},
					}
				);
			});
		}

		template <class Accept>
		auto operator()(this auto& self, v8::Local<iv8::DataBlock> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.lookup_or_visit(subject, [ & ] -> accept_target_t<Accept> {
				return util::template_traverse(
					accept_tags_of_v<Accept>,
					util::overloaded{
						// Fast paths
						[ & ](data_block_tag /*tag*/, auto /*next*/) -> accept_target_t<Accept> { return self.accept_tagged(subject, accept); },
						[ & ](array_buffer_tag /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },
						[ & ](shared_array_buffer_tag /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },

						// Slow path
						[ & ] -> accept_target_t<Accept> {
							return self.immediate(subject, accept);
						}
					}
				);
			});
		}

		// extras
		[[nodiscard]] auto witness() const {
			return context_lock_witness::make_witness(visit_type::witness(), context_);
		}

	protected:
		// object
		template <class Visit, class Accept>
		auto immediate(this Visit& self, v8::Local<v8::Object> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsArray()) {
				return self.immediate(subject.As<v8::Array>(), accept);
			} else if (subject->IsExternal()) {
				return self.immediate(subject.As<v8::External>(), accept);
			} else if (subject->IsDate()) {
				return self.immediate(subject.As<v8::Date>(), accept);
			} else if (subject->IsArrayBuffer()) {
				return self.immediate(subject.As<v8::ArrayBuffer>(), accept);
			} else if (subject->IsSharedArrayBuffer()) {
				return self.immediate(subject.As<v8::SharedArrayBuffer>(), accept);
			} else if (subject->IsArrayBufferView()) {
				return self.immediate(subject.As<v8::ArrayBufferView>(), accept);
			} else if (subject->IsPromise()) {
				return self.immediate(subject.As<v8::Promise>(), accept);
			} else if (subject->IsFunction()) {
				return self.immediate(subject.As<iv8::Function>(), accept);
			} else {
				auto visit_entry = visit_entry_pair<visit_property_name<Visit>, Visit&>{self};
				return accept(dictionary_tag{}, visit_entry, value_of{self.witness(), subject.As<v8::Object>()});
			}
		}

		// array
		template <class Visit, class Accept>
		auto immediate(this Visit& self, v8::Local<v8::Array> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto visit_entry = visit_entry_pair<visit_property_name<Visit>, Visit&>{self};
			return accept(list_tag{}, visit_entry, value_of{self.witness(), subject.As<v8::Array>()});
		}

		// function template
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::FunctionTemplate> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(function_tag{}, self, unmaybe(subject->GetFunction(self.witness().context())));
		}

		// array buffer views (typed arrays, data view)
		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::ArrayBufferView> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject->IsUint8Array()) {
				return self.immediate(subject.As<v8::Uint8Array>(), accept);
			} else if (subject->IsDataView()) {
				return self.immediate(subject.As<v8::DataView>(), accept);
			} else if (subject->IsUint8ClampedArray()) {
				return self.immediate(subject.As<v8::Uint8ClampedArray>(), accept);
			} else if (subject->IsInt8Array()) {
				return self.immediate(subject.As<v8::Int8Array>(), accept);
			} else if (subject->IsUint16Array()) {
				return self.immediate(subject.As<v8::Uint16Array>(), accept);
			} else if (subject->IsInt16Array()) {
				return self.immediate(subject.As<v8::Int16Array>(), accept);
			} else if (subject->IsUint32Array()) {
				return self.immediate(subject.As<v8::Uint32Array>(), accept);
			} else if (subject->IsInt32Array()) {
				return self.immediate(subject.As<v8::Int32Array>(), accept);
			} else if (subject->IsFloat16Array()) {
				return self.immediate(subject.As<v8::Float16Array>(), accept);
			} else if (subject->IsFloat32Array()) {
				return self.immediate(subject.As<v8::Float32Array>(), accept);
			} else if (subject->IsFloat64Array()) {
				return self.immediate(subject.As<v8::Float64Array>(), accept);
			} else if (subject->IsBigInt64Array()) {
				return self.immediate(subject.As<v8::BigInt64Array>(), accept);
			} else if (subject->IsBigUint64Array()) {
				return self.immediate(subject.As<v8::BigUint64Array>(), accept);
			} else {
				throw js::type_error{u"Received exotic v8 'ArrayBufferView'"};
			}
		}

		template <class Accept, class Type>
			requires std::is_convertible_v<Type, v8::ArrayBufferView>
		auto immediate(this auto& self, v8::Local<Type> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(v8_to_tag<Type>{}, self, value_of{self.witness(), subject});
		}

	private:
		v8::Local<v8::Context> context_;
};

// Visitor with transfer delegate. The delegate is offered transferable subjects, claiming the ones
// listed in its `transferList` before the underlying visitor gets a chance to copy them.
template <class Target, class Delegate>
struct visit_value_delegate : visit_value<Target> {
	private:
		using visit_type = visit_value<Target>;

	public:
		visit_value_delegate(auto* transfer, context_lock_witness lock, Delegate& delegate) :
				visit_type{transfer, lock},
				delegate_{delegate} {}
		visit_value_delegate(auto* transfer, const auto& lock, Delegate& delegate) :
				visit_value_delegate{transfer, context_lock_witness{util::slice(lock)}, delegate} {}

		using visit_type::operator();
		using visit_type::immediate;

		template <class Accept>
		auto immediate(this auto& self, v8::Local<v8::ArrayBuffer> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.claim_or_immediate(subject, accept);
		}

		template <class Accept>
		auto immediate(this auto& self, v8::Local<iv8::DataBlock> subject, const Accept& accept) -> accept_target_t<Accept> {
			return self.claim_or_immediate(subject, accept);
		}

	private:
		template <class Accept>
		auto claim_or_immediate(this auto& self, auto subject, const Accept& accept) -> accept_target_t<Accept> {
			if (auto claimed = self.delegate_.get()(subject, self, accept)) {
				return *std::move(claimed);
			}
			return self.visit_value<Target>::immediate(subject, accept);
		}

		std::reference_wrapper<Delegate> delegate_;
};

template <class Meta, class Delegate>
using visit_value_delegate_with = visit_cached_immediate<visit_value_delegate<typename Meta::accept_reference_type, Delegate>>;

// Forward `value_of<T>` back to acceptor
template <class Tag>
struct visit_v8_value_of {
	private:
		using visit_type = visit_value<void>;

	public:
		constexpr explicit visit_v8_value_of(auto* transfer, context_lock_witness lock) : visit_{transfer, lock} {}

		template <class Accept>
		constexpr auto operator()(value_of<Tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(Tag{}, visit_, subject);
		}

		template <class Accept>
			requires std::is_same_v<Tag, object_tag>
		constexpr auto operator()(value_of<object_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto visit_entry = visit_entry_pair<visit_property_name<visit_type>, visit_type&>{visit_};
			return accept(object_tag{}, visit_entry, subject);
		}

		template <class Accept>
			requires std::is_same_v<Tag, list_tag>
		constexpr auto operator()(value_of<list_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto visit_entry = visit_entry_pair<visit_property_name<visit_type>, visit_type&>{visit_};
			return accept(list_tag{}, visit_entry, subject);
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		visit_type visit_;
};

// Template visitor needs no lock. The acceptor will instantiate values.
struct visit_template {
		explicit visit_template(auto* /*transfer*/) {}

		template <class Accept>
		auto operator()(v8::Local<v8::Template> subject, const Accept& accept) const -> accept_target_t<Accept> {
			if (subject->IsFunctionTemplate()) {
				return (*this)(subject.As<v8::FunctionTemplate>(), accept);
			} else if (subject->IsObjectTemplate()) {
				return (*this)(subject.As<v8::ObjectTemplate>(), accept);
			} else {
				return accept_target_t<Accept>{subject};
			}
		}

		template <class Accept>
		auto operator()(v8::Local<v8::FunctionTemplate> subject, const Accept& accept) const -> accept_target_t<Accept> {
			return accept(function_prototype_tag{}, *this, subject);
		}

		template <class Accept>
		auto operator()(v8::Local<v8::ObjectTemplate> subject, const Accept& accept) const -> accept_target_t<Accept> {
			return accept(object_prototype_tag{}, *this, subject);
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

} // namespace js::iv8

namespace js {

// Name visitor (string + symbol)
template <class Meta, class Type>
	requires std::is_convertible_v<Type, v8::Primitive>
struct visit<Meta, v8::Local<Type>> : iv8::visit_uncached_flat_value_with<Meta> {
		using iv8::visit_uncached_flat_value_with<Meta>::visit_uncached_flat_value_with;
};

// Value visitor
template <class Meta, class Type>
struct visit<Meta, v8::Local<Type>> : iv8::visit_value_with<Meta> {
		using iv8::visit_value_with<Meta>::visit_value_with;
};

// `transferee_visit_subject` subject visitor
template <class Meta, class Type, class Delegate>
struct visit<Meta, transferee_visit_subject<v8::Local<Type>, Delegate>> : iv8::visit_value_delegate_with<Meta, Delegate> {
	private:
		using visit_type = iv8::visit_value_delegate_with<Meta, Delegate>;

	public:
		using visit_type::visit_type;

		template <class Accept>
		auto operator()(const transferee_visit_subject<v8::Local<Type>, Delegate>& subject, const Accept& accept) -> accept_target_t<Accept> {
			return util::invoke_as<visit_type>(*this, *subject, accept);
		}
};

// value_of<T> visitor
template <class Meta, class Tag>
struct visit<Meta, iv8::value_of<Tag>> : iv8::visit_v8_value_of<Tag> {
		using iv8::visit_v8_value_of<Tag>::visit_v8_value_of;
};

template <class Tag>
struct visit_subject_for<iv8::value_of<Tag>> : std::type_identity<v8::Local<v8::Value>> {};

// Template visitor
template <class Meta, class Type>
	requires std::is_convertible_v<Type, v8::Template>
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
		auto operator()(const v8::FunctionCallbackInfo<v8::Value>& info, const Accept& accept) -> accept_target_t<Accept> {
			return accept(vector_tag{}, *this, iv8::callback_info{info});
		}
};

// Object key maker for v8 objects
template <auto Key>
struct visit_key_literal<Key, v8::Local<v8::Object>> : iv8::visit_v8_key_literal<Key> {
		using iv8::visit_v8_key_literal<Key>::visit_v8_key_literal;
};

} // namespace js
