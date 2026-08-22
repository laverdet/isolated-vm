export module v8_js:array;
import :handle;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

class value_for_array : public handle_with_context<v8::Array> {
	public:
		class iterator;
		struct handle_data;
		using value_type = v8::Local<v8::Value>;

		value_for_array() = default;
		explicit value_for_array(v8::Local<v8::Context> context, v8::Local<v8::Array> array, bool maybe_sparse = true) :
				handle_with_context{context, array},
				maybe_sparse_{maybe_sparse} {}
		explicit value_for_array(context_lock_witness lock, v8::Local<v8::Array> array, bool maybe_sparse = true) :
				value_for_array{lock.context(), array, maybe_sparse} {}

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> std::uint32_t;

	private:
		bool maybe_sparse_ : 1 = true;
		mutable std::uint32_t length_ : 31 {};
};

class value_for_array::iterator : public util::random_access_iterator_facade<std::int32_t, std::int64_t> {
	public:
		using arithmetic_facade::operator+;
		using difference_type = random_access_iterator_facade::difference_type;
		using size_type = std::uint32_t;
		using value_type = value_for_array::value_type;

		iterator() = default;
		iterator(v8::Local<v8::Array> array, v8::Local<v8::Context> context, std::uint32_t index, bool maybe_sparse);

		auto operator*() const -> value_type;
		auto operator+=(difference_type offset) -> iterator&;
		auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		auto operator+() const -> size_type { return index_; }

		v8::Local<v8::Array> array_;
		v8::Local<v8::Context> context_;
		std::uint32_t index_{};
		bool maybe_sparse_{};
};

static_assert(std::ranges::range<value_for_array>);
static_assert(std::random_access_iterator<value_for_array::iterator>);

} // namespace js::iv8
