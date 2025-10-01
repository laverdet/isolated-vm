module;
#include <type_traits>
#include <utility>
export module v8_js:external;
import :handle;
import isolated_js;
import ivm.utility;
import v8;

namespace js::iv8 {

export class external
		: public v8::Local<v8::External>,
			public materializable<external> {
	public:
		explicit external(v8::Local<v8::External> handle) :
				Local{handle} {}

		[[nodiscard]] auto materialize(std::type_identity<void*> /*tag*/) const -> void*;
};

export template <class Type>
class external_reference
		: util::non_moveable,
			public util::pointer_facade {
	public:
		explicit external_reference(auto&&... args) :
				value{std::forward<decltype(args)>(args)...} {}
		auto operator->() -> auto* { return &value; }

	private:
		Type value;
};

// ---

auto external::materialize(std::type_identity<void*> /*tag*/) const -> void* {
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

		auto operator()(external_tag tag, const auto& visit, auto&& value) const -> iv8::external_reference<Type>* {
			return std::addressof(accept_type::operator()(tag, visit, std::forward<decltype(value)>(value)));
		}

		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> iv8::external_reference<Type>* {
			return nullptr;
		}
};

} // namespace js
