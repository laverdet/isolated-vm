module;
#include <utility>
export module v8_js:external;
import :handle;
import isolated_js;
import ivm.utility;
import v8;

namespace js::iv8 {

export class external : public v8::Local<v8::External> {
	public:
		explicit external(v8::Local<v8::External> handle) :
				v8::Local<v8::External>{handle} {}

		[[nodiscard]] explicit operator void*() const;
};

export template <class Type>
class external_reference
		: util::non_moveable,
			public util::pointer_facade {
	public:
		explicit external_reference(auto&&... args) :
				value{std::forward<decltype(args)>(args)...} {}
		auto operator*() -> Type& { return value; }

	private:
		Type value;
};

// ---

external::operator void*() const {
	return (*this)->Value();
}

} // namespace js::iv8

namespace js {

template <class Type>
struct accept<void, iv8::external_reference<Type>&> {
		auto operator()(external_tag /*tag*/, visit_holder /*visit*/, auto value) const -> iv8::external_reference<Type>& {
			auto* void_ptr = static_cast<void*>(value);
			return *static_cast<iv8::external_reference<Type>*>(void_ptr);
		}
};

// nb: Accepting a pointer allows `undefined` to pass
template <class Type>
struct accept<void, iv8::external_reference<Type>*> : accept<void, iv8::external_reference<Type>&> {
		using accept_type = accept<void, iv8::external_reference<Type>&>;
		using accept_type::accept_type;

		auto operator()(external_tag tag, auto& visit, auto&& value) const -> iv8::external_reference<Type>* {
			return std::addressof(util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(value)>(value)));
		}

		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> iv8::external_reference<Type>* {
			return nullptr;
		}
};

} // namespace js
