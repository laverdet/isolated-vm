module;
#include <csetjmp>
export module v8_js:recovery;
import std;
import util;

namespace js::iv8 {

// Thrown from a `recovery_frame` after v8 reports a fatal error
export class fatal_error : public std::exception {
	public:
		fatal_error(std::string location, std::string message) :
				location_{std::move(location)},
				message_{std::move(message)} {}

		[[nodiscard]] auto location() const noexcept -> const std::string& { return location_; }
		[[nodiscard]] auto message() const noexcept -> const std::string& { return message_; }
		[[nodiscard]] auto what() const noexcept -> const char* override { return message_.c_str(); }

	private:
		std::string location_;
		std::string message_;
};

// v8 fatal handler
export class recovery_frame : util::non_moveable {
	public:
		static auto fatal_callback(const char* location, const char* message) -> void;

		// Called right before entering v8, performs `setjmp`, which is the target of the fatal handler
		static auto invoke(std::invocable auto operation) -> decltype(auto);
		// Called right after v8 invokes embedder code, catches `js::fatal_error`
		static auto stop(std::invocable auto operation) -> decltype(auto);

	private:
		recovery_frame() = default;

		[[noreturn]] static auto throw_jump() -> void;
		static auto throw_set(fatal_error error) -> void;
		[[noreturn]] auto throw_rethrow() -> void;

		inline static thread_local recovery_frame* current_ = nullptr;
		std::jmp_buf buffer_{};
		std::optional<fatal_error> error_;
		util::scope_stack<recovery_frame*> scope_{current_, this};
};

// ---

auto recovery_frame::invoke(std::invocable auto operation) -> decltype(auto) {
	auto frame = recovery_frame{};
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay, modernize-avoid-setjmp-longjmp)
	if (setjmp(frame.buffer_) == 0) {
		return operation();
	} else {
		frame.throw_rethrow();
	}
}

auto recovery_frame::stop(std::invocable auto operation) -> decltype(auto) {
	try {
		return operation();
	} catch (const fatal_error& caught) {
		throw_set(caught);
	}
	throw_jump();
}

} // namespace js::iv8
