module;
#include <ranges>
export module napi_js.dictionary;
import isolated_js;
import ivm.utility;
import napi_js.array;
import napi_js.environment;
import napi_js.object;
import napi_js.value;
import nodejs;

namespace js::napi {

// Weird naming here because an object `{}` is a typeof "object", but lots of other things are
// objects. Also, the object iterator is implemented as transformation on a property names vector
// (which is also an object). So this needs to be its own class or it is a circular inheritance.
class dictionary_like : public bound_value<object_tag> {
	public:
		using bound_value<object_tag>::bound_value;
		using keys_type = bound_value<vector_tag>;
		using value_type = std::pair<napi_value, napi_value>;

	private:
		class iterator_transform {
			public:
				explicit iterator_transform(const dictionary_like& subject_);
				auto operator()(napi_value key) const -> value_type;

			private:
				const dictionary_like* subject_;
		};

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<keys_type&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		[[nodiscard]] auto into_range() const -> range_type;

	private:
		mutable keys_type keys_{nullptr, value<vector_tag>::from(nullptr)};
};

template <>
class bound_value<list_tag> : public dictionary_like {
	public:
		using dictionary_like::dictionary_like;
};

template <>
class bound_value<dictionary_tag> : public dictionary_like {
	public:
		using dictionary_like::dictionary_like;
};

} // namespace js::napi
