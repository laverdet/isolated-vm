export module napi_js:record;
import :array;
import :bound_value;
import :object;
import :value_handle;
import auto_js;
import std;

namespace js::napi {

class bound_value_for_record : public bound_value_next<record_tag> {
	public:
		using bound_value_next<record_tag>::bound_value_next;
		using keys_type = bound_value<vector_tag>;
		using key_type = value<primitive_tag>;
		using mapped_type = value<value_tag>;
		using value_type = std::pair<key_type, mapped_type>;

	private:
		class iterator_transform {
			public:
				explicit iterator_transform(const bound_value_for_record& subject_);
				auto operator()(value<value_tag> key) const -> value_type;

			private:
				const bound_value_for_record* subject_;
		};

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<const keys_type&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		[[nodiscard]] auto into_range() const -> range_type;
		[[nodiscard]] auto size() const -> std::size_t;

	private:
		[[nodiscard]] auto keys() const -> const keys_type&;

		mutable keys_type keys_;
};

} // namespace js::napi
