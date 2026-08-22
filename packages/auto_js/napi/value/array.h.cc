export module napi_js:array;
import :object;
import auto_js;
import util;

namespace js::napi {

class value_for_vector : public value_next<vector_tag> {
	public:
		class iterator;
		using value_type = local_of<>;

		value_for_vector() = default;
		value_for_vector(environment_lock_witness lock, local_of<vector_tag> value, bool maybe_sparse = true) :
				value_next{lock, value},
				maybe_sparse_{maybe_sparse} {}

		[[nodiscard]] auto begin() const -> iterator;
		[[nodiscard]] auto end() const -> iterator;
		[[nodiscard]] auto size() const -> std::uint32_t;

	private:
		bool maybe_sparse_ : 1 = true;
		mutable std::uint32_t size_ : 31 {};
};

class value_for_vector::iterator : public util::random_access_iterator_facade<std::int32_t, std::int64_t> {
	public:
		using arithmetic_facade::operator+;
		using difference_type = arithmetic_facade::difference_type;
		using size_type = std::uint32_t;
		using value_type = value_for_vector::value_type;

		iterator() = default;
		iterator(value_for_vector subject, size_type index);

		auto operator*() const -> value_type;

		auto operator+=(difference_type offset) -> iterator& {
			index_ += offset;
			return *this;
		}

		auto operator==(const iterator& right) const -> bool { return index_ == right.index_; }
		auto operator<=>(const iterator& right) const -> std::strong_ordering { return index_ <=> right.index_; }

	private:
		auto operator+() const -> size_type { return index_; }

		value_for_vector subject_;
		size_type index_{};
};

} // namespace js::napi
