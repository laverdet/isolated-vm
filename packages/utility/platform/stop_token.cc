export module util:platform.stop_token;
import :functional.elide;
import :platform.inplace_stop_token;
import :utility;
import std;

namespace util {

// Requests stop on the bound `inplace_stop_source`
class composite_stop_callback {
	public:
		explicit composite_stop_callback(util::inplace_stop_source& source) : source_{source} {}
		auto operator()() const noexcept -> void { source_.get().request_stop(); }

	private:
		std::reference_wrapper<util::inplace_stop_source> source_;
};

// `inplace_stop_source` which is stopped when any of the given tokens is stopped
export template <class... Tokens>
class composite_stop_source : public util::inplace_stop_source {
	public:
		template <class... Args>
		explicit composite_stop_source(Args&&... tokens) :
				callbacks_{util::elide{
					util::constructor<stop_callback<Tokens, composite_stop_callback>>,
					std::forward<Args>(tokens),
					composite_stop_callback{*this},
				}...} {}

	private:
		std::tuple<stop_callback<Tokens, composite_stop_callback>...> callbacks_;
};

export template <class... Tokens>
composite_stop_source(Tokens&&... tokens) -> composite_stop_source<std::remove_cvref_t<Tokens>...>;

} // namespace util
