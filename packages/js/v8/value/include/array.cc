module;
#include <compare>
#include <cstdint>
#include <ranges>
export module v8_js.array;
import isolated_js;
import ivm.utility;
import v8_js.handle;
import v8_js.lock;
import v8;

namespace js::iv8 {

export class array
		: public v8::Local<v8::Array>,
			public handle_with_context,
			public materializable<array> {
	public:
		class iterator;
		struct handle_data;
		using value_type = v8::Local<v8::Value>;

		array() = default;
		explicit array(const context_lock& lock, v8::Local<v8::Array> handle) :
				Local{handle},
				handle_with_context{lock} {}

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> uint32_t;

	private:
		mutable uint32_t length_{};
};

class array::iterator : public util::random_access_iterator_facade<iterator, int32_t, int64_t> {
	public:
		friend arithmetic_facade;
		using arithmetic_facade::operator+;
		using difference_type = random_access_iterator_facade::difference_type;
		using size_type = uint32_t;
		using value_type = array::value_type;

		iterator() = default;
		iterator(v8::Local<v8::Array> array, v8::Local<v8::Context> context, uint32_t index);

		auto operator*() const -> value_type;
		auto operator+=(difference_type offset) -> iterator&;
		auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		auto operator+() const -> size_type { return index_; }

		v8::Local<v8::Array> array_;
		v8::Local<v8::Context> context_;
		uint32_t index_{};
};

static_assert(std::ranges::range<array>);
static_assert(std::random_access_iterator<array::iterator>);

} // namespace js::iv8
