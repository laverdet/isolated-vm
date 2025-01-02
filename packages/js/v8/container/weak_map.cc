module;
#include <memory>
#include <unordered_map>
export module ivm.iv8:weak_map;
import v8;
import ivm.utility;

namespace ivm::js::iv8 {

template <class Key, class Container>
class weak_map_handle : util::non_copyable {
	public:
		struct hash;
		friend hash;
		using compare = util::hash_comparator<hash>;

		weak_map_handle(v8::Isolate* isolate, Container& container, v8::Local<Key> local_key) noexcept :
				container_{&container},
				isolate_{isolate},
				key_{isolate, local_key},
				identity_hash_{local_key->GetIdentityHash()} {
			key_.SetWeak(this, weak_callback, v8::WeakCallbackType::kParameter);
		}
		weak_map_handle(const weak_map_handle&) = delete;
		weak_map_handle(weak_map_handle&& right) noexcept :
				weak_map_handle{right.isolate_, *right.container_, std::move(right.key_)} {}
		~weak_map_handle() = default;
		auto operator=(const weak_map_handle&) = delete;
		auto operator=(weak_map_handle&&) = delete;

	private:
		static auto weak_callback(const v8::WeakCallbackInfo<weak_map_handle>& info) -> void;

		Container* container_;
		v8::Isolate* isolate_;
		v8::Global<Key> key_;
		int identity_hash_;
};

export template <class Key, class Value>
class weak_map : util::non_moveable {
	private:
		using handle_type = weak_map_handle<Key, weak_map>;
		friend handle_type;

	public:
		auto find(auto&& key) const -> const Value*;
		auto insert(v8::Isolate* isolate, std::pair<v8::Local<Key>, Value> entry) -> void;

	private:
		using container_type = std::unordered_map<
			std::unique_ptr<handle_type>,
			Value,
			typename handle_type::hash,
			typename handle_type::compare>;
		container_type map_;
};

// weak_map_handle::hash
template <class Key, class Container>
struct weak_map_handle<Key, Container>::hash : std::hash<int> {
		using is_transparent = void;
		using transparent_key_equal = weak_map_handle::compare;
		using std::hash<int>::operator();
		auto operator()(const weak_map_handle& handle) const -> bool { return (*this)(handle.identity_hash_); }
		auto operator()(const std::unique_ptr<weak_map_handle>& handle) const -> bool { return (*this)(*handle); }
		auto operator()(v8::Local<Key> handle) const -> bool { return (*this)(handle->GetIdentityHash()); }
};

// weak_map_handle
template <class Key, class Container>
auto weak_map_handle<Key, Container>::weak_callback(const v8::WeakCallbackInfo<weak_map_handle>& info) -> void {
	auto& handle = *info.GetParameter();
	auto it = handle.container_->map_.find(handle);
	if (it != handle.container_->map_.end()) {
		handle.container_->map_.erase(it);
	}
}

// weak_map
template <class Key, class Value>
auto weak_map<Key, Value>::find(auto&& key) const -> const Value* {
	auto it = map_.find(key);
	if (it == map_.end()) {
		return nullptr;
	} else {
		return &it->second;
	}
}

template <class Key, class Value>
auto weak_map<Key, Value>::insert(v8::Isolate* isolate, std::pair<v8::Local<Key>, Value> entry) -> void {
	map_.emplace(
		std::make_unique<handle_type>(isolate, *this, std::move(entry.first)),
		std::move(entry.second)
	);
}

} // namespace ivm::js::iv8
