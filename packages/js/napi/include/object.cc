module;
#include <ranges>
export module ivm.napi:object;
import :array;
import :object_like;
import :value;
import ivm.js;
import ivm.utility;
import napi;

namespace ivm::js::napi {

export class object : public object_like {
	public:
		using value_type = std::pair<napi_value, napi_value>;

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

		object(napi_env env, value<js::object_tag> value);
		[[nodiscard]] auto into_range() const -> range_type;

	private:
		mutable array keys_;
};

} // namespace ivm::js::napi
