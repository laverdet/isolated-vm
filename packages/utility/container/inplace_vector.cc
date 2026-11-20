export module util:container.inplace_vector;
import std;

namespace util {

// Minimal polyfill for std::inplace_vector
export template <class Type, std::size_t Size>
class inplace_vector : private std::allocator<Type> {
	public:
		constexpr inplace_vector() = default;

		template <std::ranges::forward_range Range>
		constexpr inplace_vector(std::from_range_t /*from_range*/, Range&& range) :
				size_{static_cast<std::size_t>(std::ranges::distance(range))} {
			if (size_ > Size) {
				throw std::range_error{"capacity exceeded"};
			}
			auto index = std::size_t{0};
			for (auto&& value : std::forward<Range>(range)) {
				allocator_traits::destroy(*this, &data_[ index ]);
				allocator_traits::construct(*this, &data_[ index ], std::forward<decltype(value)>(value));
				++index;
			}
		}

		[[nodiscard]] constexpr auto empty() const -> bool { return size_ == 0; }
		[[nodiscard]] constexpr auto size() const -> std::size_t { return size_; }
		constexpr auto back(this auto& self) -> auto& { return self.data_[ self.size_ - 1 ]; }
		constexpr auto begin(this auto& self) { return self.data_.begin(); }
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		constexpr auto end(this auto& self) { return self.data_.begin() + self.size_; }
		constexpr auto data(this auto& self) { return self.data_.data(); }
		constexpr auto operator[](this auto& self, std::size_t index) -> auto& { return self.data_[ index ]; }
		constexpr auto front(this auto& self) -> auto& { return self.data_.front(); }

		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr operator std::span<Type>() { return {data(), size()}; }
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr operator std::span<const Type>() const { return {data(), size()}; }

		constexpr auto append_range(std::ranges::input_range auto&& range) -> void {
			for (auto&& value : range) {
				emplace_back(std::forward<decltype(value)>(value));
			}
		}

		constexpr auto clear() -> void {
			while (size_ > 0) {
				pop_back();
			}
		}

		constexpr auto pop_back() -> void {
			allocator_traits::destroy(*this, &back());
			--size_;
			if consteval {
				allocator_traits::construct(*this, &data_[ size_ ]);
			} else {
				if constexpr (!std::is_trivially_destructible_v<Type>) {
					allocator_traits::construct(*this, &data_[ size_ ]);
				}
			}
		}

		constexpr auto emplace_back(auto&&... args) -> void {
			if (size_ >= Size) {
				throw std::range_error{"capacity exceeded"};
			}
			allocator_traits::destroy(*this, &data_[ size_ ]);
			++size_;
			allocator_traits::construct(*this, &data_[ size_ - 1 ], std::forward<decltype(args)>(args)...);
		}

	private:
		using allocator_traits = std::allocator_traits<std::allocator<Type>>;
		std::array<Type, Size> data_;
		std::size_t size_ = 0;
};

} // namespace util
