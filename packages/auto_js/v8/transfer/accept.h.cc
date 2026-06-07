export module v8_js:accept;
import :callback_storage;
import :hash;
import :value;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

// Forward declaration from :callback
template <class Lock>
constexpr auto make_free_function(auto function);

// Reference acceptor
struct reaccept_v8_value {
		using reference_type = v8::Local<v8::Value>;

		template <class Type>
		constexpr auto operator()(std::type_identity<v8::Local<Type>> /*type*/, v8::Local<v8::Value> subject) const -> v8::Local<Type> {
			return subject.As<Type>();
		}
};

// Base class for primitive acceptors. These require only an isolate lock.
struct accept_v8_primitive {
	public:
		accept_v8_primitive() = delete;
		explicit accept_v8_primitive(auto* /*transfer*/, isolate_lock_witness lock) :
				isolate_{lock.isolate()} {}
		explicit accept_v8_primitive(auto* transfer, const std::convertible_to<isolate_lock_witness> auto& lock) :
				accept_v8_primitive{transfer, isolate_lock_witness{util::slice(lock)}} {}

		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

		// Declare reference provider
		using accept_reference_type = reaccept_v8_value;

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
		auto operator()(number_tag_of<std::int32_t> tag, visit_holder visit, std::int32_t subject) const -> v8::Local<v8::Int32>;
		auto operator()(number_tag_of<std::uint32_t> tag, visit_holder visit, std::uint32_t subject) const -> v8::Local<v8::Uint32>;

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
struct accept_v8_value : accept_v8_primitive {
	public:
		using accept_type = accept_v8_primitive;
		using accept_target_type = v8::Local<v8::Value>;
		explicit accept_v8_value(auto* transfer, context_lock_witness lock) :
				accept_type{transfer, lock},
				context_{lock.context()} {}
		explicit accept_v8_value(auto* transfer, const std::convertible_to<context_lock_witness> auto& lock) :
				accept_v8_value{transfer, context_lock_witness{util::slice(lock)}} {}

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
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::FunctionTemplate> subject) const -> v8::Local<iv8::Function>;

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

		// vectors
		auto operator()(this const auto& self, vector_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<v8::Local<v8::Array>, decltype(self), decltype(visit), decltype(subject)> {
			auto [... size ] = util::maybe_range_size(subject);
			return {
				v8::Array::New(self.isolate(), size...),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](v8::Local<v8::Array> array, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					std::uint32_t ii = 0;
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& element : util::forward_range(std::forward<decltype(range)>(range))) {
						auto item = visit(std::forward<decltype(element)>(element), self);
						unmaybe(array->Set(self.context_, ii++, item));
					}
				},
			};
		}

		template <std::size_t Size>
		auto operator()(this const auto& self, tuple_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<v8::Local<v8::Array>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				v8::Array::New(self.isolate(), Size),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](v8::Local<v8::Array> array, auto& self, auto& visit, auto /*&&*/ tuple) -> void {
					const auto [... indices ] = util::sequence<Size>;
					(..., [ & ]() -> void {
						auto element = visit(indices, std::forward<decltype(tuple)>(tuple), self);
						unmaybe(array->Set(self.context_, indices, element));
					}());
				}
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

		// structs
		template <std::size_t Size>
		auto operator()(this const auto& self, struct_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<v8::Local<v8::Object>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				v8::Object::New(self.isolate()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](v8::Local<v8::Object> object, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					self.template accept_entry_pair_struct<Size>(visit, object, std::forward<decltype(subject)>(subject));
				}
			};
		}

		template <std::size_t Size>
		auto accept_entry_pair_struct(this const auto& self, auto& visit, v8::Local<v8::Object> target, auto&& subject) -> void {
			const auto [... indices ] = util::sequence<Size>;
			(..., [ & ]() -> void {
				auto accept_entry = accept_entry_pair<decltype(self), decltype(self)>{self};
				auto entry = visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(subject)>(subject), accept_entry);
				unmaybe(target->Set(self.context_, entry.first, entry.second));
			}());
		}

	private:
		v8::Local<v8::Context> context_;
};

