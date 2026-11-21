export module util:memory.header_array;
import :functional.function_constant;
import std;

namespace util {

// A `Header` followed by an array of `Element`, whose length is chosen at runtime. The array is
// also resizable.
export template <class Header, class Element>
class header_array {
	public:
		static_assert(std::is_trivially_copyable_v<Element>);
		// `realloc` has no over-aligned counterpart
		static_assert(alignof(Header) <= alignof(std::max_align_t) && alignof(Element) <= alignof(std::max_align_t));

		header_array(const header_array&) = delete;
		~header_array() { std::destroy_at(&header_); }
		auto operator=(const header_array&) -> header_array& = delete;

		static auto make(std::size_t size, auto&&... args) -> std::unique_ptr<header_array>
			requires std::constructible_from<Header, decltype(args)...> {
			// Temporary `unique_ptr` in case the constructor throws
			auto memory = std::unique_ptr<header_array, function_constant<&std::free>>{allocate(nullptr, size)};
			if (!memory) {
				throw std::bad_alloc{};
			}
			auto result = std::unique_ptr<header_array>{::new (memory.get()) header_array{size, std::forward<decltype(args)>(args)...}};
			std::ignore = memory.release();
			return result;
		}

		static auto resize(std::unique_ptr<header_array>& self, std::size_t size) -> void {
			static_assert(std::is_nothrow_move_constructible_v<Header>);
			auto header = std::move(self->header_);
			auto* holder = self.release();
			std::destroy_at(&holder->header_);
			if (auto* memory = allocate(holder, size)) {
				self.reset(::new (memory) header_array{size, std::move(header)});
			} else {
				std::construct_at(&holder->header_, std::move(header));
				self.reset(holder);
				throw std::bad_alloc{};
			}
		}

		static auto operator delete(header_array* self, std::destroying_delete_t /*destroying*/) -> void {
			std::destroy_at(self);
			// NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
			std::free(self);
		}

		[[nodiscard]] auto header(this auto& self) -> auto& { return self.header_; }
		[[nodiscard]] auto size() const -> std::size_t { return size_; }
		[[nodiscard]] auto elements(this auto& self) -> auto {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			return std::span{self.elements_ + header_element_count, self.size_};
		}

	private:
		constexpr static auto header_element_count = (sizeof(Header) + sizeof(Element) - 1) / sizeof(Element);

		explicit header_array(std::size_t size, auto&&... args) :
				size_{size},
				header_{std::forward<decltype(args)>(args)...} {}

		static auto allocate(header_array* memory, std::size_t size) -> header_array* {
			// `void*` cast to make gcc happy, `-Wclass-memaccess`
			// NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
			return static_cast<header_array*>(std::realloc(static_cast<void*>(memory), sizeof(header_array) + (size * sizeof(Element))));
		}

		std::size_t size_;
		union {
				Header header_;
				// NOLINTNEXTLINE(modernize-avoid-c-arrays)
				Element elements_[ header_element_count ];
		};
};

} // namespace util
