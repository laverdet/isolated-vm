module;
#include <cstring>
#include <memory>
#include <mutex>
#include <random>
#include <ranges>
module ivm.isolated_v8;
import :platform;
import :platform.job_handle;
import ivm.utility;
import v8;

namespace ivm {

platform::platform() {
	v8::V8::InitializeICU();
	v8::V8::InitializePlatform(this);
	v8::V8::SetEntropySource(fill_random_bytes);
	v8::V8::Initialize();
}

auto platform::fill_random_bytes(unsigned char* buffer, size_t length) -> bool {
	// Copies the bit data from an infinite view of numerics into `buffer` up to `length`.
	auto fill = [ & ](auto&& numeric_stream) {
		auto bytes_views =
			numeric_stream |
			std::views::transform([](auto value) {
				return std::bit_cast<std::array<unsigned char, sizeof(value)>>(value);
			});
		auto byte_view = std::views::join(bytes_views);
		std::ranges::copy_n(byte_view.begin(), length, buffer);
		return true;
	};
	auto* host = agent::host::get_current();
	if (host != nullptr) {
		auto seed = host->take_random_seed();
		if (seed) {
			// Repeats the user-supplied double seed
			return fill(std::ranges::repeat_view(*seed));
		}
	}
	// Stream pulls directly from `random_device`
	std::random_device device;
	auto integer_stream =
		std::views::repeat(std::nullopt) |
		std::views::transform([ & ](auto /*nullopt*/) { return device(); });
	return fill(integer_stream);
}

} // namespace ivm
