#include <nan.h>

using namespace v8;

NAN_METHOD(fn) {
	info.GetReturnValue().Set(123);
}

extern "C" IVM_DLLEXPORT void InitForContext(Isolate* isolate, Local<Context> context, Local<Object> target) {
	Nan::Set(target, Nan::New("fn").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(fn)).ToLocalChecked());
}

NAN_MODULE_INIT(init) {
	Isolate* isolate = Isolate::GetCurrent();
	InitForContext(isolate, isolate->GetCurrentContext(), target);
}
NODE_MODULE(native, init);
