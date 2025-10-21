module;
#include <type_traits>
export module ivm.utility:type_traits.transform;

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
struct apply_cv : apply_cv_impl<std::remove_reference_t<From>, To> {};

export template <class From, class To>
using apply_cv_t = apply_cv<From, To>::type;

// Copy cv-qualifiers and reference category of `From` to `To`
export template <class From, class To>
struct apply_cv_ref : apply_ref<From, apply_cv_t<From, To>> {};

export template <class From, class To>
using apply_cv_ref_t = apply_cv_ref<From, To>::type;

} // namespace util
