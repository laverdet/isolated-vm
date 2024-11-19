module;
#include <ranges>
#include <string_view>
export module ivm.napi:object;
import :array;
import :container;
import ivm.utility;
import napi;

namespace ivm::napi {

export class object : public container {
	public:
		using value_type = std::pair<napi_value, napi_value>;
		using container::container;

	private:
		class iterator_transform {
			public:
				explicit iterator_transform(const object& subject_);
				auto operator()(napi_value key) const -> value_type;

			private:
				const object* subject_;
		};

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<array&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		[[nodiscard]] auto get(napi_value key) const -> napi_value;
		[[nodiscard]] auto has(napi_value key) const -> bool;
		[[nodiscard]] auto into_range() const -> range_type;

	private:
		mutable array keys_;
};

} // namespace ivm::napi
