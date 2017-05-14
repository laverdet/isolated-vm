#include "shareable_isolate.h"

using namespace std;
using namespace v8;

namespace ivm {

// Template specializations for IsolateSpecific<FunctionTemplate>
template<>
MaybeLocal<FunctionTemplate> ShareableIsolate::IsolateSpecific<FunctionTemplate>::Deref() const {
  return Deref<FunctionTemplate, decltype(ShareableIsolate::specifics_ft), &ShareableIsolate::specifics_ft>();
}

template<>
void ShareableIsolate::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle) {
	Reset<FunctionTemplate, decltype(ShareableIsolate::specifics_ft), &ShareableIsolate::specifics_ft>(handle);
}

// Static variable declarations
thread_local ShareableIsolate* ShareableIsolate::current;
size_t ShareableIsolate::specifics_count = 0;

}
