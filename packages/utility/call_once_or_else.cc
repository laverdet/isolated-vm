module;
#include <concepts>
#include <stdexcept>
#include <tuple>
#include <utility>
export module ivm.utility:call_once_or_else;
import :utility;

// Returns an invocable which invokes a given destructor if it is destructed without being invoked.
// It also will throw if you invoke it twice.
export template <class Fn, class OrElse, std::move_constructible... Args>
	requires std::invocable<OrElse, Args...>
struct call_once_or_else {
	public:
		call_once_or_else(Fn fn, OrElse or_else, Args&&... args) :
				fn{std::move(fn)},
				or_else{std::move(or_else)},
				args{std::forward<Args>(args)...} {
		}
		call_once_or_else(const call_once_or_else&) = delete;
		call_once_or_else(call_once_or_else&& that) noexcept :
				fn{std::move(that.fn)},
				or_else{std::move(that.or_else)},
				args{std::move(that.args)},
				did_run{std::exchange(that.did_run, true)} {
		}
		~call_once_or_else() {
			if (!did_run) {
				std::apply(or_else, std::move(args));
			}
		}

		// Lambdas with movable captures are not move-assignable for some reason, so it's senseless to
		// define these.
		auto operator=(const call_once_or_else&) = delete;
		auto operator=(call_once_or_else&& that) = delete;

		auto operator()(auto&&... rest) -> void
			requires std::invocable<Fn, Args..., decltype(rest)...> {
			if (did_run) {
				throw std::logic_error("`call_once_or_else` invoked twice");
			}
			did_run = true;
			std::apply(
				take(std::move(fn)),
				std::tuple_cat(
					std::move(args),
					std::make_tuple(std::forward<decltype(rest)>(rest)...)
				)
			);
		}

		auto operator()(auto&&... rest) const -> void
			requires std::invocable<Fn, Args..., decltype(rest)...> {
			static_assert(false, "Cannot invoke a const `call_once_or_else`");
		}

	private:
		[[no_unique_address]] Fn fn;
		[[no_unique_address]] OrElse or_else;
		[[no_unique_address]] std::tuple<Args...> args;
		bool did_run = false;
};
