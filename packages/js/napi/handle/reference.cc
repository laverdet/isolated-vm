module;
#include <utility>
export module napi_js.reference;
import napi_js.environment;
import napi_js.utility;
import napi_js.value;
import nodejs;
import ivm.utility;

namespace js::napi {
namespace detail {

// Base class of reference & remote values
export class reference_handle : util::non_copyable {
	protected:
		reference_handle(napi_env env, napi_value value) :
				env_{env},
				value_{js::napi::invoke(napi_create_reference, env, value, 1)} {}

		[[nodiscard]] auto env() const -> napi_env {
			return env_;
		}

		[[nodiscard]] auto get_value(napi_env env) const -> napi_value {
			return js::napi::invoke(napi_get_reference_value, env, value_);
		}

	public:
		reference_handle(const reference_handle&) = delete;
		reference_handle(reference_handle&& right) noexcept :
				env_{right.env_},
				value_{std::exchange(right.value_, nullptr)} {}

		~reference_handle() {
			if (value_ != nullptr) {
				js::napi::invoke_dtor(napi_delete_reference, env_, value_);
			}
		}

		auto operator=(const reference_handle&) = delete;
		auto operator=(reference_handle&&) = delete;

	private:
		napi_env env_;
		napi_ref value_;
};

} // namespace detail

// A not-thread safe persistent value reference.
export template <class Tag>
class reference : public reference<typename Tag::tag_type> {
	public:
		using reference<typename Tag::tag_type>::reference;

		reference(const environment& env, value<Tag> value) :
				reference<typename Tag::tag_type>{napi_env{env}, napi_value{value}} {}

		[[nodiscard]] auto get(const environment& env) const -> value<Tag> {
			return value<Tag>::from(this->get_value(napi_env{env}));
		}
};

template <class Tag>
reference(auto, value<Tag>) -> reference<Tag>;

// nb: Protected inheritance so that `remote<T> is not comparable to `reference<T>`.
template <>
class reference<void> : protected detail::reference_handle {
		using reference_handle::reference_handle;
};

} // namespace js::napi
