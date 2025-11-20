module;
#include <stop_token>
#include <utility>
export module util:platform.stop_token;
import :functional;
import :utility;

namespace util {

// Combine multiple `std::stop_token`s into one `std::stop_token` that is stopped when any of the
// source tokens is stopped.
export template <size_t Size>
class composite_stop_token : private std::stop_source, public std::stop_token {
	private:
		static auto make_callback(std::stop_source stop_source) {
			// I wonder what's better: copying the stop state into each callback, or using function
			// pointer..
			return [ stop_source = std::move(stop_source) ]() -> void { stop_source.request_stop(); };
		}

		using callback_type = decltype(make_callback(std::declval<std::stop_source>()));
		using stop_callback_type = std::stop_callback<callback_type>;
		using callback_pack_type = std::array<stop_callback_type, Size>;

	public:
		using std::stop_token::stop_possible;
		using std::stop_token::stop_requested;

		// Check stoppability of the tokens
		template <std::convertible_to<std::stop_source> Source, std::convertible_to<std::stop_token>... Tokens>
			requires(Size == sizeof...(Tokens))
		explicit composite_stop_token(Source&& source, Tokens&&... tokens) :
				composite_stop_token{
					std::forward<Source>(source),
					(... + (tokens.stop_possible() ? 1 : 0)),
					std::forward<Tokens>(tokens)...
				} {}

		// Makes a fresh `std::stop_source` internally
		template <std::convertible_to<std::stop_token>... Tokens>
		explicit composite_stop_token(Tokens&&... tokens) :
				composite_stop_token{std::stop_source{}, std::forward<decltype(tokens)>(tokens)...} {}

	private:
		// Some finesse is required because `std::stop_callback` is not move constructible
		template <class Source, class... Tokens>
		explicit composite_stop_token(Source&& source, int stoppable, Tokens&&... tokens) :
				std::stop_source{std::forward<Source>(source)},
				std::stop_token{util::elide{[ & ]() -> std::stop_token {
					switch (stoppable) {
						// No `stop_token`
						case 0: return std::stop_token{};
						// Delegated `stop_token`
						case 1:
							{
								constexpr auto fold = util::overloaded{
									[]() -> std::stop_token { std::unreachable(); },
									[](auto&& token) -> std::stop_token { return std::forward<decltype(token)>(token); },
									[](this auto& fold, auto&& token, auto&&... tokens) -> std::stop_token {
										if (token.stop_possible()) {
											return std::forward<decltype(token)>(token);
										} else {
											return fold(std::forward<decltype(tokens)>(tokens)...);
										}
									},
								};
								return fold(std::forward<decltype(tokens)>(tokens)...);
							}
						// Callback `stop_token`
						default: return std::stop_source::get_token();
					}
				}}},
				callbacks_{[ & ]() -> callback_pack_type {
					switch (stoppable) {
						// No-op callback pack
						case 0:
						case 1:
							{
								auto callback = make_callback(std::stop_source{});
								return {stop_callback_type{(util::unused(tokens), std::stop_token{}), callback}...};
							}
						// Make callback pack
						default:
							{
								auto callback = make_callback(std::move(static_cast<std::stop_source&>(*this)));
								return {stop_callback_type{std::forward<decltype(tokens)>(tokens), callback}...};
							}
					}
				}()} {}

		// Default init callbacks with no tokens
		callback_pack_type callbacks_;
};

template <std::convertible_to<std::stop_token>... Tokens>
composite_stop_token(Tokens&&... tokens) -> composite_stop_token<sizeof...(tokens)>;

template <std::convertible_to<std::stop_source> Source, std::convertible_to<std::stop_token>... Tokens>
composite_stop_token(Source&& source, Tokens&&... tokens) -> composite_stop_token<sizeof...(tokens)>;

// `std::stop_source` that stops itself on destruction.
export class scope_stop_source : util::non_copyable, public std::stop_source {
	public:
		~scope_stop_source() { request_stop(); }
};

} // namespace util
