#include "platform_delegate.h"

namespace ivm {
namespace {
PlatformDelegate delegate;
}
#if IVM_USE_PLATFORM_DELEGATE_HACK

void PlatformDelegate::InitializeDelegate() {
	auto* node_platform = reinterpret_cast<node::MultiIsolatePlatform*>(v8::internal::V8::GetCurrentPlatform());
	delegate = PlatformDelegate{node_platform};
	v8::V8::ShutdownPlatform();
	v8::V8::InitializePlatform(&delegate);
}

void PlatformDelegate::RegisterIsolate(v8::Isolate* isolate, IsolatePlatformDelegate* isolate_delegate) {
	auto result = delegate.isolate_map.write()->insert(std::make_pair(isolate, isolate_delegate));
	assert(result.second);
}

void PlatformDelegate::UnregisterIsolate(v8::Isolate* isolate) {
	auto result = delegate.isolate_map.write()->erase(isolate);
	assert(result == 1);
}

#else

void PlatformDelegate::InitializeDelegate() {
	auto node_platform = node::GetMainThreadMultiIsolatePlatform();
	delegate = PlatformDelegate{node_platform};
}

void PlatformDelegate::RegisterIsolate(v8::Isolate* isolate, IsolatePlatformDelegate* isolate_delegate) {
	delegate.node_platform->RegisterIsolate(isolate, isolate_delegate);
}

void PlatformDelegate::UnregisterIsolate(v8::Isolate* isolate) {
	delegate.node_platform->UnregisterIsolate(isolate);
}

#endif

} // namespace ivm
