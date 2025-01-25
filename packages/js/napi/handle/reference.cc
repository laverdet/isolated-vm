export module napi_js.reference;
import napi_js.environment;
import napi_js.value;
import napi_js.value.internal;

namespace js::napi {

// A not-thread safe persistent value reference
export template <class Tag>
class reference : private reference_handle {
	public:
		reference(const environment& env, value<Tag> value) :
				reference_handle{env, value} {}

		[[nodiscard]] auto get(const environment& env) const -> value<Tag> {
			return value<Tag>::from(reference_handle::get(env));
		}
};

} // namespace js::napi
