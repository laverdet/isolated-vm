module;
#ifdef _LIBCPP_VERSION
// nb: Symbol visibility hack
#include <__iterator/wrap_iter.h>
#endif
#include <cstddef>
#include <utility>
export module ivm.iv8:accept;
import :date;
import :string;
import ivm.value;
import v8;

namespace ivm::value {

// Object key lookup for v8 values
template <class Meta, auto Key>
struct visit_key<Meta, Key, v8::Local<v8::Value>> {
	public:
		constexpr visit_key(const auto_visit auto& visit) :
				root{visit} {}

		auto operator()(const auto& object, const auto& visit, const auto_accept auto& accept) const -> decltype(auto) {
			return visit.second(object.get(get_local()), accept);
		}

	private:
		auto get_local() const -> v8::Local<v8::Value> {
			if (local_key == v8::Local<v8::String>{}) {
				local_key = v8::String::NewFromUtf8Literal(root.isolate(), Key);
			}
			return local_key;
		}

		const visit_subject_t<Meta>& root;
		v8::Local<v8::String> local_key;
};

// Base class for primitive acceptors. Not really an acceptor.
template <>
struct accept<void, v8::Local<v8::Data>> {
	public:
		accept() = delete;
		explicit accept(v8::Isolate* isolate) :
				isolate_{isolate} {}

		auto isolate() const {
			return isolate_;
		}

		auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const -> v8::Local<v8::Value> {
			return v8::Undefined(isolate());
		}

		auto operator()(null_tag /*tag*/, auto&& /*null*/) const -> v8::Local<v8::Value> {
			return v8::Null(isolate());
		}

		auto operator()(boolean_tag /*tag*/, auto&& value) const -> v8::Local<v8::Boolean> {
			return v8::Boolean::New(isolate_, std::forward<decltype(value)>(value));
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const -> v8::Local<v8::Number> {
			return v8::Number::New(isolate_, Numeric{std::forward<decltype(value)>(value)});
		}

		auto operator()(string_tag /*tag*/, auto&& value) const -> v8::Local<v8::String> {
			return iv8::string::make(isolate_, std::forward<decltype(value)>(value));
		}

	private:
		v8::Isolate* isolate_;
};

// Explicit boolean acceptor
template <>
struct accept<void, v8::Local<v8::Boolean>> : accept<void, v8::Local<v8::Data>> {
		using accept<void, v8::Local<v8::Data>>::accept;
		auto operator()(boolean_tag tag, auto&& value) const -> v8::Local<v8::Boolean> {
			const accept<void, v8::Local<v8::Data>>& accept = *this;
			return accept(tag, std::forward<decltype(value)>(value));
		}
};

// Explicit numeric acceptor
template <>
struct accept<void, v8::Local<v8::Number>> : accept<void, v8::Local<v8::Data>> {
		using accept<void, v8::Local<v8::Data>>::accept;
		template <class Numeric>
		auto operator()(number_tag_of<Numeric> tag, auto&& value) const -> v8::Local<v8::Number> {
			const accept<void, v8::Local<v8::Data>>& accept = *this;
			return accept(tag, std::forward<decltype(value)>(value));
		}
};

// Explicit string acceptor
template <>
struct accept<void, v8::Local<v8::String>> : accept<void, v8::Local<v8::Data>> {
		using accept<void, v8::Local<v8::Data>>::accept;
		auto operator()(string_tag tag, auto&& value) const -> v8::Local<v8::String> {
			const accept<void, v8::Local<v8::Data>>& accept = *this;
			return accept(tag, std::forward<decltype(value)>(value));
		}
};

// Generic acceptor for most values
template <>
struct accept<void, v8::Local<v8::Value>> : accept<void, v8::Local<v8::Data>> {
	public:
		accept() = delete;
		accept(v8::Isolate* isolate, v8::Local<v8::Context> context) :
				accept<void, v8::Local<v8::Data>>{isolate},
				context_{context} {}

		auto context() const {
			return context_;
		}

		auto operator()(auto_tag auto tag, auto&& value) const -> v8::Local<v8::Value>
			requires std::invocable<accept<void, v8::Local<v8::Data>>, decltype(tag), decltype(value)>
		{
			const accept<void, v8::Local<v8::Data>>& accept = *this;
			return accept(tag, std::forward<decltype(value)>(value));
		}

		auto operator()(date_tag /*tag*/, auto&& value) const -> v8::Local<v8::Value> {
			return iv8::date::make(context_, std::forward<decltype(value)>(value));
		}

		auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> v8::Local<v8::Value> {
			auto array = v8::Array::New(isolate());
			for (auto&& [ key, value ] : list) {
				auto result = array->Set(
					context_,
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
				result.Check();
			}
			return array;
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> v8::Local<v8::Value> {
			auto object = v8::Object::New(isolate());
			for (auto&& [ key, value ] : dictionary) {
				auto result = object->Set(
					context_,
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
				result.Check();
			}
			return object;
		}

	private:
		v8::Local<v8::Context> context_;
};

// A `MaybeLocal` also accepts `undefined`, similar to `std::optional`.
template <class Meta, class Type>
struct accept<Meta, v8::MaybeLocal<Type>> : accept<Meta, v8::Local<Type>> {
		using accept<Meta, v8::Local<Type>>::accept;
		using accept_type = accept<Meta, v8::Local<Type>>;

		auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> v8::MaybeLocal<Type>
			requires std::invocable<accept_type, decltype(tag), decltype(value), decltype(rest)...> {
			return {accept_type::operator()(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...)};
		}

		auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const -> v8::MaybeLocal<Type> {
			return v8::MaybeLocal<Type>{};
		}
};

}; // namespace ivm::value
