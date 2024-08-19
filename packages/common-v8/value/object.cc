module;
#include <ranges>
export module ivm.v8:object;
import v8;
import :array;
import :handle;
import :utility;
import ivm.utility;

namespace ivm::iv8 {

export class object : public v8::Object {
	public:
		using value_type = std::pair<handle<v8::Value>, handle<v8::Value>>;

	private:
		// We use `array` as the base iteration, over the keys of this object, and transform that into
		// an entry pair.
		class iterator_transform {
			public:
				iterator_transform() = delete;
				iterator_transform(handle_env env, v8::Object* object);
				auto operator()(handle<v8::Value> key) const -> value_type;

			private:
				handle_env env;
				v8::Object* object;
		};

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<array_handle&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		// Some good thoughts here. It's strange that there isn't an easier way to transform an
		// underlying range.
		// https://brevzin.github.io/c++/2024/05/18/range-customization/
		[[nodiscard]] auto into_range(handle_env env, array_handle& keys) const -> range_type;
		static auto Cast(v8::Value* data) -> object*;

	private:
		[[nodiscard]] auto transform(handle_env env, array_handle& keys) const -> range_type;
};

export using object_handle = handle<object, mutable_value<array_handle>>;
static_assert(std::ranges::range<object::range_type>);
static_assert(std::random_access_iterator<object::iterator>);

auto object::Cast(v8::Value* data) -> object* {
	return reinterpret_cast<object*>(v8::Object::Cast(data));
}

auto object::into_range(handle_env env, array_handle& keys) const -> range_type {
	auto* non_const = const_cast<object*>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast)
	if (keys == array_handle{}) {
		auto property_names = unmaybe(non_const->GetPropertyNames(
			env.context,
			v8::KeyCollectionMode::kOwnOnly,
			v8::PropertyFilter::ONLY_ENUMERABLE,
			v8::IndexFilter::kIncludeIndices,
			v8::KeyConversionMode::kConvertToString
		));
		keys = array_handle{property_names.As<iv8::array>(), env, 0};
	}
	return keys | std::views::transform(iterator_transform{env, non_const});
}

object::iterator_transform::iterator_transform(handle_env env, v8::Object* object) :
		env{env}, object{object} {}

auto object::iterator_transform::operator()(handle<v8::Value> key) const -> value_type {
	auto value = unmaybe(object->GetRealNamedProperty(env.context, key.As<v8::Name>()));
	return std::make_pair(key, handle{value, env});
}

} // namespace ivm::iv8