// `accept_v8_value` subclass, implementing specialized acceptor with lock type.
template <class Lock>
struct accept_v8_value_with;

template <>
struct accept_v8_value_with<context_lock_witness> : accept_v8_value {
		using accept_v8_value::accept_v8_value;
		using accept_v8_value::operator();
		using accept_v8_value::witness;

		// function instantiation
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> v8::Local<iv8::Function> {
			auto [ function, length ] = make_free_function<context_lock_witness>(std::move(subject).callback);
			auto [ callback, data ] = make_callback_storage(witness(), std::move(function));
			return unmaybe(v8::Function::New(witness().context(), callback, data, length, v8::ConstructorBehavior::kThrow).template As<iv8::Function>());
		}
};

template <class Lock>
struct accept_v8_value_with : accept_v8_value {
	public:
		accept_v8_value_with(auto* transfer, const Lock& lock) :
				accept_v8_value{transfer, lock},
				lock_{lock} {}

		using accept_v8_value::operator();

		// function instantiation
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> v8::Local<iv8::Function> {
			auto [ function, length ] = make_free_function<Lock>(std::move(subject).callback);
			auto [ callback, data ] = make_callback_storage(lock_.get(), std::move(function));
			return v8::Function::New(lock_.get().context(), callback, data, length, v8::ConstructorBehavior::kThrow);
		}

	private:
		std::reference_wrapper<const Lock> lock_;
};

// Acceptor with template environment
template <class Meta, class Lock>
struct accept_v8_template : accept_v8_primitive {
	public:
		explicit accept_v8_template(auto* transfer, const Lock& lock) :
				accept_v8_primitive{transfer, lock},
				lock_{lock} {}

		// function template
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> v8::Local<v8::FunctionTemplate> {
			auto [ function, length ] = make_free_function<Lock>(std::move(subject).callback);
			auto [ callback, data ] = make_callback_storage(lock_.get(), std::move(function));
			return v8::FunctionTemplate::New(lock_.get().isolate(), callback, data, v8::Local<v8::Signature>{}, length, v8::ConstructorBehavior::kThrow);
		}

	private:
		std::reference_wrapper<const Lock> lock_;
};

// object key lookup (struct_template, etc)
template <class Meta, auto Key, class Type>
struct accept_v8_property_value {
	public:
		explicit constexpr accept_v8_property_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		auto operator()(dictionary_tag /*tag*/, auto& visit, const auto& subject) const -> Type {
			if (auto local = first(std::type_identity<void>{}, visit.first); subject.has(local)) {
				return visit.second(subject.get(local), second);
			} else {
				return second(undefined_in_tag{}, visit.second, std::monostate{});
			}
		}

	private:
		mutable visit_key_literal<Key, v8::Local<v8::Object>> first;
		accept_value<Meta, Type> second;
};

// `return_into(...)`
struct return_into_marker {};

auto return_into(auto& env, v8::ReturnValue<v8::Value> return_value, auto value) -> void {
	std::ignore = js::transfer_in_strict<return_into_marker>(std::move(value), env, return_value);
}

template <class Type>
auto return_into(auto& /*env*/, v8::ReturnValue<v8::Value> return_value, js::forward<v8::Local<Type>> value) -> void {
	return_value.Set(*value);
}

template <class Lock>
struct accept_v8_return_into : accept_v8_value_with<Lock> {
	private:
		using accept_type = accept_v8_value_with<Lock>;
		using value_type = v8::ReturnValue<v8::Value>;

