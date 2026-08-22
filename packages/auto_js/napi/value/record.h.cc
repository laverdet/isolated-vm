export module napi_js:record;
import :array;
import auto_js;
import std;

namespace js::napi {

class value_for_record : public value_next<record_tag> {
	public:
		using value_next<record_tag>::value_next;
		using keys_type = value_of<vector_tag>;
		using key_type = local_of<primitive_tag>;
		using mapped_type = local_of<>;
		using value_type = std::pair<key_type, mapped_type>;

	private:
		class iterator_transform {
			public:
				explicit iterator_transform(const value_for_record& subject_);
				auto operator()(local_of<> key) const -> value_type;

			private:
				const value_for_record* subject_;
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

class value_for_list : public value_next<list_tag> {
	public:
		using value_next<list_tag>::value_next;
		auto values() const -> value_of<vector_tag>;
};

} // namespace js::napi
