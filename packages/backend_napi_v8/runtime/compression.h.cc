module;
#include <zlib.h>
export module backend_napi_v8:compression;
import :lock;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// https://compression.spec.whatwg.org/#enumdef-compressionformat
enum class compression_format : std::uint8_t {
	deflate,
	deflate_raw,
	gzip,
};

// Linked list node of output array buffer chunk
struct output_chunk {
		using node_type = util::header_array<output_chunk, std::byte>;

		static auto release(std::unique_ptr<node_type> chunk) -> js::array_buffer;

		std::unique_ptr<node_type> next;
		std::size_t written = 0;
};

// Output of a write, as a list of chunks. zlib writes straight into a chunk, and when it fills up a larger one follows.
class output_chunk_list {
	public:
		using range_type = js::tagged_range_of<js::vector_tag, output_chunk_list>;

		class iterator : public util::incrementable_facade {
			public:
				using incrementable_facade::operator++;
				using value_type = std::unique_ptr<output_chunk::node_type>;

				iterator() = default;
				explicit iterator(std::unique_ptr<output_chunk::node_type>& owner) :
						owner_{owner ? &owner : nullptr},
						next_{next_after(owner_)} {}

				auto operator*() const -> std::unique_ptr<output_chunk::node_type>& { return *owner_; }
				auto operator++() -> iterator& {
					owner_ = next_;
					next_ = next_after(owner_);
					return *this;
				}
				auto operator==(const iterator& right) const -> bool { return owner_ == right.owner_; }

			private:
				static auto next_after(std::unique_ptr<output_chunk::node_type>* current) -> std::unique_ptr<output_chunk::node_type>* {
					if (current == nullptr) {
						return nullptr;
					}
					auto& next = (*current)->header().next;
					return next ? &next : nullptr;
				}

				std::unique_ptr<output_chunk::node_type>* owner_ = nullptr;
				std::unique_ptr<output_chunk::node_type>* next_ = nullptr;
		};

		output_chunk_list() = default;
		output_chunk_list(output_chunk_list&& right) noexcept :
				head_{std::move(right.head_)},
				tail_{right.tail_},
				size_{right.size_} {}
		~output_chunk_list() = default;
		auto operator=(output_chunk_list&& right) noexcept -> output_chunk_list& = delete;

		auto begin() -> iterator { return iterator{head_}; }
		static auto end() -> iterator { return iterator{}; }
		[[nodiscard]] auto back() -> output_chunk::node_type& { return *tail_; }
		[[nodiscard]] auto empty() const -> bool { return size_ == 0; }
		[[nodiscard]] auto size() const -> std::size_t { return size_; }

		// Unwritten space in the last chunk, which is empty when there is none
		auto space() -> std::span<std::byte>;
		// Records `size` bytes written at the front of the last `space()`
		auto commit(std::size_t size) -> void;
		// Starts a new chunk, the next size up, with `bytes`
		auto append(std::span<const std::byte> bytes) -> void;

	private:
		auto push_back(std::unique_ptr<output_chunk::node_type> chunk) -> void;

		std::unique_ptr<output_chunk::node_type> head_;
		output_chunk::node_type* tail_ = nullptr;
		std::size_t size_ = 0;
};

static_assert(std::ranges::forward_range<output_chunk_list>);
static_assert(std::ranges::sized_range<output_chunk_list>);

// Result of `write`
struct codec_result {
		output_chunk_list::range_type output;
		std::uint32_t bytes_read;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"bytesRead">, &codec_result::bytes_read},
			js::struct_member{util::cw<"output">, &codec_result::output},
		};
};

// `z_stream` helper class
class zlib_stream : util::non_moveable, public util::pointer_facade {
	public:
		auto operator*(this auto& self) -> auto& { return self.stream_; }

		[[noreturn]] auto throw_zlib_error(int result) const -> void;
		auto pump(auto step, int flush, std::span<const std::byte> input, output_chunk_list& output) -> int;
		static auto window_bits(compression_format format) -> int;

	protected:
		zlib_stream();
		~zlib_stream() = default;

	private:
		z_stream stream_{};
};

// A stream initialized by `init(stream)` and ended by `End`
template <auto End>
class zlib_stream_of : public zlib_stream {
	public:
		explicit zlib_stream_of(auto init);
		~zlib_stream_of() { End(&**this); }
};

// Streaming deflate over zlib. The output is a zlib, raw, or gzip stream according to the format.
class zlib_deflate {
	public:
		explicit zlib_deflate(compression_format format);

		auto write(std::span<const std::byte> input, output_chunk_list& output) -> void;
		auto flush(output_chunk_list& output) -> void;

	private:
		zlib_stream_of<deflateEnd> stream_;
};

// Streaming inflate over zlib, strict about the container which the format selects.
class zlib_inflate {
	public:
		explicit zlib_inflate(compression_format format);

		auto write(std::span<const std::byte> input, output_chunk_list& output) -> std::size_t;
		auto flush() const -> void;

	private:
		[[noreturn]] auto throw_state() const -> void;

		zlib_stream_of<inflateEnd> stream_;
		int state_ = Z_OK;
};

// `Compressor` handle, backend of `CompressionStream`
export class compressor_handle {
	public:
		explicit compressor_handle(compression_format format) : stream_{format} {}

		auto write(js::local_span<const char> input) -> codec_result;
		auto flush() -> output_chunk_list::range_type;
		static auto class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<compressor_handle>>;

	private:
		zlib_deflate stream_;
};

// `Decompressor` handle, backend of `DecompressionStream`
export class decompressor_handle {
	public:
		explicit decompressor_handle(compression_format format) : stream_{format} {}

		auto write(js::local_span<const char> input) -> codec_result;
		[[nodiscard]] auto flush() const -> output_chunk_list::range_type;
		static auto class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<decompressor_handle>>;

	private:
		zlib_inflate stream_;
};

} // namespace backend_napi_v8

namespace js {
using backend_napi_v8::compression_format;
using backend_napi_v8::output_chunk;

template <>
struct visit<void, std::unique_ptr<output_chunk::node_type>> : visit<void, array_buffer> {
		template <class Accept>
		auto operator()(std::unique_ptr<output_chunk::node_type>&& subject, const Accept& accept) const -> accept_target_t<Accept> {
			const visit<void, array_buffer>& visitor = *this;
			return visitor(output_chunk::release(std::move(subject)), accept);
		}
};

template <>
struct enum_values<compression_format> {
		constexpr static auto values = std::array{
			std::pair{u"deflate", compression_format::deflate},
			std::pair{u"deflate-raw", compression_format::deflate_raw},
			std::pair{u"gzip", compression_format::gzip},
		};
};

} // namespace js
