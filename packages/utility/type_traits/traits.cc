module;
#include <type_traits>
#include <utility>
export module ivm.utility:type_traits.traits;

namespace util {

// Copy reference category of `From` to `To`
template <class From, class To>
struct apply_ref_impl : std::type_identity<To> {};

template <class From, class To>
struct apply_ref_impl<From&, To> : std::type_identity<To&> {};

template <class From, class To>
struct apply_ref_impl<From&&, To> : std::type_identity<To&&> {};

export template <class From, class To>
	requires(!std::is_reference_v<To>)
struct apply_ref : apply_ref_impl<From, To> {};

export template <class From, class To>
using apply_ref_t = apply_ref<From, To>::type;

// Copy cv-qualifiers of `From` to `To`
template <class From, class To>
struct apply_cv_noref_impl : std::type_identity<To> {};

template <class From, class To>
struct apply_cv_noref_impl<const From, To> : std::type_identity<const To> {};

template <class From, class To>
struct apply_cv_noref_impl<volatile From, To> : std::type_identity<volatile To> {};

template <class From, class To>
struct apply_cv_noref_impl<const volatile From, To> : std::type_identity<const volatile To> {};

template <class From, class To>
	requires(!std::is_const_v<To> && !std::is_volatile_v<To>)
struct apply_cv_noref : apply_cv_noref_impl<From, To> {};

template <class From, class To>
using apply_cv_noref_t = apply_cv_noref<From, To>::type;

template <class From, class To>
struct apply_cv_impl : apply_cv_noref<From, To> {};

template <class From, class To>
struct apply_cv_impl<From, To&> : std::type_identity<apply_cv_noref_t<From, To>&> {};

template <class From, class To>
struct apply_cv_impl<From, To&&> : std::type_identity<apply_cv_noref_t<From, To>&&> {};

template <class From, class To>
struct apply_cv : apply_cv_noref<std::remove_reference_t<From>, To> {};

export template <class From, class To>
using apply_cv_t = apply_cv<From, To>::type;

// Copy cv-qualifiers and reference category of `From` to `To`
export template <class From, class To>
	requires std::is_same_v<std::remove_cvref_t<To>, To>
struct apply_cv_ref : apply_ref<From, apply_cv_t<From, To>> {};

export template <class From, class To>
using apply_cv_ref_t = apply_cv_ref<From, To>::type;

// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0870r4.html
export template <class From, class To>
constexpr inline bool is_convertible_without_narrowing_v = false;

template <class To, class From>
concept construct_without_narrowing = requires(From&& from) {
	// NOLINTNEXTLINE(modernize-avoid-c-arrays)
	{ std::type_identity_t<To[]>{std::forward<From>(from)} };
};

template <class From, class To>
	requires std::is_convertible_v<From, To>
constexpr inline bool is_convertible_without_narrowing_v<From, To> =
	construct_without_narrowing<To, From>;

export template <class From, class To>
concept convertible_without_narrowing = is_convertible_without_narrowing_v<From, To>;

} // namespace util
