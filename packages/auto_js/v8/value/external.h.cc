module;
#include <v8_js/version.h>
export module v8_js:external;
import :collected_handle;
import :lock;
import std;
import v8;

namespace js::iv8 {

// Untagged `v8::External` for pointer types.
export template <class Type>
class untagged_external : public v8::External {
	public:
		[[nodiscard]] auto value() const -> Type*;

		static auto make(isolate_lock_witness lock, Type* value) -> v8::Local<untagged_external>;
		static auto make_collected(const collected_handle_lock& lock, auto&&... args) -> v8::Local<untagged_external>
			requires std::constructible_from<Type, decltype(args)...>;

	private:
		constexpr static auto type_tag = v8::ExternalPointerTypeTag{1};
};

// Tagged `v8::External` which holds a v8-managed host object.
export template <class Type>
class tagged_external : public v8::External {
	private:
		class holder;

	public:
		[[nodiscard]] auto get() const -> Type&;
		[[nodiscard]] auto try_cast() const -> Type*;
		static auto make_collected(const collected_handle_lock& lock, auto&&... args) -> v8::Local<tagged_external>
			requires std::constructible_from<Type, decltype(args)...>;

	private:
		constexpr static auto type_tag = v8::ExternalPointerTypeTag{2};
};

// Inherited by `tagged_external<Type>::holder` instances
struct external_type_tag {
		std::reference_wrapper<const std::type_info> type_tag;
};

// Host object with type tag
template <class Type>
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class tagged_external<Type>::holder
		: public external_type_tag,
			public Type {
	public:
		explicit holder(auto&&... args)
			requires std::constructible_from<Type, decltype(args)...> :
				external_type_tag{typeid(Type)},
				Type{std::forward<decltype(args)>(args)...} {}
};

// ---

// untagged_external
template <class Type>
auto untagged_external<Type>::value() const -> Type* {
#if V8_HAS_TAGGED_EXTERNAL
	return static_cast<Type*>(Value(type_tag));
#else
	return static_cast<Type*>(Value());
#endif
}

template <class Type>
auto untagged_external<Type>::make(isolate_lock_witness lock, Type* value) -> v8::Local<untagged_external> {
#if V8_HAS_TAGGED_EXTERNAL
	return v8::External::New(lock.isolate(), value, type_tag).template As<untagged_external>();
#else
	return v8::External::New(lock.isolate(), value).template As<untagged_external>();
#endif
}

template <class Type>
auto untagged_external<Type>::make_collected(const collected_handle_lock& lock, auto&&... args) -> v8::Local<untagged_external>
	requires std::constructible_from<Type, decltype(args)...> {
	using collected_handle_type = collected_handle<untagged_external, Type>;
	auto collected_handle = collected_handle_type::make(lock, std::forward<decltype(args)>(args)...);
	auto value =
#if V8_HAS_TAGGED_EXTERNAL
		v8::External::New(lock.isolate(), collected_handle->get(), type_tag).template As<untagged_external>();
#else
		v8::External::New(lock.isolate(), collected_handle->get()).template As<untagged_external>();
#endif
	collected_handle_type::reset(lock, std::move(collected_handle), value);
	return value;
}

// tagged_external
template <class Type>
auto tagged_external<Type>::get() const -> Type& {
	auto* pointer = try_cast();
	if (pointer == nullptr) {
		throw std::logic_error{"external tag mismatch"};
	} else {
		return *pointer;
	}
}

template <class Type>
auto tagged_external<Type>::try_cast() const -> Type* {
	auto& object =
#if V8_HAS_TAGGED_EXTERNAL
		*static_cast<holder*>(Value(type_tag));
#else
		*static_cast<holder*>(Value());
#endif
	if (object.type_tag.get() == typeid(Type)) {
		return &object;
	} else {
		return nullptr;
	}
}

template <class Type>
auto tagged_external<Type>::make_collected(const collected_handle_lock& lock, auto&&... args) -> v8::Local<tagged_external>
	requires std::constructible_from<Type, decltype(args)...> {
	using collected_handle_type = collected_handle<tagged_external, holder>;
	auto collected_handle = collected_handle_type::make(lock, std::forward<decltype(args)>(args)...);
	auto value =
#if V8_HAS_TAGGED_EXTERNAL
		v8::External::New(lock.isolate(), collected_handle->get(), type_tag).template As<tagged_external>();
#else
		v8::External::New(lock.isolate(), collected_handle->get()).template As<tagged_external>();
#endif
	collected_handle_type::reset(lock, std::move(collected_handle), value);
	return value;
}

} // namespace js::iv8
