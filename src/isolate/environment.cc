#include "environment.h"
#include <v8-platform.h>

using namespace std;
using namespace v8;

namespace ivm {

// Template specializations for IsolateSpecific<FunctionTemplate>
template<>
MaybeLocal<FunctionTemplate> IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Deref() const {
  return Deref<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>();
}

template<>
void IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle) {
	Reset<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>(handle);
}

// Static variable declarations
thread_local IsolateEnvironment* IsolateEnvironment::current;
size_t IsolateEnvironment::specifics_count = 0;
thread_pool_t IsolateEnvironment::thread_pool(8);
uv_async_t IsolateEnvironment::root_async;
std::thread::id IsolateEnvironment::default_thread;
std::atomic<int> IsolateEnvironment::uv_refs(0);
shared_ptr<IsolateEnvironment::BookkeepingStatics> IsolateEnvironment::bookkeeping_statics_shared = make_shared<IsolateEnvironment::BookkeepingStatics>();

}
