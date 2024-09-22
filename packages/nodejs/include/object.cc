module;
#include <ranges>
#include <string_view>
export module ivm.node:object;
import napi;
import :array;
import ivm.utility;

namespace ivm::napi {

export class object {
	public:
		using value_type = std::pair<Napi::Value, Napi::Value>;

	private:
		class iterator_transform {
			public:
				explicit iterator_transform(Napi::Object object);
				auto operator()(Napi::Value key) const -> value_type;

			private:
				Napi::Object object_;
		};

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<array&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		explicit object(Napi::Object object);
		[[nodiscard]] auto get(std::string_view key) -> Napi::Value;
		[[nodiscard]] auto into_range() const -> range_type;

	private:
		mutable array keys_;
		Napi::Object object_;
};

} // namespace ivm::napi
