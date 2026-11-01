export module util:platform.inplace_stop_token;
import :functional.function_ref;
import :utility;
import std;

namespace util {

export template <class Token>
concept stoppable_token =
	std::copyable<std::remove_cvref_t<Token>> &&
	std::equality_comparable<Token> &&
	requires(const Token token) {
		{ token.stop_requested() } noexcept -> std::same_as<bool>;
		{ token.stop_possible() } noexcept -> std::same_as<bool>;
	};

class inplace_stop_callback_base;
export class inplace_stop_source;
export class inplace_stop_token;
export template <class Callback>
class inplace_stop_callback;

// Sentinel head shared by `inplace_stop_source` (head) and `inplace_stop_callback` (node)
class inplace_stop_callback_head : non_moveable {
	private:
		friend inplace_stop_callback_base;
		friend inplace_stop_source;
		class inplace_stop_callback_base* next_{};
};

// Type-erased node used by `inplace_stop_callback`
class inplace_stop_callback_base : inplace_stop_callback_head {
	protected:
		using invoke_type = function_ref<auto() noexcept -> void>;

		explicit inplace_stop_callback_base(inplace_stop_token token, invoke_type invoke);

		// Links the callback, or invokes it immediately if stop was already requested
		auto attach() noexcept -> void;
		// Links the callback without invoking it; returns `false` if stop was already requested
		auto try_attach() noexcept -> bool;
		auto detach() noexcept -> void;

	private:
		friend inplace_stop_source;
		auto unlink() noexcept -> void;

		const inplace_stop_source* source_;
		invoke_type invoke_;
		inplace_stop_callback_head* prev_{};
};

class inplace_stop_source : non_moveable {
	public:
		[[nodiscard]] auto get_token() const noexcept -> inplace_stop_token;
		auto request_stop() noexcept -> bool;
		[[nodiscard]] auto stop_requested() const noexcept -> bool { return stop_requested_.load(std::memory_order_acquire); }
		[[nodiscard]] constexpr static auto stop_possible() noexcept -> bool { return true; }

	private:
		friend inplace_stop_callback_base;

		std::atomic<bool> stop_requested_;
		mutable std::mutex mutex_;
		mutable inplace_stop_callback_head callbacks_;
		std::atomic<const inplace_stop_callback_base*> executing_;
		std::thread::id requesting_thread_;
};

class inplace_stop_token {
	public:
		template <class Callback>
		using callback_type = inplace_stop_callback<Callback>;

		inplace_stop_token() = default;

		[[nodiscard]] auto stop_requested() const noexcept -> bool { return stop_possible() && source_->stop_requested(); }
		[[nodiscard]] auto stop_possible() const noexcept -> bool { return source_ != nullptr; }
		auto swap(inplace_stop_token& that) noexcept -> void { std::swap(source_, that.source_); }
		auto operator==(const inplace_stop_token&) const -> bool = default;

	private:
		friend inplace_stop_source;
		friend inplace_stop_callback_base;
		explicit inplace_stop_token(const inplace_stop_source* source) : source_{source} {}

		const inplace_stop_source* source_{};
};

template <class Callback>
class inplace_stop_callback : inplace_stop_callback_base {
	public:
		using callback_type = Callback;

		explicit inplace_stop_callback(inplace_stop_token token, Callback callback) :
				inplace_stop_callback{std::defer_lock, token, std::move(callback)} {
			attach();
		}
		// Deferred; attach with `try_attach`
		explicit inplace_stop_callback(std::defer_lock_t /*defer*/, inplace_stop_token token, Callback callback) :
				inplace_stop_callback_base{token, callback_},
				callback_{std::move(callback)} {}
		inplace_stop_callback(const inplace_stop_callback&) = delete;
		~inplace_stop_callback() { detach(); }
		auto operator=(const inplace_stop_callback&) -> inplace_stop_callback& = delete;

		using inplace_stop_callback_base::try_attach;

	private:
		Callback callback_;
};

// Selects the `stop_callback` implementation for a token type
template <class Token, class Callback>
struct stop_callback_for;

template <class Callback>
struct stop_callback_for<std::stop_token, Callback> : std::type_identity<std::stop_callback<Callback>> {};

template <class Callback>
struct stop_callback_for<inplace_stop_token, Callback> : std::type_identity<inplace_stop_callback<Callback>> {};

export template <stoppable_token Token, class Callback>
class stop_callback : non_moveable {
	public:
		explicit stop_callback(Token token, Callback callback) :
				callback_{std::move(token), std::move(callback)} {}

	private:
		stop_callback_for<Token, Callback>::type callback_;
};

export template <class Token, class Callback>
stop_callback(Token, Callback) -> stop_callback<Token, Callback>;

} // namespace util
