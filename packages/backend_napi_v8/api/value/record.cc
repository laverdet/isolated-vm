module isolated_vm;
import :support.cast;
import :support.lock;
import :value;
import auto_js;
import v8_js;
import std;

namespace isolated_vm {
using namespace js;

// value_for_record
auto value_for_record::set(const runtime_lock& lock, value_of<> key, value_of<> value) const -> void {
	iv8::unmaybe(cast_in(*this)->Set(unwrap_lock_witness(lock).context(), cast_in(key), cast_in(value)));
}

auto value_for_record::make(const runtime_lock& lock) -> value_of<record_tag> {
	return value_of<dictionary_tag>::from(cast_out(v8::Object::New(unwrap_lock_witness(lock).isolate())));
}

// bound_value_for_record
constexpr auto get_property(const runtime_lock& lock, value_of<object_tag> object, value_of<name_tag> key) -> value_of<> {
	return cast_out(iv8::unmaybe(cast_in(object)->GetRealNamedProperty(unwrap_lock_witness(lock).context(), cast_in(key))));
}

constexpr auto get_property(const runtime_lock& lock, value_of<object_tag> object, value_of<number_tag> key) -> value_of<> {
	return cast_out(iv8::unmaybe(cast_in(object)->Get(unwrap_lock_witness(lock).context(), cast_in(key).As<v8::Uint32>()->Value())));
}

constexpr auto get_property(const runtime_lock& lock, value_of<object_tag> object, value_of<primitive_tag> key) -> value_of<> {
	if (cast_in(key)->IsString() || !cast_in(key)->IsNumber()) {
		return get_property(lock, object, value_of<name_tag>::from(key));
	} else {
		return get_property(lock, object, value_of<number_tag>::from(key));
	}
}

constexpr auto has_property(const runtime_lock& lock, value_of<object_tag> object, value_of<name_tag> key) -> bool {
	return iv8::unmaybe(cast_in(object)->HasRealNamedProperty(unwrap_lock_witness(lock).context(), cast_in(key)));
}

constexpr auto has_property(const runtime_lock& lock, value_of<object_tag> object, value_of<number_tag> key) -> bool {
	return iv8::unmaybe(cast_in(object)->HasRealIndexedProperty(unwrap_lock_witness(lock).context(), cast_in(key).As<v8::Uint32>()->Value()));
}

constexpr auto has_property(const runtime_lock& lock, value_of<object_tag> object, value_of<primitive_tag> key) -> bool {
	if (cast_in(key)->IsString() || !cast_in(key)->IsNumber()) {
		return has_property(lock, object, value_of<name_tag>::from(key));
	} else {
		return has_property(lock, object, value_of<number_tag>::from(key));
	}
}

auto bound_value_for_record::keys() const -> const internal_keys_type& {
	if (!keys_) {
		auto context = unwrap_lock_witness(lock()).context();
		auto object = cast_in(value_of{*this});
		auto names = iv8::unmaybe(object->GetPropertyNames(
			context,
			v8::KeyCollectionMode::kOwnOnly,
			static_cast<v8::PropertyFilter>(v8::PropertyFilter::ONLY_ENUMERABLE | v8::PropertyFilter::SKIP_SYMBOLS),
			v8::IndexFilter::kIncludeIndices,
			v8::KeyConversionMode::kKeepNumbers
		));
		keys_ = isolated_vm::bound_value{lock(), value_of<vector_tag>::from(cast_out(names))};
	}
	return keys_;
}

auto bound_value_for_record::get(value_of<name_tag> key) const -> mapped_type {
	return get_property(lock(), value_of{*this}, key);
}

auto bound_value_for_record::get(value_of<number_tag> key) const -> mapped_type {
	return get_property(lock(), value_of{*this}, key);
}

auto bound_value_for_record::get(value_of<primitive_tag> key) const -> mapped_type {
	return get_property(lock(), value_of{*this}, key);
}

auto bound_value_for_record::has(value_of<name_tag> key) const -> bool {
	return has_property(lock(), value_of{*this}, key);
}

auto bound_value_for_record::has(value_of<number_tag> key) const -> bool {
	return has_property(lock(), value_of{*this}, key);
}

auto bound_value_for_record::has(value_of<primitive_tag> key) const -> bool {
	return has_property(lock(), value_of{*this}, key);
}

auto bound_value_for_record::into_range() const -> range_type {
	return std::views::transform(keys(), iterator_transform{*this});
}

auto bound_value_for_record::size() const -> std::size_t {
	return keys().size();
}

// bound_value_for_record::iterator_transform
bound_value_for_record::iterator_transform::iterator_transform(const bound_value_for_record& subject) :
		subject_{&subject} {}

auto bound_value_for_record::iterator_transform::operator()(value_of<> key) const -> value_type {
	auto key_as = value_of<primitive_tag>::from(key);
	return {key_as, subject_->get(key_as)};
}

} // namespace isolated_vm
