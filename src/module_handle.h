#pragma once
#include <v8.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"

namespace ivm {

class IsolatedModule {
	public:
#if V8_AT_LEAST(6, 1, 328)
		class shared {
			private:
				using available_modules_type = std::unordered_multimap<int, IsolatedModule*>;
				static std::mutex mutex;
				static available_modules_type available_modules;

			public:
				// using raw pointers locks dangerous maybe, but couldn´t get it to work
				// with weak_ptr.
				static IsolatedModule* find(v8::Local<v8::Module> handle);
				static void add(IsolatedModule* ptr);
				static void remove(IsolatedModule* ptr);
	};

	private:
		std::recursive_mutex mutex;
		// required by ClassHandle used to construct the namespace reference
		std::shared_ptr<IsolateHolder> isolate;
		std::vector<std::string> dependency_specifiers;
		std::shared_ptr<RemoteHandle<v8::Module>> module_handle;
		std::shared_ptr<RemoteHandle<v8::Context>> context_handle;
		std::shared_ptr<RemoteHandle<v8::Value>> global_namespace;
		std::unordered_map<std::string, std::shared_ptr<IsolatedModule>> resolutions;

	private:
		// helper methods
		static v8::MaybeLocal<v8::Module> ResolveCallback(
				v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
				v8::Local<v8::Module> referrer);

		public:
		IsolatedModule(std::shared_ptr<IsolateHolder> isolate,
			std::shared_ptr<RemoteHandle<v8::Module>> handle,
			std::vector<std::string> dependency_specifiers);
		~IsolatedModule();

		// BasicLockable requirements
		void lock();
		void unlock();

		const std::vector<std::string>& GetDependencySpecifiers() const;

		void SetDependency(const std::string& specifier,
			std::shared_ptr<IsolatedModule> isolated_module);

		void Instantiate(std::shared_ptr<RemoteHandle<v8::Context>> _context_handle);

		std::unique_ptr<Transferable> Evaluate(std::size_t timeout);

		v8::Local<v8::Value> GetNamespace();

		friend bool operator==(const IsolatedModule&, const v8::Local<v8::Module>&);
#else
		inline void lock() {}
		inline void unlock() {};

		inline const std::vector<std::string> & GetDependencySpecifiers() const {
			throw js_generic_error("OPERATION NOT SUPPORTED");
		}

		inline void SetDependency(const std::string & specifier, std::shared_ptr<IsolatedModule> isolated_module) {
		}

		inline void Instantiate(std::shared_ptr<RemoteHandle<v8::Context>> _context_handle) {

		}

		std::unique_ptr<Transferable> Evaluate(std::size_t timeout) {
			throw js_generic_error("OPERATION NOT SUPPORTED");
		}

		v8::Local<v8::Value> GetNamespace() {
			throw js_generic_error("OPERATION NOT SUPPORTED");
		}
#endif
};

class ContextHandle;
class ModuleHandle : public TransferableHandle {
	private:
		class ModuleHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<IsolatedModule> isolated_module;
			public:
				ModuleHandleTransferable(std::shared_ptr<IsolateHolder>, std::shared_ptr<IsolatedModule>);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<IsolatedModule> isolated_module;
	public:
		ModuleHandle(std::shared_ptr<IsolateHolder> isolate, std::shared_ptr<IsolatedModule> isolated_module);

		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		v8::Local<v8::Value> GetDependencySpecifiers();

		v8::Local<v8::Value> SetDependency(v8::Local<v8::String> specifier, ModuleHandle* module_handle);

		template <int async>
		v8::Local<v8::Value> Instantiate(ContextHandle* context_handle);


		template <int async>
		v8::Local<v8::Value> Evaluate(v8::MaybeLocal<v8::Object> maybe_options);

		v8::Local<v8::Value> GetNamespace();
};

} // namespace ivm
