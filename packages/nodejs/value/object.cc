module;
#include <ranges>
#include <string>
module ivm.node;
import :array;
import :object;
import ivm.utility;
import napi;

namespace ivm::napi {

object::object(Napi::Object object) :
		object_{object} {}

auto object::into_range() const -> range_type {
	if (keys_.value().IsEmpty()) {
		keys_ = array{object_.GetPropertyNames()};
	}
	return keys_ | std::views::transform(iterator_transform{object_});
}

auto object::get(Napi::Value key) const -> Napi::Value {
	return object_.Get(key);
}

object::iterator_transform::iterator_transform(Napi::Object object) :
		object_{object} {}

auto object::iterator_transform::operator()(Napi::Value key) const -> value_type {
	return std::pair{key, object_.Get(key)};
}

} // namespace ivm::napi
