module;
#include <zlib.h>
#include <cassert>
module backend_napi_v8;
import :compression;
import :lock;
import auto_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {
namespace {

// Every `avail_in` / `avail_out` pass is bounded by `unsigned`
constexpr auto max_avail = std::size_t{std::numeric_limits<unsigned>::max()};

// Size of the first output chunk. Each one after it doubles, up to the maximum.
constexpr auto minimum_output = std::size_t{16} << 10;
constexpr auto maximum_output = std::size_t{1} << 20;
static_assert(maximum_output <= max_avail);

// Size to use to probe for available zlib stream data
constexpr auto probe_size = std::size_t{64};
static_assert(probe_size <= minimum_output);

// zlib's default `memLevel`, which `zlib.h` does not name
constexpr auto default_memory_level = 8;

} // namespace

// output_chunk
auto output_chunk::release(std::unique_ptr<node_type> chunk) -> js::array_buffer {
	// TODO: Pointer to pointer. `js::array_buffer` probably needs classic C-style callback w/ data
	auto deleter = [](node_type* chunk, std::byte* /*data*/) -> void {
		delete chunk;
	};
	const auto written = chunk->header().written;
	if (written != chunk->size()) {
		assert(!chunk->header().next);
		node_type::resize(chunk, written);
	}
	auto* raw = chunk.release();
	return js::array_buffer{raw->elements(), js::array_buffer::deleter{deleter, raw}};
}

// output_chunk_list
auto output_chunk_list::push_back(std::unique_ptr<output_chunk::node_type> chunk) -> void {
	auto& slot = tail_ == nullptr ? head_ : tail_->header().next;
	slot = std::move(chunk);
	tail_ = slot.get();
	++size_;
}

auto output_chunk_list::space() -> std::span<std::byte> {
	if (empty()) {
		return {};
	}
	auto& tail = back();
	return tail.elements().subspan(tail.header().written);
}

auto output_chunk_list::commit(std::size_t size) -> void {
	auto& tail = back();
	tail.header().written += size;
	assert(tail.header().written <= tail.size());
}

auto output_chunk_list::append(std::span<const std::byte> bytes) -> void {
	if (bytes.empty()) {
		return;
	}
	auto chunk = output_chunk::node_type::make(empty() ? minimum_output : std::min(back().size() * 2, maximum_output));
	assert(bytes.size() <= chunk->size());
	std::ranges::copy(bytes, chunk->elements().begin());
	chunk->header().written = bytes.size();
	push_back(std::move(chunk));
}

// zlib_stream_of
template class zlib_stream_of<deflateEnd>;
template class zlib_stream_of<inflateEnd>;

template <auto End>
zlib_stream_of<End>::zlib_stream_of(auto init) {
	const auto result = init(&**this);
	if (result != Z_OK) {
		throw_zlib_error(result);
	}
}

// zlib_stream
zlib_stream::zlib_stream() {
	if (&deflateInit2_ == nullptr || &inflateInit2_ == nullptr) {
		throw js::runtime_error{u"zlib is not available in this process"};
	}
}

auto zlib_stream::window_bits(compression_format format) -> int {
	switch (format) {
		case compression_format::deflate: return MAX_WBITS;
		case compression_format::deflate_raw: return -MAX_WBITS;
		case compression_format::gzip: return MAX_WBITS + 16;
	}
	std::unreachable();
}

auto zlib_stream::throw_zlib_error(int result) const -> void {
	if (result == Z_MEM_ERROR) {
		throw std::bad_alloc{};
	} else if (stream_.msg == nullptr) {
		throw js::runtime_error{"zlib error {}", result};
	} else {
		throw js::runtime_error{"zlib error {}: {}", result, stream_.msg};
	}
}

auto zlib_stream::pump(auto step, int flush, std::span<const std::byte> input, output_chunk_list& output) -> int {
	if (input.size() > max_avail) {
		throw js::range_error{u"Input is too large"};
	}
	stream_.next_in = reinterpret_cast<const Bytef*>(input.data());
	stream_.avail_in = static_cast<unsigned>(input.size());
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	std::array<std::byte, probe_size> probe;
	while (true) {
		// Small stream read to see if there's more data available
		auto space = output.space();
		const auto probing = space.empty();
		if (probing) {
			space = probe;
		}
		stream_.next_out = reinterpret_cast<Bytef*>(space.data());
		stream_.avail_out = static_cast<unsigned>(space.size());
		const auto result = step(&stream_, flush);
		const auto written = space.size() - stream_.avail_out;
		if (probing) {
			output.append(space.first(written));
		} else {
			output.commit(written);
		}
		if (result == Z_BUF_ERROR) {
			// Input ran out just as the last chunk filled. It is not an error, and the pass is done.
			return Z_OK;
		}
		if (result != Z_OK || stream_.avail_out != 0) {
			return result;
		}
	}
}

// zlib_deflate
zlib_deflate::zlib_deflate(compression_format format) :
		stream_{[ & ](z_streamp stream) -> int {
			return deflateInit2(stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, zlib_stream::window_bits(format), default_memory_level, Z_DEFAULT_STRATEGY);
		}} {}

auto zlib_deflate::write(std::span<const std::byte> input, output_chunk_list& output) -> void {
	const auto result = stream_.pump(util::fn<deflate>, Z_NO_FLUSH, input, output);
	if (result != Z_OK) {
		stream_.throw_zlib_error(result);
	}
}

auto zlib_deflate::flush(output_chunk_list& output) -> void {
	const auto result = stream_.pump(util::fn<deflate>, Z_FINISH, {}, output);
	if (result != Z_STREAM_END) {
		stream_.throw_zlib_error(result);
	}
}

// zlib_inflate
zlib_inflate::zlib_inflate(compression_format format) :
		stream_{[ & ](z_streamp stream) -> int {
			return inflateInit2(stream, zlib_stream::window_bits(format));
		}} {}

auto zlib_inflate::write(std::span<const std::byte> input, output_chunk_list& output) -> std::size_t {
	// https://compression.spec.whatwg.org/#decompress-and-enqueue-a-chunk
	if (input.empty()) {
		return 0;
	}
	if (state_ != Z_OK) {
		// Pending error from a previous write
		throw_state();
	}
	state_ = stream_.pump(util::fn<inflate>, Z_NO_FLUSH, input, output);
	switch (state_) {
		case Z_OK:
		case Z_STREAM_END:
		case Z_NEED_DICT:
		case Z_DATA_ERROR:
			break;
		default:
			stream_.throw_zlib_error(state_);
	}
	// Stop short on end of the stream or invalid data. The rest is left for the next write.
	const auto consumed = input.size() - stream_->avail_in;
	if (consumed == 0) {
		throw_state();
	}
	return consumed;
}

auto zlib_inflate::flush() const -> void {
	// https://compression.spec.whatwg.org/#decompress-flush-and-enqueue
	switch (state_) {
		case Z_STREAM_END: break;
		case Z_OK: throw js::type_error{u"invalid compressed payload"};
		default: throw_state();
	}
}

auto zlib_inflate::throw_state() const -> void {
	switch (state_) {
		case Z_STREAM_END:
		case Z_NEED_DICT:
		case Z_DATA_ERROR:
			throw js::type_error{u"invalid compressed payload"};
		default:
			stream_.throw_zlib_error(state_);
	}
}

// compressor_handle
auto compressor_handle::write(js::local_span<const char> input) -> codec_result {
	auto output = output_chunk_list{};
	stream_.write(std::as_bytes(input), output);
	return codec_result{
		.output = output_chunk_list::range_type{js::vector_tag{}, std::move(output)},
		.bytes_read = static_cast<std::uint32_t>(input.size()),
	};
}

auto compressor_handle::flush() -> output_chunk_list::range_type {
	auto output = output_chunk_list{};
	stream_.flush(output);
	return output_chunk_list::range_type{js::vector_tag{}, std::move(output)};
}

auto compressor_handle::class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<compressor_handle>> {
	auto constructor = [](compression_format format) -> auto {
		// nb: `elide` stores a reference to an lvalue argument
		return util::elide{util::constructor<compressor_handle>, compression_format{format}};
	};
	return lock->environment().class_template(
		std::type_identity<compressor_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Compressor">, util::fn<+constructor>},
			js::class_method{util::cw<"flush">, util::fn<&compressor_handle::flush>},
			js::class_method{util::cw<"write">, util::fn<&compressor_handle::write>},
		}
	);
}

// decompressor_handle
auto decompressor_handle::write(js::local_span<const char> input) -> codec_result {
	auto output = output_chunk_list{};
	const auto consumed = stream_.write(std::as_bytes(input), output);
	return codec_result{
		.output = output_chunk_list::range_type{js::vector_tag{}, std::move(output)},
		.bytes_read = static_cast<std::uint32_t>(consumed),
	};
}

// Nothing is held back, so the output is always empty. The result matches `compressor_handle`.
auto decompressor_handle::flush() const -> output_chunk_list::range_type {
	stream_.flush();
	return output_chunk_list::range_type{js::vector_tag{}, output_chunk_list{}};
}

auto decompressor_handle::class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<decompressor_handle>> {
	auto constructor = [](compression_format format) -> auto {
		// nb: `elide` stores a reference to an lvalue argument
		return util::elide{util::constructor<decompressor_handle>, compression_format{format}};
	};
	return lock->environment().class_template(
		std::type_identity<decompressor_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Decompressor">, util::fn<+constructor>},
			js::class_method{util::cw<"flush">, util::fn<&decompressor_handle::flush>},
			js::class_method{util::cw<"write">, util::fn<&decompressor_handle::write>},
		}
	);
}

} // namespace backend_napi_v8