	public:
		accept_v8_return_into(auto* transfer, const auto& lock, value_type return_value) :
				accept_type{transfer, lock},
				return_value_{return_value} {}

		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, const auto& subject) const -> return_into_marker {
			return_value_.Set(bool{subject});
			return {};
		}

		template <class Type>
		auto operator()(number_tag_of<Type> /*tag*/, visit_holder /*visit*/, const auto& subject) const -> return_into_marker {
			return_value_.Set(Type{subject});
			return {};
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*subject*/) const -> return_into_marker {
			return_value_.SetNull();
			return {};
		}

		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*subject*/) const -> return_into_marker {
			return_value_.SetUndefined();
			return {};
		}

		auto operator()(auto tag, auto& visit, auto&& subject) const -> return_into_marker
			requires std::invocable<const accept_type&, decltype(tag), decltype(visit), decltype(subject)> {
			auto value = util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(subject)>(subject));
			return_value_.Set(js::dispatch_referenceable(std::move(value)));
			return {};
		}

	private:
		mutable v8::ReturnValue<v8::Value> return_value_;
};

// `object_assign(value, ...)`
struct object_assign_marker {};

export auto object_assign(auto& env, v8::Local<v8::Object> object, auto source) -> void {
	std::ignore = js::transfer_in_strict<object_assign_marker>(std::move(source), env, object);
}

template <class Meta, class Lock>
struct accept_v8_object_assign {
	private:
		using accept_type = accept_v8_value_with<Lock>;

	public:
		accept_v8_object_assign(auto* transfer, const auto& lock, v8::Local<v8::Object> object) :
				accept_{transfer, lock},
				object_{object} {}

		template <std::size_t Size>
		auto operator()(this const auto& self, tuple_tag<Size> /*tag*/, auto& visit, auto&& subject) -> iv8::object_assign_marker {
			self.accept_.template accept_entry_pair_struct<Size>(visit, self.object_, std::forward<decltype(subject)>(subject));
			return {};
		}

		consteval static auto types(auto recursive) { return accept<Meta, v8::Local<v8::Value>>::types(recursive); }

	private:
		accept_value<Meta, v8::Local<v8::Value>> accept_;
		v8::Local<v8::Object> object_;
};

} // namespace js::iv8

namespace js {

template <class Type>
struct accept_property_subject<v8::Local<Type>> : std::type_identity<v8::Local<v8::Object>> {};

// primitives
template <class Meta, class Type>
	requires std::is_convertible_v<Type, v8::Primitive>
struct accept<Meta, v8::Local<Type>> : iv8::accept_v8_primitive {
		using accept_v8_primitive::accept_v8_primitive;
};

// values
template <class Meta, class Type>
struct accept<Meta, v8::Local<Type>> : iv8::accept_v8_value_with<typename Meta::accept_context_type> {
		using iv8::accept_v8_value_with<typename Meta::accept_context_type>::accept_v8_value_with;
};

// templates
template <class Meta, class Type>
	requires std::is_convertible_v<Type, v8::Template>
struct accept<Meta, v8::Local<Type>> : iv8::accept_v8_template<Meta, typename Meta::accept_context_type> {
		using iv8::accept_v8_template<Meta, typename Meta::accept_context_type>::accept_v8_template;
};

// A `MaybeLocal` also accepts `undefined`, similar to `std::optional`.
template <class Meta, class Type>
struct accept<Meta, v8::MaybeLocal<Type>> : accept<Meta, v8::Local<Type>> {
		using accept_type = accept<Meta, v8::Local<Type>>;
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

// object key lookup (struct_template, etc)
template <class Meta, auto Key, class Type>
struct accept_property_value<Meta, Key, Type, v8::Local<v8::Object>> : iv8::accept_v8_property_value<Meta, Key, Type> {
		using iv8::accept_v8_property_value<Meta, Key, Type>::accept_v8_property_value;
};

// return_into
template <>
struct accept_property_subject<iv8::return_into_marker> : accept_property_subject<v8::Local<v8::Value>> {};

template <class Meta>
struct accept<Meta, iv8::return_into_marker> : iv8::accept_v8_return_into<typename Meta::accept_context_type> {
		using iv8::accept_v8_return_into<typename Meta::accept_context_type>::accept_v8_return_into;
};

// object_assign
template <class Meta>
struct accept<Meta, iv8::object_assign_marker> : iv8::accept_v8_object_assign<Meta, typename Meta::accept_context_type> {
		using iv8::accept_v8_object_assign<Meta, typename Meta::accept_context_type>::accept_v8_object_assign;
};

} // namespace js
