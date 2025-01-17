module;
#include <functional>
#include <ranges>
export module napi_js.object;
import isolated_js;
import ivm.utility;
import napi_js.array;
import napi_js.object_like;
import napi_js.value;
import nodejs;

namespace js::napi {

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

		template <class... Entries>
		static auto assign(napi_env env, object_like target, std::tuple<Entries...> entries) -> void;

	private:
		mutable array keys_;
};

template <class... Entries>
auto object::assign(napi_env env, object_like target, std::tuple<Entries...> entries) -> void {
	std::invoke(
		[ & ]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) {
			return (invoke(std::integral_constant<size_t, Index>{}), ...);
		},
		[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
			target.set(
				transfer_in_strict<napi_value>(std::get<Index>(entries).first, env),
				transfer_in_strict<napi_value>(std::get<Index>(entries).second, env)
			);
		},
		std::index_sequence_for<Entries...>{}
	);
}

} // namespace js::napi
