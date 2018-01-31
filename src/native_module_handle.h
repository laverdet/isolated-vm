#pragma once
#include <v8.h>
#include <uv.h>

#include <memory>

#include "transferable.h"
#include "context_handle.h"
#include "transferable_handle.h"

namespace ivm {

class NativeModuleHandle : public TransferableHandle {
	private:
		class NativeModule {
			private:
				typedef void (*init_t)(v8::Isolate*, v8::Local<v8::Context>, v8::Local<v8::Object>);
				uv_lib_t lib;
				init_t init;

			public:
				NativeModule(const std::string& filename);
				NativeModule(const NativeModule&) = delete;
				~NativeModule();
				void InitForContext(v8::Isolate* isolate, v8::Local<v8::Context> context, v8::Local<v8::Object> target);
		};

		class NativeModuleTransferable : public Transferable {
			private:
				std::shared_ptr<NativeModule> module;
			public:
				NativeModuleTransferable(std::shared_ptr<NativeModule> module);
				virtual v8::Local<v8::Value> TransferIn() override;
		};

		std::shared_ptr<NativeModule> module;

		template <bool async>
		v8::Local<v8::Value> Create(ContextHandle* context);

	public:
		NativeModuleHandle(std::shared_ptr<NativeModule> module);
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		static std::unique_ptr<NativeModuleHandle> New(v8::Local<v8::String> value);
		virtual std::unique_ptr<Transferable> TransferOut() override;
};

}
