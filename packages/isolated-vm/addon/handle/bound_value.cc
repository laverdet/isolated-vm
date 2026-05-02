export module isolated_vm:handle.bound_value;
import :handle.types;
import :support.lock_fwd;

namespace isolated_vm {

// Details applied to each level of the `bound_value<T>` hierarchy.
template <class Tag>
class bound_value_next : public bound_value<typename Tag::tag_type> {
	public:
		using tag_type = Tag;
		using bound_value<typename Tag::tag_type>::bound_value;
		bound_value_next() = default;
		bound_value_next(const basic_lock& lock, value_of<Tag> value) :
				bound_value<typename Tag::tag_type>{lock, value} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator value_of<Tag>() const { return value_of<Tag>::from(value_handle{*this}); }
};

// Member & method implementation for stateful objects. Used internally in visitors.
template <class Tag>
class bound_value : public value_specialization<Tag>::bound_type {
	public:
		using value_specialization<Tag>::bound_type::bound_type;
};

template <class Tag>
bound_value(auto, value_of<Tag>) -> bound_value<Tag>;

template <>
class bound_value<void> : public value_handle {
	protected:
		bound_value() = default;
		bound_value(const runtime_lock& lock, value_handle value) :
				value_handle{value},
				lock_{&lock} {}

		[[nodiscard]] auto lock() const -> const runtime_lock& { return *lock_; }

	private:
		const runtime_lock* lock_{};
};

} // namespace isolated_vm
