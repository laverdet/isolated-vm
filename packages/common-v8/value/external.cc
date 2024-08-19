module;
#include <type_traits>
#include <utility>
export module ivm.v8:external;
import ivm.utility;
import ivm.value;
import v8;

namespace ivm::iv8 {

export class external : public v8::External {
	public:
		[[nodiscard]] auto materialize(std::type_identity<void*> /*tag*/) const -> void*;
		static auto Cast(v8::Value* value) -> external*;
};

export template <class Type>
class collected_external : non_moveable {
	public:
		explicit collected_external(collection_group& collection, auto&&... args) :
				collection{&collection},
				value{std::forward<decltype(args)>(args)...} {}

		auto operator*() -> Type& { return value; }
		auto operator->() -> Type* { return &value; }
		static auto make(collection_group& collection, v8::Isolate* isolate, auto&&... args) -> v8::Local<v8::External>;

	private:
		collection_group* collection;
		Type value;
		v8::Global<v8::External> global;
};

auto external::Cast(v8::Value* value) -> external* {
	return reinterpret_cast<external*>(v8::External::Cast(value));
}

auto external::materialize(std::type_identity<void*> /*tag*/) const -> void* {
	return static_cast<void*>(Value());
}

template <class Type>
auto collected_external<Type>::make(collection_group& collection, v8::Isolate* isolate, auto&&... args) -> v8::Local<v8::External> {
	auto collected = collection.make_ptr<collected_external<Type>>(collection, std::forward<decltype(args)>(args)...);
	auto external = v8::External::New(isolate, collected.get());
	collected->global.Reset(isolate, external);
	collected->global.SetWeak(
		collected.release(),
		[](const v8::WeakCallbackInfo<collected_external>& data) {
			auto* collected = static_cast<collected_external*>(data.GetParameter());
			collected->collection->collect(collected);
		},
		v8::WeakCallbackType::kParameter
	);
	return external;
}

export template <class Type>
auto make_collected_external(collection_group& collection, v8::Isolate* isolate, Type value) -> v8::Local<v8::External> {
	return collected_external<Type>::make(collection, isolate, std::move(value));
}

} // namespace ivm::iv8

namespace ivm::value {

template <class Meta, class Type>
struct accept<Meta, iv8::collected_external<Type>> {
		auto operator()(external_tag /*tag*/, auto value) -> iv8::collected_external<Type>& {
			auto* void_ptr = static_cast<void*>(value);
			return *static_cast<iv8::collected_external<Type>*>(void_ptr);
		}
};

} // namespace ivm::value
