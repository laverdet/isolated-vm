export module util:functional.move_only_function;
import :type_traits;
import std;

namespace util {

#if defined(__cpp_lib_move_only_function)

export template <class Signature>
using move_only_function = std::move_only_function<Signature>;

#else

// Virtual base class. There must be two, one for each const/non-const signature because virtual
// function cannot have explicit this or requirement clauses.
template <class CvRef, class Signature>
struct move_only_function_virtual;

template <class... Args, bool Nx, class Result>
struct move_only_function_virtual<int, auto(Args...) noexcept(Nx)->Result> {
		virtual constexpr ~move_only_function_virtual() = default;
		virtual constexpr auto operator()(Args... args) noexcept(Nx) -> Result = 0;
};

template <class... Args, bool Nx, class Result>
struct move_only_function_virtual<const int, auto(Args...) noexcept(Nx)->Result> {
		virtual constexpr ~move_only_function_virtual() = default;
		virtual constexpr auto operator()(Args... args) const noexcept(Nx) -> Result = 0;
};

// Instance per underlying function type
template <class CvRef, class Signature, class Function>
class move_only_function_instance;

template <class... Args, bool Nx, class Result, class Function>
class move_only_function_instance<int, auto(Args...) noexcept(Nx)->Result, Function>
		: public move_only_function_virtual<int, auto(Args...) noexcept(Nx)->Result> {
	public:
		explicit constexpr move_only_function_instance(Function function) : function_{std::move(function)} {}
		virtual constexpr auto operator()(Args... args) noexcept(Nx) -> Result { return function_(std::forward<Args>(args)...); }

	private:
		Function function_;
};

template <class... Args, bool Nx, class Result, class Function>
class move_only_function_instance<const int, auto(Args...) noexcept(Nx)->Result, Function>
		: public move_only_function_virtual<const int, auto(Args...) noexcept(Nx)->Result> {
	public:
		explicit constexpr move_only_function_instance(Function function) : function_{std::move(function)} {}
		virtual constexpr auto operator()(Args... args) const noexcept(Nx) -> Result { return function_(std::forward<Args>(args)...); }

	private:
		Function function_;
};

// std::move_only_function polyfill implementation (separated to split out cvref from signature)
template <class CvRef, class Signature>
class move_only_function_handle;

template <class Signature>
using move_only_function_handle_for =
	move_only_function_handle<util::extract_function_cvref_t<Signature>, remove_function_cvref_t<Signature>>;

template <class CvRef, class... Args, bool Nx, class Result>
class move_only_function_handle<CvRef, auto(Args...) noexcept(Nx)->Result> {
	public:
		move_only_function_handle() = default;

		template <class Function>
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr move_only_function_handle(Function function) :
				instance_{std::make_unique<move_only_function_instance<CvRef, auto(Args...) noexcept(Nx)->Result, Function>>(std::move(function))} {}
		constexpr auto operator()(this util::apply_cvref_t<CvRef, move_only_function_handle>& self, Args... args) noexcept(Nx) -> Result {
			return (*self.instance_)(std::forward<Args>(args)...);
		}
		constexpr explicit operator bool() const { return bool{instance_}; }

	private:
		std::unique_ptr<move_only_function_virtual<CvRef, auto(Args...) noexcept(Nx)->Result>> instance_;
};

// std::move_only_function polyfill
export template <class Signature>
struct move_only_function : move_only_function_handle_for<Signature> {
		move_only_function() = default;
		using move_only_function_handle_for<Signature>::move_only_function_handle_for;
};

template <class Function>
move_only_function(Function) -> move_only_function<util::function_signature_t<Function>>;

#endif

} // namespace util
