module;
#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <variant>
export module auto_js:intrinsics.array_buffer;
import util;

namespace js {

// "Data block" [6.2.9]. v8 calls it `BackingStore`.
export class data_block {
	public:
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		using array_type = std::byte[];
		constexpr static auto max_align_v = 8;
		struct deleter {
				auto operator()(array_type ptr) const -> void {
					operator delete[](ptr, std::align_val_t{max_align_v});
				}
		};
		using shared_pointer_type = std::shared_ptr<array_type>;
		using unique_pointer_type = std::unique_ptr<array_type, deleter>;

	private:
		using view_pointer_type = std::byte*;
		using variant_pointer_type = std::variant<shared_pointer_type, unique_pointer_type, view_pointer_type>;

	public:
		constexpr data_block() : data_block{view_pointer_type{nullptr}, 0} {}
		~data_block() = default;

		// from existing pointer
		constexpr data_block(variant_pointer_type data, size_t byte_length) :
				data_{std::move(data)},
				byte_length_{byte_length} {}

		// from view
		explicit constexpr data_block(std::span<std::byte> data) :
				data_{data.data()},
				byte_length_{data.size()} {
			data_ = copy_or_ref_data();
		}

		// copyable
		constexpr data_block(const data_block& right) :
				data_{right.copy_or_ref_data()},
				byte_length_{right.byte_length_} {}

		auto operator=(const data_block& right) -> data_block& {
			if (this != &right) {
				data_ = right.copy_or_ref_data();
				byte_length_ = right.byte_length_;
			}
			return *this;
		}

		// moveable
		constexpr data_block(data_block&& right) = default;
		auto operator=(data_block&& right) -> data_block& = default;

		// getters
		[[nodiscard]] constexpr auto data() const -> const std::byte* { return get_data(); }
		[[nodiscard]] constexpr auto data() -> std::byte* { return get_data(); }
		[[nodiscard]] constexpr auto size() const -> size_t { return byte_length_; }
		explicit constexpr operator std::span<std::byte>() { return {data(), size()}; }
		explicit constexpr operator std::span<const std::byte>() const { return {data(), size()}; }

		// take ownership of this data block, leaving a moved-from value
		constexpr auto acquire_ownership() && -> variant_pointer_type {
			return std::move(data_);
		}

	private:
		[[nodiscard]] constexpr auto get_data() const -> std::byte* {
			if (data_.index() == std::variant_npos) {
				std::unreachable();
			} else {
				return std::visit(
					util::overloaded{
						[](std::byte* data) -> std::byte* { return data; },
						[](const auto& data) -> std::byte* { return data.get(); },
					},
					data_
				);
			}
		}

		// For `shared_ptr` it returns a pointer to the same shared block. Otherwise a new unique_ptr
		// with a copy of the data.
		[[nodiscard]] constexpr auto copy_or_ref_data() const -> variant_pointer_type {
			return std::visit(
				util::overloaded{
					[ & ](const shared_pointer_type& data) -> variant_pointer_type { return data; },
					[ & ](const auto& /*data*/) -> variant_pointer_type {
						auto data = unique_pointer_type{new (std::align_val_t{max_align_v}) std::byte[ size() ]};
						std::ranges::copy(std::span<const std::byte>{*this}, data.get());
						return data;
					},
				},
				data_
			);
		}

		variant_pointer_type data_;
		size_t byte_length_;
};

export class array_buffer : public data_block {
	public:
		array_buffer() = default;
		explicit array_buffer(data_block&& block) :
				data_block{std::move(block)} {}
};

export class shared_array_buffer : public data_block {
	public:
		shared_array_buffer() = default;
		explicit shared_array_buffer(data_block&& block) :
				data_block{std::move(block)} {}
};

} // namespace js
