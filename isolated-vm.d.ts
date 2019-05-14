declare module "isolated-vm" {
	type Transferable =
		| null
		| undefined
		| string
		| number
		| boolean
		| Isolate
		| Context
		| Script
		| ExternalCopy<any>
		| Copy<any>
		| Reference<any>
		| Dereference<any>
		| Module;

	/**
	 * This is the main reference to an isolate. Every handle to an isolate is
	 * transferable, which means you can give isolates references to each other. An
	 * isolate will remain valid as long as someone holds a handle to the isolate or
	 * anything created inside that isolate. Once an isolate is lost the garbage
	 * collector should eventually find it and clean up its memory. Since an isolate
	 * and all it contains can represent quite a large chunk of memory though you
	 * may want to explicitly call the dispose() method on isolates that you are
	 * finished with to get that memory back immediately.
	 */
	export class Isolate {
		constructor(options?: IsolateOptions);

		/**
		 * Isolate snapshots are a very useful feature if you intend to create
		 * several isolates running common libraries between them. A snapshot
		 * serializes the entire v8 heap including parsed code, global variables,
		 * and compiled code.
		 */
		static createSnapshot(
			scripts: ScriptCode[],
			warmup_script?: string
		): ExternalCopy<ArrayBuffer>;

		compileScript(code: string, scriptInfo?: ScriptInfo): Promise<Script>;

		compileScriptSync(code: string, scriptInfo?: ScriptInfo): Script;

		compileModule(code: string, scriptInfo?: ScriptInfo): Promise<Module>;
		compileModuleSync(code: string, scriptInfo?: ScriptInfo): Module;

		createContext(opts?: ContextOptions): Promise<Context>;

		createContextSync(opts?: ContextOptions): Context;

		createInspectorSession(): any;

		/**
		 * Returns heap statistics from v8.
		 *
		 * The return value is almost identical to the nodejs function
		 * v8.getHeapStatistics().
		 *
		 * See: https://nodejs.org/dist/latest-v8.x/docs/api/v8.html#v8_v8_getheapstatistics.
		 *
		 * This function returns one additional property:
		 * "externally_allocated_size" which is the total amount of
		 * currently allocated memory which is not included in the v8
		 * heap but counts against this isolate's "memoryLimit".
		 *
		 * ArrayBuffer instances over a certain size are externally
		 * allocated and will be counted here.
		 */
		getHeapStatistics(): Promise<HeapStatistics>;
		getHeapStatisticsSync(): HeapStatistics;

		/**
		 * Destroys this isolate and invalidates all references obtained from it.
		 */
		dispose(): void;

		/**
		 * The total CPU and wall time spent in this isolate. CPU time is the amount
		 * of time the isolate has spent actively doing work on the CPU. Wall time
		 * is the amount of time the isolate has been running, including passive
		 * time spent waiting (think "wall" like a clock on the wall). For instance,
		 * if an isolate makes a call into another isolate, wall time will continue
		 * increasing while CPU time will remain the same.
		 *
		 * The return format is [ seconds, nanoseconds ], which is the same as the
		 * nodejs method [process.hrtime](https://nodejs.org/api/process.html#process_process_hrtime_time).
		 * To convert this value to milliseconds you could do something like: (ret[0] + ret[1] / 1e9) * 1000.
		 * Some precision is lost in this conversion but for most applications it's probably not a big deal.
		 *
		 * Note that CPU time may vary drastically if there is contention for the CPU.
		 * This could occur if other processes are trying to do work, or if you have
		 * more than require('os').cpus().length isolates currently doing work in the same nodejs process.
		 */
		cpuTime: [number, number];
		wallTime: [number, number];

		/**
		 * Returns the total count of active Reference instances that belong to this isolate.
		 * Note that in certain cases many Reference instances in JavaScript will point to
		 * the same underlying reference handle, in which case this number will only reflect
		 * the underlying reference handle. This happens when you transfer a Reference instance
		 * via some method which accepts transferable values. This will also include underlying
		 * reference handles created by isolated-vm like Script or Context objects.
		 */
		referenceCount: number;

		/**
		 * Flag that indicates whether this isolate has been disposed.
		 */
		isDisposed: boolean;
	}

	export interface IsolateOptions {
		/**
		 * Memory limit that this isolate may use, in MB. Note that this is more of
		 * a guideline instead of a strict limit. A determined attacker could use
		 * 2-3 times this limit before their script is terminated.
		 */
		memoryLimit?: number;
		/**
		 * This is an optional snapshot created from createSnapshot which will be
		 * used to initialize the heap of this isolate.
		 */
		snapshot?: ExternalCopy<ArrayBuffer>;

		inspector?: boolean;
	}

	export interface ContextOptions {
		inspector?: boolean;
	}

	export interface HeapStatistics {
		total_heap_size: number;
		total_heap_size_executable: number;
		total_physical_size: number;
		total_available_size: number;
		used_heap_size: number;
		heap_size_limit: number;
		malloced_memory: number;
		peak_malloced_memory: number;
		does_zap_garbage: number;

		/**
		 * The total amount of currently allocated memory which is not
		 * included in the v8 heap but counts against this isolate's
		 * "memoryLimit".
		 */
		externally_allocated_size: number;
	}

	export interface ScriptCode extends ScriptInfo {
		/**
		 * Script code.
		 */
		code: string;
	}

	export interface ScriptInfo {
		/**
		 * Optional filename of this script, used in stack traces.
		 */
		filename?: string;

		/**
		 * Optional column offset of this script.
		 */
		columnOffset?: number;

		/**
		 * Optional line offset of this script.
		 */
		lineOffset?: number;

		/**
		 * Produce V8 cache data. Similar to the VM.Script option of the same name.
		 * If this is true then the returned script object will have cachedData set to an ExternalCopy handle.
		 * Note that this differs from the VM.Script option slightly in that cachedDataProduced is never set.
		 */
		produceCachedData?: boolean;

		/**
		 * This will consume cached compilation data from a previous call to this function.
		 * Please don't use produceCachedData and cachedData options at the same time.
		 * cachedDataRejected will be set to true if the supplied data was rejected by V8.
		 */
		cachedData?: ExternalCopy<ArrayBuffer>;
	}

	/**
	 * A script is a compiled chunk of JavaScript which can be executed in any
	 * context within a single isolate.
	 */
	export class Script {
		constructor();

		/**
		 * Runs a given script within a context. This will return the last value
		 * evaluated in a given script, as long as that value was transferable,
		 * otherwise undefined will be returned. For instance if your script was
		 * "let foo = 1; let bar = 2; bar = foo + bar" then the return value will be
		 * 3 because that is the last expression.
		 */
		run(context: Context, options?: ScriptRunOptions): Promise<any>;

		runIgnored(context: Context, options?: ScriptRunOptions): void;

		/**
		 * Runs a given script within a context synchronously. This will return the
		 * last value evaluated in a given script, as long as that value was
		 * transferable, otherwise undefined will be returned. For instance if your
		 * script was "let foo = 1; let bar = 2; bar = foo + bar" then the return
		 * value will be 3 because that is the last expression.
		 *
		 * @return [transferable]
		 */
		runSync(context: Context, options?: ScriptRunOptions): any;
	}

	export interface ScriptRunOptions {
		/**
		 * Maximum amount of time this script is allowed to run before execution is
		 * canceled. Default is no timeout.
		 */
		timeout?: number;
	}

	export interface ModuleEvaluateOptions {
		/**
		 * Maximum amount of time this module is allowed to run before execution is canceled. Default is no timeout.
		 */
		timeout: number;
	}

	/**
	 * A JavaScript module. Note that a Module can only run in the isolate which created it.
	 */
	export class Module {
		/**
		 * A read-only array of all dependency specifiers the module has.
		 */
		readonly dependencySpecifiers: string[];

		/**
		 * Returns a Reference containing all exported values.
		 */
		namespace: Reference<any>;

		/**
		 * Instantiate the module together with all its dependencies. Calling this more than once on a single module will have no effect.
		 * @param context The context the module should use.
		 * @param resolveCallback This callback is responsible for resolving all direct and indirect dependencies of this module. It accepts two parameters: specifier and referrer. It must return a Module instance or a promise which will be used to satisfy the dependency.
		 */
		instantiate(
			context: Context,
			resolveCallback: (
				specifier: string,
				referrer: Module
			) => Module | Promise<Module>
		): Promise<void>;

		/**
		 * Instantiate the module together with all its dependencies. Calling this more than once on a single module will have no effect.
		 * @param context The context the module should use.
		 * @param resolveCallback This callback is responsible for resolving all direct and indirect dependencies of this module. It accepts two parameters: specifier and referrer. It must return a Module instance which will be used to satisfy the dependency.
		 */
		instantiateSync(
			context: Context,
			resolveCallback: (specifier: string, referrer: Module) => Module
		): void;

		/**
		 * Evaluate the module and return the last expression (same as script.run). If evaluate is called more than once on the same module the return value from the first invocation will be returned (or thrown).
		 * @param options Optional
		 */
		evaluate(options?: ModuleEvaluateOptions): Promise<Transferable>;

		/**
		 * Evaluate the module and return the last expression (same as script.run). If evaluate is called more than once on the same module the return value from the first invocation will be returned (or thrown).
		 * @param options Optional
		 */
		evaluateSync(options?: ModuleEvaluateOptions): Transferable;
	}

	/**
	 * A context is a sandboxed execution environment within an isolate. Each
	 * context contains its own built-in objects and global space.
	 */
	export class Context {
		constructor();

		/**
		 * Returns a Reference to this context's global object.
		 */
		global: Reference<Object>;

		release(): void;
	}

	/**
	 * A instance of Reference is a pointer to a value stored in any isolate.
	 */
	export class Reference<T> {
		constructor(value: T);

		/**
		 * This is the typeof the referenced value, and is available at any time
		 * from any isolate. Note that this differs from the real typeof operator in
		 * that null is "null", and Symbols are "object".
		 */
		typeof: string;

		/**
		 * Creates a copy of the referenced value and internalizes it into this
		 * isolate. This uses the same copy rules as ExternalCopy.
		 */
		copy(): Promise<T>;

		/**
		 * Creates a copy of the referenced value and internalizes it into this
		 * isolate. This uses the same copy rules as ExternalCopy.
		 *
		 * @return JavaScript value of the reference.
		 */
		copySync(): T;

		/**
		 * Will attempt to return the actual value or object pointed to by this
		 * reference. Note that in order to call this function the reference must be
		 * owned by the current isolate, otherwise an error will be thrown.
		 */
		deref(opts?: ReferencingOptions): T;

		/**
		 * Returns an object, which when passed to another isolate will cause that
		 * isolate to dereference the handle.
		 */
		derefInto(opts?: ReferencingOptions): Dereference<T>;

		/**
		 * Releases this reference. If you're passing around a lot of references
		 * between isolates it's wise to release the references when you are done.
		 * Otherwise you may run into issues with isolates running out of memory
		 * because other isolates haven't garbage collected recently. After calling
		 * this method all attempts to access the reference will throw an error.
		 */
		release(): void;
		dispose(): void;

		/**
		 * Will access a reference as if using reference[property] and return a
		 * reference to that value.
		 */
		get(property: any): Promise<Reference<any>>;

		/**
		 * Will access synchronously a reference as if using reference[property] and
		 * return a reference to that value.
		 */
		getSync(property: any): Reference<any>;

		/**
		 * @return {boolean} Indicating whether or not this operation succeeded. I'm
		 * actually not really sure when false would be returned, I'm just giving
		 * you the result back straight from the v8 API.
		 */
		set(property: any, value: Transferable): Promise<boolean>;

		setIgnored(property: any, value: Transferable): void;

		/**
		 * @return {boolean} Indicating whether or not this operation succeeded. I'm
		 * actually not really sure when false would be returned, I'm just giving
		 * you the result back straight from the v8 API.
		 */
		setSync(property: any, value: Transferable): boolean;

		/**
		 * Will attempt to invoke an object as if it were a function. If the return
		 * value is transferable it will be returned to the called of apply,
		 * otherwise an error will be thrown.
		 */
		apply(
			receiver?: any,
			arguments?: Transferable[],
			options?: ScriptRunOptions
		): Promise<any>;

		applyIgnored(
			receiver?: any,
			arguments?: Transferable[],
			options?: ScriptRunOptions
		): void;

		/**
		 * Will attempt to invoke an object as if it were a function. If the return
		 * value is transferable it will be returned to the called of apply,
		 * otherwise an error will be thrown.
		 */
		applySync(
			receiver?: any,
			arguments?: Transferable[],
			options?: ScriptRunOptions
		): any;

		/**
		 * `applySyncPromise` is a special version of `applySync` which may only be
		 * invoked on functions belonging to the default isolate AND may only be
		 * invoked from a non-default thread. Functions invoked in this way may
		 * return a promise and the invoking isolate will wait for that promise to
		 * resolve before resuming execution. You can use this to implement functions
		 * like readFileSync in a way that doesn't block the default isolate. Note that
		 * the invoking isolate will not respond to any async functions until this
		 * promise is resolved, however synchronous functions will still function
		 * correctly. Misuse of this feature may result in deadlocked isolates, though
		 * the default isolate will never be at risk of a deadlock.
		 */
		applySyncPromise(
			receiver?: any,
			arguments?: Transferable[],
			options?: ScriptRunOptions
		): any;
	}

	export interface AutomaticallyReleasableOptions {
		// If true `release()` will automatically be called on this instance.
		release?: boolean;
	}

	export interface ReferencingOptions extends AutomaticallyReleasableOptions {}

	export interface ExternalCopyOptions {
		/**
		 * If true this will release ownership of the given resource from this isolate.
		 * This operation completes in constant time since it doesn't have to copy an
		 * arbitrarily large object. This only applies to ArrayBuffer and TypedArray instances.
		 */
		transferOut?: boolean;
	}

	export interface ExternalCopyCopyOptions
		extends AutomaticallyReleasableOptions {
		/**
		 * If true this will transfer the resource directly into this isolate,
		 * invalidating the ExternalCopy handle.
		 */
		transferIn?: boolean;
	}

	/**
	 * Dummy type referencing a type copied into a different Isolate.
	 */
	export interface Copy<T> {}
	/**
	 * Dummy type referencing a type dereferenced into a different Isolate.
	 */
	export interface Dereference<T> {}

	/**
	 * Instances of this class represent some value that is stored outside of any v8
	 * isolate. This value can then be quickly copied into any isolate.
	 */
	export class ExternalCopy<T> {
		/**
		 * Primitive values can be copied exactly as they are. Date objects will be
		 * copied as as Dates. ArrayBuffers, TypedArrays, and DataViews will be
		 * copied in an efficient format. All other objects will be passed to
		 * JSON.stringify() and stored in serialized form.
		 *
		 * @param value The value to copy
		 */
		constructor(value: T, options?: ExternalCopyOptions);

		/**
		 * Static property which will return the total number of bytes that isolated-vm
		 * has allocated outside of v8 due to instances of `ExternalCopy`.
		 */
		static totalExternalSize: number;

		/**
		 * Internalizes the ExternalCopy data into this isolate.
		 *
		 * @return JavaScript value of the external copy.
		 */
		copy(options?: ExternalCopyCopyOptions): T;

		/**
		 * Returns an object, which when passed to another isolate will cause that
		 * isolate to internalize a copy of this value.
		 */
		copyInto(options?: ExternalCopyCopyOptions): Copy<T>;

		/**
		 * Releases the reference to this copy. If there are other references to
		 * this copy elsewhere the copy will still remain in memory, but this handle
		 * will no longer be active. Disposing ExternalCopy instances isn't super
		 * important, v8 is a lot better at cleaning these up automatically because
		 * there's no inter-isolate dependencies.
		 */
		release(): void;
		dispose(): void;
	}

	/**
	 * C++ native module for v8 representation.
	 */
	export class NativeModule {
		/**
		 * Instantiate a native module with the full path to the compiled library.
		 * For instance, filename would represent the path to a .node file
		 * compiled using node-gyp.
		 *
		 * @param filename Full path to compiled library.
		 */
		constructor(filename: string);

		/**
		 * Instantiates the module with a Context by running the `InitForContext`
		 * symbol, throws if that symbol is not present.
		 *
		 * Returned Reference<NativeModule> should be dereferenced into a context
		 *
		 * @param context Context to initialize the module with.
		 */
		create(context: Context): Promise<Reference<NativeModule>>;

		/**
		 * Synchronous version of `create`
		 *
		 * @param context Context to initialize the module with.
		 */
		createSync(context: Context): Reference<NativeModule>;
	}
}
