export module napi_js:container;
import :environment;
import :utility;
import nodejs;
import std;
import util;

namespace js::napi {

// Map container for napi values
template <class Type>
struct virtual_value_map {
	public:
		using container_type = std::unordered_map<napi_value, Type>;
		using iterator = container_type::iterator;
		using key_type = container_type::key_type;
		using mapped_type = container_type::mapped_type;

		virtual auto end() const -> iterator = 0;
		virtual auto find(napi_value key) const -> iterator = 0;
		virtual auto try_emplace(napi_value key, Type value) -> std::pair<iterator, bool> = 0;
};

template <class Map>
struct concrete_value_map final : virtual_value_map<typename Map::mapped_type> {
	public:
		using container_type = Map;
		using typename virtual_value_map<typename Map::mapped_type>::mapped_type;
		using typename virtual_value_map<typename Map::mapped_type>::iterator;

		virtual auto end() const -> iterator {
			return std::bit_cast<iterator>(map_.end());
		}

		virtual auto find(napi_value key) const -> iterator {
			// nb: The iterator type between `std::unordered_map<int, int, left>` and
			// `std::unordered_map<int, int, right>` are not technically compatible.
			return std::bit_cast<iterator>(map_.find(key));
		}

		virtual auto try_emplace(napi_value key, mapped_type value) -> std::pair<iterator, bool> {
			auto [ it, inserted ] = map_.try_emplace(key, std::move(value));
			return std::pair{std::bit_cast<iterator>(it), inserted};
		}

	private:
		container_type map_;
};

export template <class Type>
class value_map {
	private:
		using direct_map_type = concrete_value_map<std::unordered_map<napi_value, Type, direct_address_hash, direct_address_equal>>;
		using indirect_map_type = concrete_value_map<std::unordered_map<napi_value, Type, indirect_address_hash, indirect_address_equal>>;
		using virtual_map_type = util::covariant_value<virtual_value_map<Type>, direct_map_type, indirect_map_type>;

	public:
		using container_type = virtual_value_map<Type>;
		using iterator = container_type::iterator;
		using key_type = container_type::key_type;
		using mapped_type = container_type::mapped_type;

		explicit value_map(const environment& env) :
				map_{
					env.uses_direct_handles()
						? virtual_map_type{direct_map_type{}}
						: virtual_map_type{indirect_map_type{}}
				} {}

		auto end() const -> iterator { return map_->end(); }
		auto find(key_type key) const -> iterator { return map_->find(key); }
		auto try_emplace(key_type key, mapped_type value) -> std::pair<iterator, bool> { return map_->try_emplace(key, std::move(value)); }

	private:
		virtual_map_type map_;
};

} // namespace js::napi
