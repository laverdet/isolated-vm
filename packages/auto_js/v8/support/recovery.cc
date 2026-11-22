module;
#include <csetjmp>
#include <cstdio>
module v8_js;
import :recovery;
import std;

namespace js::iv8 {

[[noreturn]] auto abort_with_fatal_error(const char* location, const char* message) -> void {
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
	std::print(stderr, "\n#\n# Fatal error in {}\n# {}\n#\n\n", location, message);
	std::fflush(stderr);
	std::abort();
}

auto recovery_frame::fatal_callback(const char* location, const char* message) -> void {
	if (current_ == nullptr || std::uncaught_exceptions() > 0) {
		abort_with_fatal_error(location, message);
	}
	throw_set(fatal_error{location, message});
	throw_jump();
}

auto recovery_frame::throw_set(fatal_error error) -> void {
	auto* frame = current_;
	if (frame == nullptr) {
		abort_with_fatal_error(error.location().c_str(), error.message().c_str());
	}
	frame->error_.emplace(std::move(error));
}

auto recovery_frame::throw_jump() -> void {
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay, modernize-avoid-setjmp-longjmp)
	std::longjmp(current_->buffer_, 1);
}

auto recovery_frame::throw_rethrow() -> void {
	throw std::move(error_).value();
}

} // namespace js::iv8
