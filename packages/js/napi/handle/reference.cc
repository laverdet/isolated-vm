module;
#include <stdexcept>
#include <utility>
export module napi_js:reference;
import :api;
import :environment;
import :value;
import ivm.utility;

namespace js::napi {
namespace detail {

// Base class of reference & remote values
export class reference_handle : util::non_copyable {
	protected:
		reference_handle() = default;
		reference_handle(napi_env env, napi_value value) :
				env_{env},
				ref_{js::napi::invoke(napi_create_reference, env, value, 1)} {}

		[[nodiscard]] auto env() const -> napi_env {
			return env_;
		}

		[[nodiscard]] auto get_value(napi_env env) const -> napi_value {
			return js::napi::invoke(napi_get_reference_value, env, ref_);
		}

		auto reset(napi_env env, napi_value value) -> void {
			if (ref_ != nullptr) {
				if (env != env_) {
					throw std::logic_error{"Environment mismatch"};
				}
				js::napi::invoke0(napi_delete_reference, env, std::exchange(ref_, nullptr));
			}
			env_ = env;
			ref_ = js::napi::invoke(napi_create_reference, env, value, 1);
		}

	public:
		consteval reference_handle(const reference_handle& /*right*/) {
			static_assert(std::is_constant_evaluated());
		}

		constexpr reference_handle(reference_handle&& right) noexcept :
				env_{right.env_},
				ref_{std::exchange(right.ref_, nullptr)} {}

		constexpr ~reference_handle() {
			if (ref_ != nullptr) {
				js::napi::invoke0_noexcept(napi_delete_reference, env_, ref_);
			}
		}

		auto operator=(const reference_handle&) = delete;
		auto operator=(reference_handle&&) = delete;

		explicit operator bool() const { return ref_ != nullptr; }

	private:
		napi_env env_{};
		napi_ref ref_{};
};

} // namespace detail

// A not-thread safe persistent value reference.
export template <class Tag>
class reference : public reference<typename Tag::tag_type> {
	public:
		using value_type = value<Tag>;
		using reference<typename Tag::tag_type>::reference;

		reference(const environment& env, value_type value) :
				reference<typename Tag::tag_type>{napi_env{env}, napi_value{value}} {}

		explicit operator bool() const { return detail::reference_handle::operator bool(); }
		[[nodiscard]] auto get(const environment& env) const -> value_type { return value_type::from(this->get_value(napi_env{env})); }

		auto reset(const environment& env, value_type value) -> void { detail::reference_handle::reset(napi_env{env}, napi_value{value}); }
};

template <class Tag>
reference(auto, value<Tag>) -> reference<Tag>;

// nb: Protected inheritance so that `remote<T> is not comparable to `reference<T>`.
template <>
class reference<void> : protected detail::reference_handle {
		using reference_handle::reference_handle;
};

} // namespace js::napi
