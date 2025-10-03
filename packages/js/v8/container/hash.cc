module;
#include <concepts>
#include <functional>
export module v8_js:hash;
import ivm.utility;
import v8;

namespace js::iv8 {

// Hash based on the physical location of the local handle's value
export struct address_hash : std::hash<void*> {
		using std::hash<void*>::operator();

		template <class Type>
		auto operator()(v8::Local<Type> local) const -> std::size_t {
			return (*this)(*local);
		}
};

// Concept matching `v8::Local<v8::Name>`, `v8::Local<v8::Object>`, etc.
export template <class Type>
concept identity_hashable = requires(Type handle) {
	{ handle.GetIdentityHash() } -> std::same_as<int>;
};

// Hash based on intrinsic identity. Good for long term storage.
export struct identity_hash : std::hash<int> {
		using is_transparent = void;
		using std::hash<int>::operator();

		template <identity_hashable Type>
		auto operator()(v8::Local<Type> local) const -> std::size_t {
			return (*this)(local->GetIdentityHash());
		}

		auto operator()(const identity_hashable auto& hashable) const -> std::size_t {
			return (*this)(hashable.GetIdentityHash());
		}
};

} // namespace js::iv8
