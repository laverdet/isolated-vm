module;
#include <cassert>
#include <unordered_map>
export module v8_js.weak_map;
import ivm.utility;
import v8_js.identity_hash;
import v8_js.lock;
import v8;

namespace js::iv8 {

template <class Key, class Container>
class weak_map_handle {
	public:
		weak_map_handle(v8::Isolate* isolate, Container& container, v8::Local<Key> local_key) noexcept :
				container_{&container},
				key_{isolate, local_key},
				identity_hash_{local_key->GetIdentityHash()} {
			key_.SetWeak(this, weak_callback, v8::WeakCallbackType::kParameter);
		}

		[[nodiscard]] auto GetIdentityHash() const -> int { return identity_hash_; }

	private:
		static auto weak_callback(const v8::WeakCallbackInfo<weak_map_handle>& info) -> void;

		Container* container_;
		v8::Global<Key> key_;
		int identity_hash_;
};

export template <class Key, class Value>
class weak_map : util::non_moveable {
	private:
		using handle_type = weak_map_handle<Key, weak_map>;
		friend handle_type;
		using hasher = identity_hash;
		using key_equal = util::hash_compare<identity_hash>;
		using container_type = std::unordered_map<handle_type, Value, hasher, key_equal>;

	public:
		// NOTE: All iterator comparisons, I guess, need to take place in this file. Otherwise we get a
		// missing symbol of `std::__detail::operator==(std::__detail::_Node_iterator_base, ...)` which
		// I'm pretty sure is not my fault. clang v19.1.3
		[[nodiscard]] auto find(auto&& key) const -> const Value*;
		auto emplace(const isolate_lock& lock, std::pair<v8::Local<Key>, Value> entry) -> bool;
		auto extract(const isolate_lock& lock, auto&& key) -> Value;

	private:
		container_type map_;
};

// weak_map_handle
template <class Key, class Container>
auto weak_map_handle<Key, Container>::weak_callback(const v8::WeakCallbackInfo<weak_map_handle>& info) -> void {
	auto* handle = info.GetParameter();
	auto& map = (*handle->container_).map_;
	auto it = map.find(*handle);
	assert(it != map.end());
	map.erase(it);
}

// weak_map
template <class Key, class Value>
auto weak_map<Key, Value>::find(auto&& key) const -> const Value* {
	auto pos = map_.find(std::forward<decltype(key)>(key));
	if (pos == map_.end()) {
		return nullptr;
	} else {
		return &pos->second;
	}
}

template <class Key, class Value>
auto weak_map<Key, Value>::emplace(const isolate_lock& lock, std::pair<v8::Local<Key>, Value> entry) -> bool {
	auto result = map_.emplace(
		std::piecewise_construct,
		std::forward_as_tuple(lock.isolate(), *this, std::move(entry.first)),
		std::tuple{std::move(entry.second)}
	);
	return result.second;
}

template <class Key, class Value>
auto weak_map<Key, Value>::extract(const isolate_lock& /*lock*/, auto&& key) -> Value {
	auto it = map_.find(std::forward<decltype(key)>(key));
	assert(it != map_.end());
	auto value = std::move(it->second);
	map_.erase(it);
	return value;
}

} // namespace js::iv8
