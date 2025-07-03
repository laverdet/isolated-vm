export module napi_js:bound_value;
import :environment;
import :value;
import isolated_js;
import nodejs;

namespace js::napi {

// Forward declaration
export template <class Tag> class bound_value;

namespace detail {

// Details applied to each level of the `bound_value<T>` hierarchy.
export template <class Tag>
class bound_value_next : public bound_value<typename Tag::tag_type> {
	public:
		using bound_value<typename Tag::tag_type>::bound_value;
		bound_value_next() = default;
		bound_value_next(napi_env env, value<Tag> value) :
				bound_value<typename Tag::tag_type>{env, napi_value{value}} {}
		bound_value_next(const environment& env, value<Tag> value) :
				bound_value_next{napi_env{env}, napi_value{value}} {}

		explicit operator value<Tag>() const { return value<Tag>::from(napi_value{*this}); }
};

} // namespace detail

// Member & method implementation for stateful objects. Used internally in visitors. I think it
// might make sense to have the environment specified by a template parameter. Then you would use
// `bound_value<T>` or something instead of passing the environment to each `value<T>` method.
export template <class Tag>
class bound_value : public detail::bound_value_next<Tag> {
	public:
		using detail::bound_value_next<Tag>::bound_value_next;
};

template <class Tag>
bound_value(auto, value<Tag>) -> bound_value<Tag>;

template <>
class bound_value<void> : public detail::value_handle {
	protected:
		bound_value() = default;
		bound_value(napi_env env, napi_value value) :
				value_handle{value},
				env_{env} {}

		[[nodiscard]] auto env() const { return env_; }

	private:
		napi_env env_{};
};

} // namespace js::napi
