export module auto_js:intrinsics.external;
import util;

namespace js {

// Holder for a runtime type-tagged external pointer.
export template <class Type>
class tagged_external : public util::pointer_facade {
	public:
		explicit tagged_external(Type& value) : value_{&value} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		operator Type&() const { return *value_; }

		auto operator*() const -> Type& { return *value_; }

	private:
		Type* value_;
};

} // namespace js
