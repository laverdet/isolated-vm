module;
#include "auto_js/chunk_view.h"
export module v8_js:map;
import :array;
import :handle;
import std;
import v8;

namespace js::iv8 {

export class value_for_map : public handle_with_context<v8::Map> {
	public:
		using key_type = v8::Local<v8::Value>;
		using mapped_type = v8::Local<v8::Value>;
		using value_type = std::pair<key_type, mapped_type>;
		using handle_with_context<v8::Map>::handle_with_context;

	private:
		using entries_view = std::ranges::chunk_view<std::views::all_t<const value_for_array&>>;
		struct iterator_transform {
				auto operator()(std::ranges::range_reference_t<entries_view> entry) const -> value_type;
		};

		[[nodiscard]] auto entries() const -> const value_for_array&;

	public:
		using range_type = std::ranges::transform_view<entries_view, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		[[nodiscard]] auto into_range() -> range_type;
		[[nodiscard]] auto size() const -> std::size_t;

	private:
		mutable value_for_array entries_;
};

static_assert(std::ranges::range<value_for_map::range_type>);

} // namespace js::iv8
