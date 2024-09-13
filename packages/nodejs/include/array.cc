module;
#include <compare>
#include <cstdint>
// Fix visibility bug on llvm 18
// /__w/ivm/ivm/packages/nodejs/value/array.cc:9:20: error: declaration of 'Array' must be imported from module 'napi' before it is required
//     9 | array::array(Napi::Array array) :
// /__w/ivm/ivm/packages/third_party/napi.cc:8:20: note: declaration here is not visible
//     8 | export using Napi::Array;
#include <napi.h>
export module ivm.node:array;
import ivm.utility;
import napi;

namespace ivm::napi {

export class array {
	public:
		class iterator;
		using value_type = Napi::Value;

		array() = default;
		explicit array(Napi::Array array);

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> uint32_t;
		auto value() -> Napi::Value&;

	private:
		Napi::Array array_;
};

class array::iterator : public util::arithmetic_facade<iterator, int32_t, int64_t> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = uint32_t;
		using value_type = array::value_type;

		iterator() = default;
		iterator(Napi::Array array, size_type index);

		auto operator*() const -> value_type;
		auto operator->() const -> value_type { return **this; }
		auto operator[](difference_type offset) const -> value_type { return *(*this + offset); }

		auto operator+=(difference_type offset) -> iterator& {
			index += offset;
			return *this;
		}

		auto operator==(const iterator& right) const -> bool { return index == right.index; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index <=> right.index; }

	private:
		auto operator+() const -> size_type { return index; }

		Napi::Array array;
		size_type index{};
};

} // namespace ivm::napi
