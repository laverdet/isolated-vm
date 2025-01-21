module;
#include <concepts>
#include <functional>
export module v8_js.identity_hash;
import ivm.utility;
import v8;

namespace js::iv8 {

export template <class Type>
concept identity_hashable = requires(Type handle) {
	{ handle.GetIdentityHash() } -> std::same_as<int>;
};

export struct identity_hash : std::hash<int> {
		using is_transparent = void;
		using std::hash<int>::operator();
		template <identity_hashable Type>
		auto operator()(v8::Local<Type> local) const {
			return (*this)(local->GetIdentityHash());
		}
		auto operator()(const identity_hashable auto& hashable) const {
			return (*this)(hashable.GetIdentityHash());
		}
};

} // namespace js::iv8
