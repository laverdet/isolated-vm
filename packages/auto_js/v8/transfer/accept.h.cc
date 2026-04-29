export module v8_js:accept;
import :array_buffer;
import :hash;
import :lock;
import :primitive;
import :unmaybe;
import :value.tag;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

// Reference acceptor
struct reaccept_value {
		using reference_type = v8::Local<v8::Value>;

		template <class Type>
		constexpr auto operator()(std::type_identity<v8::Local<Type>> /*type*/, v8::Local<v8::Value> value) const -> v8::Local<Type> {
			return value.As<Type>();
		}
};

// Base class for primitive acceptors. These require only an isolate lock.
struct accept_primitive {
	public:
		accept_primitive() = delete;
		explicit accept_primitive(isolate_lock_witness lock) :
				isolate_{lock.isolate()} {}
		explicit accept_primitive(const std::convertible_to<isolate_lock_witness> auto& lock) :
				accept_primitive{isolate_lock_witness{util::slice(lock)}} {}

		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

		// Declare reference provider
		using accept_reference_type = reaccept_value;

		// undefined & null
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> v8::Local<iv8::Undefined> {
			return v8::Undefined(isolate()).As<iv8::Undefined>();
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*null*/) const -> v8::Local<iv8::Null> {
			return v8::Null(isolate()).As<iv8::Null>();
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Boolean> {
			return v8::Boolean::New(isolate_, std::forward<decltype(subject)>(subject));
		}

		// number
		auto operator()(number_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Number> {
			return v8::Number::New(isolate_, double{std::forward<decltype(subject)>(subject)});
		}

		auto operator()(number_tag_of<double> /*tag*/, visit_holder visit, auto&& subject) const -> v8::Local<iv8::Double> {
			return (*this)(number_tag{}, visit, std::forward<decltype(subject)>(subject)).template As<iv8::Double>();
		}

		auto operator()(number_tag_of<std::int32_t> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Int32> {
			return v8::Integer::New(isolate_, std::int32_t{std::forward<decltype(subject)>(subject)}).As<v8::Int32>();
		}

		auto operator()(number_tag_of<std::uint32_t> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Uint32> {
			return v8::Integer::NewFromUnsigned(isolate_, std::uint32_t{std::forward<decltype(subject)>(subject)}).As<v8::Uint32>();
		}

		// string
		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, std::convertible_to<std::basic_string_view<Char>> auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::String>> {
			return js::referenceable_value{string::make(isolate_, std::basic_string_view<Char>{std::forward<decltype(subject)>(subject)})};
		}

		// auto operator()(string_tag_of<char> /*tag*/, visit_holder /*visit*/, std::string_view subject) const
		// 	-> js::referenceable_value<v8::Local<iv8::StringOneByte>> {
		// 	return js::referenceable_value{string::make(isolate_, std::basic_string<Char>{std::forward<decltype(subject)>(subject)})};
		// }

		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::String>> {
			return js::referenceable_value{string::make(isolate_, std::basic_string<Char>{std::forward<decltype(subject)>(subject)})};
		}

		// function (instantiated in visitor)
		auto operator()(function_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::Function> subject) const -> v8::Local<v8::Function> {
			return subject;
		}

		// no required types
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		v8::Isolate* isolate_;
};

// Generic acceptor for most values. These require a context lock.
struct accept_value : accept_primitive {
	public:
		using accept_type = accept_primitive;
		explicit accept_value(context_lock_witness lock) :
				accept_type{lock},
				context_{lock.context()} {}
		explicit accept_value(const std::convertible_to<context_lock_witness> auto& lock) :
				accept_value{context_lock_witness{util::slice(lock)}} {}

		// accept all primitives
		using accept_type::operator();

		[[nodiscard]] auto witness() const -> context_lock_witness {
			auto isolate_lock = isolate_lock_witness::make_witness(isolate());
			return context_lock_witness::make_witness(isolate_lock, context_);
		}

		// hacky function template acceptor
		// NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
		auto operator()(function_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::Function> value) const -> v8::Local<v8::Function> {
			return value;
		}

		// bigint (why does `NewFromWords` need a context?)
		auto operator()(bigint_tag /*tag*/, visit_holder /*visit*/, const js::bigint& subject) const
			-> js::referenceable_value<v8::Local<v8::BigInt>> {
			auto value = unmaybe(v8::BigInt::NewFromWords(context_, subject.sign_bit(), static_cast<int>(subject.size()), subject.data()));
			return js::referenceable_value{value};
		}

		auto operator()(bigint_tag tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::BigInt>> {
			return (*this)(tag, visit, js::bigint{std::forward<decltype(subject)>(subject)});
		}

		auto operator()(bigint_tag_of<js::bigint> /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<v8::Local<iv8::BigIntWords>> {
			auto value = (*this)(bigint_tag{}, visit, std::forward<decltype(subject)>(subject));
			return js::referenceable_value{value->template As<iv8::BigIntWords>()};
		}

		auto operator()(bigint_tag_of<std::int64_t> /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<iv8::BigInt64>> {
			auto value = v8::BigInt::New(isolate(), std::forward<decltype(subject)>(subject));
			return js::referenceable_value{value.template As<iv8::BigInt64>()};
		}

		auto operator()(bigint_tag_of<std::uint64_t> /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<iv8::BigIntU64>> {
			auto value = v8::BigInt::NewFromUnsigned(isolate(), std::forward<decltype(subject)>(subject));
			return js::referenceable_value{value.template As<iv8::BigIntU64>()};
		}

		// date
		auto operator()(date_tag /*tag*/, visit_holder /*visit*/, js_clock::time_point subject) const
			-> js::referenceable_value<v8::Local<v8::Date>> {
			auto value = unmaybe(v8::Date::New(context_, subject.time_since_epoch().count())).As<v8::Date>();
			return js::referenceable_value{value};
		}

		// error
		auto operator()(error_tag /*tag*/, visit_holder /*visit*/, const auto& subject) const
			-> js::referenceable_value<v8::Local<v8::Object>> {
			auto message = string::make(isolate(), subject.message());
			auto error = v8::Local<v8::Value>{[ & ]() -> v8::Local<v8::Value> {
				switch (subject.name()) {
					// These functions don't need an isolate somehow?
					default:
						return v8::Exception::Error(message);
					case js::error_name::range_error:
						return v8::Exception::RangeError(message);
					case js::error_name::reference_error:
						return v8::Exception::ReferenceError(message);
					case js::error_name::syntax_error:
						return v8::Exception::SyntaxError(message);
					case js::error_name::type_error:
						return v8::Exception::TypeError(message);
				}
			}()};
			return js::referenceable_value{error.As<v8::Object>()};
		}

		// array
		auto operator()(this const auto& self, list_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<v8::Local<v8::Array>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				v8::Array::New(self.isolate()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](v8::Local<v8::Array> array, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
						auto result = array->Set(
							self.context_,
							visit.first(std::forward<decltype(key)>(key), self),
							visit.second(std::forward<decltype(value)>(value), self)
						);
						unmaybe(result);
					}
				},
			};
		}

		// object
		auto operator()(this const auto& self, dictionary_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<v8::Local<v8::Object>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				v8::Object::New(self.isolate()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](v8::Local<v8::Object> object, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
						auto result = object->Set(
							self.context_,
							visit.first(std::forward<decltype(key)>(key), self),
							visit.second(std::forward<decltype(value)>(value), self)
						);
						unmaybe(result);
					}
				},
			};
		}

		// data blocks
		auto operator()(array_buffer_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::ArrayBuffer>> {
			auto data = js::array_buffer{std::forward<decltype(subject)>(subject)};
			return js::referenceable_value{array_buffer::make(util::slice(witness()), std::move(data))};
		}

		auto operator()(shared_array_buffer_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::SharedArrayBuffer>> {
			auto data = js::shared_array_buffer{std::forward<decltype(subject)>(subject)};
			return js::referenceable_value{shared_array_buffer::make(util::slice(witness()), std::move(data))};
		}

		// typed arrays
		template <class Type>
		auto operator()(this const auto& self, typed_array_tag_of<Type> /*tag*/, auto& visit, auto&& subject)
			-> js::referenceable_value<v8::Local<v8::ArrayBufferView>> {
			using v8_type = tag_to_v8<typed_array_tag_of<Type>>;
			auto byte_offset = subject.byte_offset();
			auto length = subject.size();
			auto buffer = v8::Local<v8::Value>{visit(std::forward<decltype(subject)>(subject).buffer(), self)};
			auto view = [ & ]() -> v8::Local<v8_type> {
				if (buffer->IsSharedArrayBuffer()) {
					return v8_type::New(buffer.As<v8::SharedArrayBuffer>(), byte_offset, length);
				} else {
					return v8_type::New(buffer.As<v8::ArrayBuffer>(), byte_offset, length);
				}
			}();
			return js::referenceable_value<v8::Local<v8::ArrayBufferView>>{view};
		};

		// data view
		template <class Type>
		auto operator()(this const auto& self, data_view_tag /*tag*/, auto& visit, auto&& subject)
			-> js::referenceable_value<v8::Local<v8::ArrayBufferView>> {
			auto byte_offset = subject.byte_offset();
			auto length = subject.size();
			auto buffer = v8::Local<v8::Object>{visit(std::forward<decltype(subject)>(subject).buffer(), self)};
			auto view = [ & ]() -> v8::Local<Type> {
				if (buffer->IsSharedArrayBuffer()) {
					return v8::DataView::New(buffer.As<v8::SharedArrayBuffer>(), byte_offset, length);
				} else {
					return v8::DataView::New(buffer.As<v8::ArrayBuffer>(), byte_offset, length);
				}
			}();
			return js::referenceable_value<v8::Local<v8::ArrayBufferView>>{view};
		}

	private:
		v8::Local<v8::Context> context_;
};

} // namespace js::iv8

namespace js {

template <class Type>
	requires std::is_base_of_v<primitive_tag, iv8::v8_to_tag<Type>>
struct accept<void, v8::Local<Type>> : iv8::accept_primitive {
		using accept_primitive::accept_primitive;
};

template <class Type>
struct accept<void, v8::Local<Type>> : iv8::accept_value {
		using accept_value::accept_value;
};

// A `MaybeLocal` also accepts `undefined`, similar to `std::optional`.
template <class Type>
struct accept<void, v8::MaybeLocal<Type>> : accept<void, v8::Local<Type>> {
		using accept_type = accept<void, v8::Local<Type>>;
		using accept_type::accept_type;

		using accept_type::operator();

		// `requires true` used as a tiebreaker for the regular `v8::Local<v8::Value>` acceptor above.
		// There seems to be some disagreement about whether or not the specification is ambiguous in
		// this matter.
		// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119859
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> v8::MaybeLocal<Type>
			requires true {
			return {};
		}
};

// return value
template <>
struct accept<void, v8::ReturnValue<v8::Value>> : accept<void, v8::Local<v8::Value>> {
		using accept_type = accept<void, v8::Local<v8::Value>>;
		using accept_type::accept_type;
		using value_type = v8::ReturnValue<v8::Value>;

		accept(iv8::context_lock_witness lock, value_type return_value) :
				accept_type{lock},
				return_value_{return_value} {}
		explicit accept(const std::convertible_to<iv8::context_lock_witness> auto& lock, value_type return_value) :
				accept{iv8::context_lock_witness{util::slice(lock)}, return_value} {}

		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, const auto& value) const -> value_type {
			return_value_.Set(bool{value});
			return return_value_;
		}

		template <class Type>
		auto operator()(number_tag_of<Type> /*tag*/, visit_holder /*visit*/, const auto& value) const -> value_type {
			return_value_.Set(Type{value});
			return return_value_;
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*null*/) const -> value_type {
			return_value_.SetNull();
			return return_value_;
		}

		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> value_type {
			return_value_.SetUndefined();
			return return_value_;
		}

		auto operator()(auto tag, auto& visit, auto&& subject) const -> value_type
			requires std::invocable<const accept_type&, decltype(visit), decltype(tag), decltype(subject)> {
			return_value_.Set(util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(subject)>(subject)));
			return return_value_;
		}

	private:
		mutable v8::ReturnValue<v8::Value> return_value_;
};

} // namespace js
