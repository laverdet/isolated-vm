export module auto_js:intrinsics.array_buffer;
import :reference_of;
import std;
import util;

namespace js {

// "Data block" [6.2.9]. v8 calls it `BackingStore`.
export class data_block {
	public:
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		using array_type = std::byte[];
		constexpr static auto max_align_v = 8;

		data_block() = default;
		explicit data_block(std::size_t byte_length) : byte_length_{byte_length} {}
		[[nodiscard]] constexpr auto size() const -> std::size_t { return byte_length_; }

	private:
		std::size_t byte_length_{};
};

// `ArrayBuffer`
export class array_buffer : public data_block {
	private:
		struct deleter {
				auto operator()(array_type ptr) const -> void {
					operator delete[](ptr, std::align_val_t{max_align_v});
				}
		};

	public:
		using unique_pointer_type = std::unique_ptr<array_type, deleter>;
		array_buffer() = default;
		~array_buffer() = default;

		// from view
		explicit constexpr array_buffer(std::span<const std::byte> data) :
				data_block{data.size()},
				data_{unique_pointer_type{new (std::align_val_t{max_align_v}) std::byte[ size() ]}} {
			std::ranges::copy(data, data_.get());
		}

		// copyable
		constexpr array_buffer(const array_buffer& right) :
				array_buffer{std::span<const std::byte>{right}} {}

		auto operator=(const array_buffer& right) -> array_buffer& {
			if (this != &right) {
				data_block::operator=(right);
				data_ = array_buffer{right}.data_;
			}
			return *this;
		}

		// moveable
		constexpr array_buffer(array_buffer&& right) = default;
		auto operator=(array_buffer&& right) -> array_buffer& = default;

		// getters
		[[nodiscard]] constexpr auto data() const -> const std::byte* { return data_.get(); }
		[[nodiscard]] constexpr auto data() -> std::byte* { return data_.get(); }
		explicit constexpr operator std::span<std::byte>() { return {data(), size()}; }
		explicit constexpr operator std::span<const std::byte>() const { return {data(), size()}; }

		// take ownership of this data block, leaving a moved-from value
		constexpr auto acquire_ownership() && -> unique_pointer_type { return std::move(data_); }

	private:
		unique_pointer_type data_;
};

// `SharedArrayBuffer`
export class shared_array_buffer : public data_block {
	public:
		using shared_pointer_type = std::shared_ptr<array_type>;
		shared_array_buffer() = default;

		// from existing data block
		explicit constexpr shared_array_buffer(std::size_t byte_length, shared_pointer_type data) :
				data_block{byte_length},
				data_{std::move(data)} {}

		// getters
		[[nodiscard]] constexpr auto data() const -> const std::byte* { return data_.get(); }
		[[nodiscard]] constexpr auto data() -> std::byte* { return data_.get(); }
		explicit constexpr operator std::span<std::byte>() { return {data(), size()}; }
		explicit constexpr operator std::span<const std::byte>() const { return {data(), size()}; }

		// take ownership of this data block, leaving a moved-from value
		constexpr auto acquire_ownership() && -> shared_pointer_type { return std::move(data_); }

	private:
		shared_pointer_type data_;
};

// Any data block type
export struct data_block_variant : public std::variant<reference_of<array_buffer>, reference_of<shared_array_buffer>> {
	public:
		using value_type = std::variant<reference_of<array_buffer>, reference_of<shared_array_buffer>>;
		using value_type::value_type;

		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr data_block_variant(value_type value) : value_type{value} {}
};

// `Uint8Array`, `Int16Array`, etc.
// nb: Type:void is `DataView`
export template <class Type>
class typed_array {
	public:
		typed_array() = default;

		// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
		typed_array(data_block_variant buffer, std::size_t byte_offset, std::size_t length) :
				data_{buffer},
				byte_offset_{byte_offset},
				length_{length} {}

		[[nodiscard]] constexpr auto buffer() && -> data_block_variant { return data_; }
		[[nodiscard]] constexpr auto byte_offset() const -> std::size_t { return byte_offset_; }
		[[nodiscard]] constexpr auto size() const -> std::size_t { return length_; }

	private:
		data_block_variant data_;
		std::size_t byte_offset_{};
		std::size_t length_{};
};

} // namespace js
