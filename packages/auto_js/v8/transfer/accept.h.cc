export module v8_js:accept;
import :array_buffer;
import :callback_storage;
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

// Forward declaration from :callback
template <class Lock>
constexpr auto make_free_function(auto function);

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

		// undefined
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*subject*/) const -> v8::Local<iv8::Undefined> {
			// nb: Inline definition because of the finnicky `v8::MaybeLocal<T>` acceptor
			return v8::Undefined(isolate()).As<iv8::Undefined>();
		}

		// null
		auto operator()(null_tag tag, visit_holder visit, std::nullptr_t subject) const -> v8::Local<iv8::Null>;

		auto operator()(null_tag tag, visit_holder visit, const auto& /*subject*/) const -> v8::Local<iv8::Null> {
			return (*this)(tag, visit, nullptr);
		}

		// boolean
		auto operator()(boolean_tag tag, visit_holder visit, bool subject) const -> v8::Local<v8::Boolean>;

		// number
		auto operator()(number_tag_of<double> tag, visit_holder visit, double subject) const -> v8::Local<iv8::Double>;
		auto operator()(number_tag_of<std::int32_t> tag, visit_holder visit, std::int32_t) const -> v8::Local<v8::Int32>;
		auto operator()(number_tag_of<std::uint32_t> tag, visit_holder visit, std::uint32_t) const -> v8::Local<v8::Uint32>;

		auto operator()(number_tag /*tag*/, visit_holder visit, auto&& subject) const -> v8::Local<v8::Number> {
			return (*this)(number_tag_of<double>{}, visit, double{std::forward<decltype(subject)>(subject)});
		}

		template <class Type>
		auto operator()(number_tag_of<Type> tag, visit_holder visit, auto&& subject) const -> v8::Local<tag_to_v8<number_tag_of<Type>>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		// string
		auto operator()(string_tag_of<char> tag, visit_holder visit, std::string_view subject) const
			-> js::referenceable_value<v8::Local<iv8::StringOneByte>>;
		auto operator()(string_tag_of<char8_t> tag, visit_holder visit, std::u8string_view subject) const
			-> js::referenceable_value<v8::Local<v8::String>>;
		auto operator()(string_tag_of<char16_t> tag, visit_holder visit, std::u16string_view subject) const
			-> js::referenceable_value<v8::Local<iv8::StringTwoByte>>;

		auto operator()(string_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::String>> {
			auto value = (*this)(string_tag_of<char16_t>{}, visit, std::u16string_view{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{v8::Local<v8::String>{*value}};
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, std::convertible_to<std::basic_string_view<Char>> auto&& subject) const
			-> js::referenceable_value<v8::Local<tag_to_v8<string_tag_of<Char>>>> {
			return (*this)(tag, visit, std::basic_string_view<Char>{std::forward<decltype(subject)>(subject)});
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<v8::Local<tag_to_v8<string_tag_of<Char>>>> {
			return (*this)(tag, visit, std::basic_string<Char>{std::forward<decltype(subject)>(subject)});
		}

		// extras
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

		// bigint (why does `NewFromWords` need a context?)
		auto operator()(bigint_tag_of<std::int64_t> tag, visit_holder visit, std::int64_t subject) const
			-> js::referenceable_value<v8::Local<iv8::BigInt64>>;
		auto operator()(bigint_tag_of<std::uint64_t> tag, visit_holder visit, std::uint64_t subject) const
			-> js::referenceable_value<v8::Local<iv8::BigIntU64>>;
		auto operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, const js::bigint& subject) const
			-> js::referenceable_value<v8::Local<iv8::BigIntWords>>;
		auto operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, js::bigint&& subject) const
			-> js::referenceable_value<v8::Local<iv8::BigIntWords>>;

		auto operator()(bigint_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::BigInt>> {
			auto value = (*this)(bigint_tag_of<js::bigint>{}, visit, js::bigint{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{v8::Local<v8::BigInt>{*value}};
		}

		template <class Type>
		auto operator()(bigint_tag_of<Type> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<v8::Local<tag_to_v8<bigint_tag_of<Type>>>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		// date
		auto operator()(date_tag tag, visit_holder /*visit*/, js_clock::time_point subject) const
			-> js::referenceable_value<v8::Local<v8::Date>>;

		// error
		auto operator()(error_tag tag, visit_holder visit, const js::error_value& subject) const
			-> js::referenceable_value<v8::Local<v8::Object>>;

		auto operator()(error_tag tag, visit_holder visit, const auto& subject) const
			-> js::referenceable_value<v8::Local<v8::Object>> {
			return (*this)(tag, visit, js::error_value{subject});
		}

		// function
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::FunctionTemplate> subject) const -> v8::Local<v8::Function>;

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

		// data blocks (array buffer, shared array buffer)
		auto operator()(array_buffer_tag tag, visit_holder visit, js::array_buffer subject) const
			-> js::referenceable_value<v8::Local<v8::ArrayBuffer>>;
		auto operator()(shared_array_buffer_tag tag, visit_holder visit, js::shared_array_buffer&& subject) const
			-> js::referenceable_value<v8::Local<v8::SharedArrayBuffer>>;
		auto operator()(shared_array_buffer_tag tag, visit_holder visit, const js::shared_array_buffer& subject) const
			-> js::referenceable_value<v8::Local<v8::SharedArrayBuffer>>;

		// array buffer views (typed arrays, data view)
		template <std::convertible_to<array_buffer_view_tag> Tag>
		auto operator()(this const auto& self, Tag /*tag*/, auto& visit, auto&& subject)
			-> js::referenceable_value<v8::Local<tag_to_v8<Tag>>> {
			using v8_type = tag_to_v8<Tag>;
			auto byte_offset = subject.byte_offset();
			auto length = subject.size();
			auto buffer = visit(std::forward<decltype(subject)>(subject).buffer(), self);
			auto make = util::overloaded{
				[ & ](this auto&, v8::Local<v8::ArrayBuffer> buffer) -> v8::Local<v8_type> {
					return v8_type::New(buffer, byte_offset, length);
				},
				[ & ](this auto&, v8::Local<v8::SharedArrayBuffer> buffer) -> v8::Local<v8_type> {
					return v8_type::New(buffer, byte_offset, length);
				},
				[](this auto& make, v8::Local<v8::Value> buffer) -> v8::Local<v8_type> {
					if (buffer->IsSharedArrayBuffer()) {
						return make(buffer.As<v8::SharedArrayBuffer>());
					} else {
						return make(buffer.As<v8::ArrayBuffer>());
					}
				},
			};
			return js::referenceable_value{make(buffer)};
		};

	private:
		v8::Local<v8::Context> context_;
};

// Acceptor with template environment
template <class Meta, class Environment>
struct accept_template;

template <class Meta, class Agent, class... Implements>
struct accept_template<Meta, isolate_lock_witness_of<Agent, Implements...>> : accept_primitive {
	public:
		explicit accept_template(auto* /*transfer*/, const isolate_lock_witness_of<Agent, Implements...>& lock) :
				accept_primitive{lock},
				lock_{lock} {}

		// function template
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> v8::Local<v8::FunctionTemplate> {
			using lock_type = const context_lock_witness_of<Agent, Implements...>&;
			auto [ callback, data ] = make_callback_storage(lock_.get(), make_free_function<lock_type>(std::move(subject).callback));
			return v8::FunctionTemplate::New(lock_.get().isolate(), callback, data, v8::Local<v8::Signature>{}, 0, v8::ConstructorBehavior::kThrow);
		}

	private:
		std::reference_wrapper<const isolate_lock_witness_of<Agent, Implements...>> lock_;
};

} // namespace js::iv8

namespace js {

template <class Type>
	requires std::is_base_of_v<v8::Primitive, Type>
struct accept<void, v8::Local<Type>> : iv8::accept_primitive {
		using accept_primitive::accept_primitive;
};

template <class Type>
struct accept<void, v8::Local<Type>> : iv8::accept_value {
		using accept_value::accept_value;
};

template <class Meta, class Type>
	requires std::is_base_of_v<v8::Template, Type>
struct accept<Meta, v8::Local<Type>> : iv8::accept_template<Meta, typename Meta::accept_context_type> {
		using iv8::accept_template<Meta, typename Meta::accept_context_type>::accept_template;
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
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*subject*/) const -> v8::MaybeLocal<Type>
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
