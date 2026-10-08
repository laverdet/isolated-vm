module;
#include <v8_js/version.h>
export module v8_js:wrappable;
import :lock;
import std;
import v8;

namespace js::iv8 {

#if V8_HAS_OBJECT_WRAPPABLE
// Host object which is managed by cppgc and wrapped by a JavaScript object. All instances share one
// `v8::CppHeapPointerTag`, so the runtime type is checked here instead.
class wrappable_holder : public v8::Object::Wrappable {
	public:
		explicit wrappable_holder(const std::type_info& type) : type_{type} {}
		[[nodiscard]] auto is(const std::type_info& type) const -> bool { return type_.get() == type; }

	private:
		std::reference_wrapper<const std::type_info> type_;
};

export template <class Type>
class wrappable_of final
		: public wrappable_holder,
			public Type {
	public:
		explicit wrappable_of(auto&&... args)
			requires std::constructible_from<Type, decltype(args)...> :
				wrappable_holder{typeid(Type)},
				Type{std::forward<decltype(args)>(args)...} {}

		static auto wrap(isolate_lock_witness lock, v8::Local<v8::Object> object, auto&&... args) -> Type&
			requires std::constructible_from<Type, decltype(args)...>;
		static auto unwrap(isolate_lock_witness lock, v8::Local<v8::Object> object) -> Type*;

	private:
		constexpr static auto pointer_tag = v8::CppHeapPointerTag::kDefaultTag;
};

// ---

template <class Type>
auto wrappable_of<Type>::wrap(isolate_lock_witness lock, v8::Local<v8::Object> object, auto&&... args) -> Type&
	requires std::constructible_from<Type, decltype(args)...> {
	auto* isolate = lock.isolate();
	auto& allocation_handle = isolate->GetCppHeap()->GetAllocationHandle();
	auto* instance = cppgc::MakeGarbageCollected<wrappable_of>(allocation_handle, std::forward<decltype(args)>(args)...);
	v8::Object::Wrap<pointer_tag>(isolate, object, instance);
	return *instance;
}

template <class Type>
auto wrappable_of<Type>::unwrap(isolate_lock_witness lock, v8::Local<v8::Object> object) -> Type* {
	if (object->IsApiWrapper()) {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
		auto* holder = static_cast<wrappable_holder*>(v8::Object::Unwrap<pointer_tag, v8::Object::Wrappable>(lock.isolate(), object));
		if (holder != nullptr && holder->is(typeid(Type))) {
			return static_cast<wrappable_of*>(holder);
		}
	}
	return nullptr;
}

#else
// Unsupported v8 version
export template <class Type>
class wrappable_of;
#endif

} // namespace js::iv8
