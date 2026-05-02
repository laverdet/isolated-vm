export module v8_js:object;
import :array;
import :primitive;
import auto_js;
import std;
import v8;

namespace js::iv8 {

class value_for_object : public handle_with_context<v8::Object> {
	public:
		using key_type = v8::Local<v8::Primitive>;
		using mapped_type = v8::Local<v8::Value>;
		using value_type = std::pair<key_type, mapped_type>;
		using handle_with_context<v8::Object>::handle_with_context;

	private:
		// We use `array` as the base iteration, over the keys of this object, and transform that into
		// an entry pair.
		class iterator_transform {
			public:
				iterator_transform(v8::Local<v8::Object> object, v8::Local<v8::Context> context);
				auto operator()(v8::Local<v8::Value> key) const -> value_type;

			private:
				v8::Local<v8::Object> object_;
				v8::Local<v8::Context> context_;
		};

		[[nodiscard]] auto keys() const -> const value_for_array&;

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<const value_for_array&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		[[nodiscard]] auto get(v8::Local<v8::Name> key) -> v8::Local<v8::Value>;
		[[nodiscard]] auto get(v8::Local<v8::Number> key) -> v8::Local<v8::Value>;
		[[nodiscard]] auto get(v8::Local<v8::Primitive> key) -> v8::Local<v8::Value>;
		[[nodiscard]] auto into_range() -> range_type;
		[[nodiscard]] auto size() const -> std::size_t;

	private:
		mutable value_for_array keys_;
};

static_assert(std::ranges::range<value_for_object::range_type>);
static_assert(std::random_access_iterator<value_for_object::iterator>);

} // namespace js::iv8
