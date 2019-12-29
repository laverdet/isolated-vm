#pragma once
#include <v8.h>
#include <uv.h>

#include <memory>

#include "transferable.h"
#include "context_handle.h"
#include "transferable.h"

namespace ivm {

class NativeModule {
	private:
		using init_t = void(*)(v8::Isolate *, v8::Local<v8::Context>, v8::Local<v8::Object>);
		uv_lib_t lib;
		init_t init;

	public:
		explicit NativeModule(const std::string& filename);
		NativeModule(const NativeModule&) = delete;
		NativeModule& operator= (const NativeModule&) = delete;
		~NativeModule();
		void InitForContext(v8::Isolate* isolate, v8::Local<v8::Context> context, v8::Local<v8::Object> target);
};

class NativeModuleHandle : public TransferableHandle {
	private:
		class NativeModuleTransferable : public Transferable {
			private:
				std::shared_ptr<NativeModule> module;
			public:
				explicit NativeModuleTransferable(std::shared_ptr<NativeModule> module);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<NativeModule> module;

		template <int async>
		v8::Local<v8::Value> Create(ContextHandle& context_handle);

	public:
		explicit NativeModuleHandle(std::shared_ptr<NativeModule> module);
		static v8::Local<v8::FunctionTemplate> Definition();
		static std::unique_ptr<NativeModuleHandle> New(v8::Local<v8::String> value);
		std::unique_ptr<Transferable> TransferOut() final;
};

} // namespace ivm
