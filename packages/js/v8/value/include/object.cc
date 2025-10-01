module;
#include <ranges>
export module v8_js:object;
import :array;
import :handle;
import :lock;
import :string;
import isolated_js;
import ivm.utility;
import v8;

namespace js::iv8 {

export class object
		: public v8::Local<v8::Object>,
			public handle_with_context,
			public materializable<object> {
	public:
		using value_type = std::pair<v8::Local<v8::Value>, v8::Local<v8::Value>>;

		explicit object(const context_lock_witness& lock, v8::Local<v8::Object> handle) :
				Local{handle},
				handle_with_context{lock} {}

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

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<array&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		[[nodiscard]] auto get(v8::Local<v8::Value> key) -> v8::Local<v8::Value>;
		[[nodiscard]] auto has(v8::Local<v8::Value> key) -> bool;
		[[nodiscard]] auto into_range() -> range_type;

	private:
		mutable array keys_;
};

static_assert(std::ranges::range<object::range_type>);
static_assert(std::random_access_iterator<object::iterator>);

} // namespace js::iv8
