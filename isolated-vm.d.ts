declare module "isolated-vm" {
	export type Transferable =
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
	 * This is the main reference to an isolate. Every handle to an isolate is transferable, which
	 * means you can give isolates references to each other. An isolate will remain valid as long as
	 * someone holds a handle to the isolate or anything created inside that isolate. Once an isolate
	 * is lost the garbage collector should eventually find it and clean up its memory. Since an
	 * isolate and all it contains can represent quite a large chunk of memory though you may want to
	 * explicitly call the `dispose()` method on isolates that you are finished with to get that memory
	 * back immediately.
	 */
	export class Isolate {
		private __ivm_isolate: undefined;
		constructor(options?: IsolateOptions);

		/**
		 * The total CPU time spent in this isolate. CPU time is the amount of time the isolate has
		 * spent actively doing work on the CPU.
		 *
		 * Note that CPU time may vary drastically if there is contention for the CPU. This could occur
		 * if other processes are trying to do work, or if you have more than
		 * require('os').cpus().length isolates currently doing work in the same nodejs process.
		 */
		readonly cpuTime: [number, number];

		/**
		 * Flag that indicates whether this isolate has been disposed.
		 */
		readonly isDisposed: boolean;

		/**
		 * The total wall time spent in this isolate. Wall time is the amount of time the isolate has
		 * been running, including passive time spent waiting (think "wall" like a clock on the wall).
		 * For instance, if an isolate makes a call into another isolate, wall time will continue
		 * increasing while CPU time will remain the same.
		 */
		readonly wallTime: Hrtime;

		/**
		 * Returns the total count of active `Reference` instances that belong to this isolate. Note
		 * that in certain cases many `Reference` instances in JavaScript will point to the same
		 * underlying reference handle, in which case this number will only reflect the underlying
		 * reference handle. This happens when you transfer a `Reference` instance via some method which
		 * accepts transferable values. This will also include underlying reference handles created by
		 * isolated-vm like `Script` or `Context` objects.
		 */
		readonly referenceCount: number;

		/**
		 * Isolate snapshots are a very useful feature if you intend to create several isolates running
		 * common libraries between them. A snapshot serializes the entire v8 heap including parsed code,
		 * global variables, and compiled code. Check out the examples section for tips on using this.
		 *
		 * **Please note that versions of nodejs 10.4.0 - 10.9.0 may crash while using the snapshot
		 * feature.**
		 *
		 * @param warmup_script - Optional script to "warmup" the snapshot by triggering code compilation
		 */
		static createSnapshot(scripts: SnapshotScriptInfo[], warmup_script?: string): ExternalCopy<ArrayBuffer>;

		compileScript(code: string, scriptInfo?: ScriptInfo): Promise<Script>;
		compileScriptSync(code: string, scriptInfo?: ScriptInfo): Script;

		compileModule(code: string, scriptInfo?: ScriptInfo): Promise<Module>;
		compileModuleSync(code: string, scriptInfo?: ScriptInfo): Module;

		createContext(options?: ContextOptions): Promise<Context>;
		createContextSync(options?: ContextOptions): Context;

		createInspectorSession(): InspectorSession;

		/**
		 * Destroys this isolate and invalidates all references obtained from it.
		 */
		dispose(): void;

		/**
		 * Returns heap statistics from v8.
		 *
		 * The return value is almost identical to the nodejs function v8.getHeapStatistics().
		 *
		 * See: https://nodejs.org/dist/latest-v8.x/docs/api/v8.html#v8_v8_getheapstatistics.
		 *
		 * This function returns one additional property: "externally_allocated_size" which is the total
		 * amount of currently allocated memory which is not included in the v8 heap but counts against
		 * this isolate's "memoryLimit".
		 *
		 * ArrayBuffer instances over a certain size are externally allocated and will be counted here.
		 */
		getHeapStatistics(): Promise<HeapStatistics>;
		getHeapStatisticsSync(): HeapStatistics;
	}

	export type IsolateOptions = {
		/**
		 * Memory limit that this isolate may use, in MB. Note that this is more of a guideline
		 * instead of a strict limit. A determined attacker could use 2-3 times this limit before
		 * their script is terminated. Against non-hostile code this limit should be pretty close. The
		 * default is 128MB and the mimium is 8MB.
		 */
		memoryLimit?: number;

		/**
		 * Enable v8 inspector support in this isolate. See `inspector-example.js` in this repository
		 * for an example of how to use this.
		 */
		inspector?: boolean;

		/**
		 * This is an optional snapshot created from `createSnapshot` which will be used to initialize
		 * the heap of this isolate.
		 */
		snapshot?: ExternalCopy<ArrayBuffer>;
	};

	export type ContextOptions = {
		inspector?: boolean;
	};

	export type HeapStatistics = {
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
		 * The total amount of currently allocated memory which is not included in the v8 heap but
		 * counts against this isolate's "memoryLimit".
		 */
		externally_allocated_size: number;
	};

	/**
	 * Format is [ seconds, nanoseconds ], which is the same as the nodejs method
	 * [process.hrtime](https://nodejs.org/api/process.html#process_process_hrtime_time). To convert
	 * this value to milliseconds you could do something like: (ret[0] + ret[1] / 1e9) * 1000. Some
	 * precision is lost in this conversion but for most applications it's probably not a big deal.
	 */
	export type Hrtime = [ number, number ];

	/**
	 * A context is a sandboxed execution environment within an isolate. Each context contains its own
	 * built-in objects and global space.
	 */
	export class Context {
		private __ivm_context: undefined;
		private constructor();

		/**
		 * `Reference` to this context's global object. Note that if you call `context.release()` the
		 * global reference will be released as well.
		 */
		readonly global: Reference<Object>;

		/**
		 * Compiles and executes a script within a context. This will return the last value evaluated,
		 * as long as that value was transferable, otherwise `undefined` will be returned.
		 */
		eval<Options extends ContextEvalOptions>(
			code: string, options?: Options
		): Promise<ContextEvalResult<ResultTypeSync<Options>>>; // `ResultTypeSync` used intentionally
		evalIgnored(code: string, options?: ContextEvalOptions): void
		evalSync<Options extends ContextEvalOptions>(
			code: string, options?: Options
		): ContextEvalResult<ResultTypeSync<Options>>;

		/**
		 * Compiles and runs code as if it were inside a function, similar to the seldom-used `new
		 * Function(code)` constructor. You can pass arguments to the function and they will be
		 * available as `$0`, `$1`, and so on. You can also use `return` from the code.
		 */
		evalClosure<Options extends ContextEvalClosureOptions>(
			code: string, arguments?: ArgumentsTypeBidirectional<Options>[], options?: Options
		): Promise<ContextEvalResult<ResultTypeBidirectionalSync<Options>>>; // `ResultTypeBidirectionalSync` used intentionally
		evalClosureIgnored<Options extends ContextEvalClosureOptions>(
			code: string, arguments?: ArgumentsTypeBidirectional<Options>[], options?: Options
		): void
		evalClosureSync<Options extends ContextEvalClosureOptions>(
			code: string, arguments?: ArgumentsTypeBidirectional<Options>[], options?: Options
		): ContextEvalResult<ResultTypeBidirectionalSync<Options>>;

		/**
		 * Releases this reference to the context. You can call this to free up v8 resources
		 * immediately, or you can let the garbage collector handle it when it feels like it. Note that
		 * if there are other references to this context it will not be disposed. This only affects this
		 * reference to the context.
		 */
		release(): void;
	}

	export type ContextEvalOptions = CachedDataOptions & RunOptions & TransferOptions;
	export type ContextEvalClosureOptions = CachedDataOptions & RunOptions & TransferOptionsBidirectional;
	export type ContextEvalResult<Result> = CachedDataResult & { result: Result };

	/**
	 * A script is a compiled chunk of JavaScript which can be executed in any context within a single
	 * isolate.
	 */
	export class Script {
		private __ivm_script: undefined;
		private constructor();

		/**
		 * Releases the reference to this script, allowing the script data to be garbage collected.
		 * Functions and data created in the isolate by previous invocations to `script.run(...)` will
		 * still be alive in their respective contexts-- this only means that you can't invoke
		 * `script.run(...)` again with this reference.
		 */
		release(): void;

		/**
		 * Runs a given script within a context. This will return the last value evaluated in a given
		 * script, as long as that value was transferable, otherwise `undefined` will be returned. For
		 * instance if your script was "let foo = 1; let bar = 2; bar = foo + bar" then the return value
		 * will be 3 because that is the last expression.
		 */
		run<Options extends ScriptRunOptions>(context: Context, options?: Options): ResultTypeAsync<Options>;
		runIgnored(context: Context, options?: ScriptRunOptions): void;
		runSync<Options extends ScriptRunOptions>(context: Context, options?: Options): ResultTypeSync<Options>;
	}

	export type ScriptRunOptions = RunOptions & ReleaseOptions & TransferOptions;

	/**
	 * A JavaScript module. Note that a Module can only run in the isolate which created it.
	 */
	export class Module {
		private __ivm_module: undefined;
		private constructor();

		/**
		 * A read-only array of all dependency specifiers the module has.
		 */
		readonly dependencySpecifiers: string[];

		/**
		 * Returns a Reference containing all exported values.
		 */
		readonly namespace: Reference<any>;

		/**
		 * Instantiate the module together with all its dependencies. Calling this more than once on a
		 * single module will have no effect.
		 * @param context The context the module should use.
		 * @param resolveCallback This callback is responsible for resolving all direct and indirect
		 * dependencies of this module. It accepts two parameters: specifier and referrer. It must
		 * return a Module instance or a promise which will be used to satisfy the dependency.
		 */
		instantiate(
			context: Context,
			resolveCallback: (
				specifier: string,
				referrer: Module
			) => Module | Promise<Module>
		): Promise<void>;
		instantiateSync(
			context: Context,
			resolveCallback: (specifier: string, referrer: Module) => Module
		): void;

		/**
		 * Evaluate the module and return the last expression (same as script.run). If evaluate is
		 * called more than once on the same module the return value from the first invocation will be
		 * returned (or thrown).
		 * @param options Optional
		 */
		evaluate(options?: ScriptRunOptions): Promise<Transferable>;
		evaluateSync(options?: ScriptRunOptions): Transferable;
	}

	/**
	 * A instance of Reference is a pointer to a value stored in any isolate.
	 */
	export class Reference<T = any> {
		private __ivm_reference: undefined;
		constructor(value: T);

		/**
		 * This is the typeof the referenced value, and is available at any time
		 * from any isolate. Note that this differs from the real typeof operator in
		 * that null is "null", and Symbols are "object".
		 */
		readonly typeof: string;

		/**
		 * Creates a copy of the referenced value and internalizes it into this isolate. This uses the
		 * same copy rules as ExternalCopy.
		 */
		copy(): Promise<T>;

		/**
		 * Creates a copy of the referenced value and internalizes it into this isolate. This uses the
		 * same copy rules as ExternalCopy.
		 *
		 * @return JavaScript value of the reference.
		 */
		copySync(): T;

		/**
		 * Will attempt to return the actual value or object pointed to by this reference. Note that in
		 * order to call this function the reference must be owned by the current isolate, otherwise an
		 * error will be thrown.
		 */
		deref(options?: ReleaseOptions): T;

		/**
		 * Returns an object, which when passed to another isolate will cause that isolate to
		 * dereference the handle.
		 */
		derefInto(options?: ReleaseOptions): Dereference<T>;

		/**
		 * Releases this reference. If you're passing around a lot of references between isolates it's
		 * wise to release the references when you are done. Otherwise you may run into issues with
		 * isolates running out of memory because other isolates haven't garbage collected recently.
		 * After calling this method all attempts to access the reference will throw an error.
		 */
		release(): void;

		/**
		 * Delete a property from this reference, as if using `delete reference[property]`
		 */
		 delete(property: string): Promise<boolean>;
		 deleteIgnored(property: string): void;
		 deleteSync(property: string): boolean;

		/**
		 * Will access a reference as if using reference[property] and return a reference to that value.
		 */
		get<Options extends TransferOptions>(property: string, options?: Options): ResultTypeAsync<Options>;
		getSync<Options extends TransferOptions>(property: string, options?: Options): ResultTypeSync<Options>;

		/**
		 * Will access a reference as if using reference[property] and return a reference to that value.
		 *
		 * @return {boolean} Indicating whether or not this operation succeeded. I'm actually not really
		 * sure when false would be returned, I'm just giving you the result back straight from the v8
		 * API.
		 */
		set<Options extends TransferOptions>(property: string, value: ArgumentsType<Options>, options?: Options): Promise<boolean>;
		setIgnored<Options extends TransferOptions>(property: string, value: ArgumentsType<Options>, options?: Options): void;
		setSync<Options extends TransferOptions>(property: string, value: ArgumentsType<Options>, options?: Options): boolean;

		/**
		 * Will attempt to invoke an object as if it were a function. If the return
		 * value is transferable it will be returned to the called of apply,
		 * otherwise an error will be thrown.
		 */
		apply<Options extends ReferenceApplyOptions>(
			receiver?: Transferable, arguments?: ArgumentsTypeBidirectional<Options>[], options?: Options
		): ResultTypeBidirectionalAsync<Options>;
		applyIgnored<Options extends ReferenceApplyOptions>(
			receiver?: Transferable, arguments?: ArgumentsTypeBidirectional<Options>[], options?: Options
		): void;
		applySync<Options extends ReferenceApplyOptions>(
			receiver?: Transferable, arguments?: ArgumentsTypeBidirectional<Options>[], options?: Options
		): ResultTypeBidirectionalSync<Options>;

		/**
		 * `applySyncPromise` is a special version of `applySync` which may only be invoked on functions
		 * belonging to the default isolate AND may only be invoked from a non-default thread. Functions
		 * invoked in this way may return a promise and the invoking isolate will wait for that promise
		 * to resolve before resuming execution. You can use this to implement functions like
		 * readFileSync in a way that doesn't block the default isolate. Note that the invoking isolate
		 * will not respond to any async functions until this promise is resolved, however synchronous
		 * functions will still function correctly. Misuse of this feature may result in deadlocked
		 * isolates, though the default isolate will never be at risk of a deadlock.
		 */
		applySyncPromise<Options extends ReferenceApplyOptions>(
			receiver?: Transferable, arguments?: ArgumentsTypeBidirectional<Options>[], options?: Options
		): ResultTypeBidirectionalSync<Options>;
	}

	/**
	 * Dummy type referencing a type dereferenced into a different Isolate.
	 */
	export class Dereference<T> {
		private constructor();
		private __ivm_deref: undefined;
	}

	export type ReferenceApplyOptions = RunOptions & TransferOptionsBidirectional;

	/**
	 * Instances of this class represent some value that is stored outside of any v8
	 * isolate. This value can then be quickly copied into any isolate.
	 */
	export class ExternalCopy<T = any> {
		private __ivm_external_copy: undefined;

		/**
		 * Primitive values can be copied exactly as they are. Date objects will be copied as as Dates.
		 * ArrayBuffers, TypedArrays, and DataViews will be copied in an efficient format.
		 * SharedArrayBuffers will simply copy a reference to the existing memory and when copied into
		 * another isolate the new SharedArrayBuffer will point to the same underlying data. After
		 * passing a SharedArrayBuffer to ExternalCopy for the first time isolated-vm will take over
		 * management of the underlying memory block, so a "copied" SharedArrayBuffer can outlive the
		 * isolate that created the memory originally.
		 *
		 * All other objects will be copied in seralized form using the [structured clone
		 * algorithm](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Structured_clone_algorithm).
		 *
		 * `ExternalCopy` can copy objects with deeply nested *transferable* objects.
		 */
		constructor(value: T, options?: ExternalCopyOptions);

		/**
		 * Static property which will return the total number of bytes that isolated-vm has allocated
		 * outside of v8 due to instances of `ExternalCopy`.
		 */
		static readonly totalExternalSize: number;

		/**
		 * Internalizes the ExternalCopy data into this isolate.
		 *
		 * @return JavaScript value of the external copy.
		 */
		copy(options?: ExternalCopyCopyOptions): T;

		/**
		 * Returns an object, which when passed to another isolate will cause that isolate to
		 * internalize a copy of this value.
		 */
		copyInto(options?: ExternalCopyCopyOptions): Copy<T>;

		/**
		 * Releases the reference to this copy. If there are other references to this copy elsewhere the
		 * copy will still remain in memory, but this handle will no longer be active. Disposing
		 * ExternalCopy instances isn't super important, v8 is a lot better at cleaning these up
		 * automatically because there's no inter-isolate dependencies.
		 */
		release(): void;
	}

	/**
	 * Dummy type referencing a type copied into a different Isolate.
	 */
	export class Copy<T> {
		private constructor();
		private __ivm_copy: undefined;
	}

	export type ExternalCopyOptions = {
		/**
		 * An array of `ArrayBuffer` instances to transfer ownership. This behaves in a similar way to
		 * [`postMessage`](https://developer.mozilla.org/en-US/docs/Web/API/Worker/postMessage).
		 */
		transferList?: any[];
		/**
		 * If true this will release ownership of the given resource from this isolate. This operation
		 * completes in constant time since it doesn't have to copy an arbitrarily large object. This
		 * only applies to ArrayBuffer and TypedArray instances.
		 */
		transferOut?: boolean;
	};

	export type ExternalCopyCopyOptions = ReleaseOptions & {
		/**
		 * If true this will transfer the resource directly into this isolate, invalidating the
		 * ExternalCopy handle.
		 */
		transferIn?: boolean;
	};

	/**
	 * C++ native module for v8 representation.
	 */
	export class NativeModule {
		private __ivm_native_module: undefined;

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
		create(context: Context): Promise<Reference<any>>;

		/**
		 * Synchronous version of `create`
		 *
		 * @param context Context to initialize the module with.
		 */
		createSync(context: Context): Reference<any>;
	}

	export type InspectorSession = {
		dispatchProtocolMessage(message: string): void;
		dispose(): void;
		onNotification: (callId: number, message: string) => void;
		onResponse: (message: string) => void;
	};

	/**
	 * Most functions which compile or run code can produce and consume cached data. You can produce
	 * cached data and use the data in later invocations to drastically speed up parsing of the same
	 * script. You can even save this data to disk and use it in a different process. You can set both
	 * `cachedData` and `produceCachedData`, in which case new cached data will only be produced if
	 * the data supplied was invalid.
	 */
	export type CachedDataOptions = {
		/**
		 * This will consume cached compilation data from a previous call to this function. Please don't
		 * use `produceCachedData` and `cachedData` options at the same time. `cachedDataRejected` will
		 * be set to `true` if the supplied data was rejected by V8.
		 */
		cachedData?: ExternalCopy<ArrayBuffer>;
		/**
		 * Produce V8 cache data. Similar to the [VM.Script](https://nodejs.org/api/vm.html) option of
		 * the same name. If this is true then the returned object will have `cachedData` set to an
		 * ExternalCopy handle. Note that this differs from the VM.Script option slightly in that
		 * `cachedDataProduced` is never set.
		 */
		produceCachedData?: boolean;
	};

	export type CachedDataResult = {
		cachedData?: ExternalCopy<ArrayBuffer>;
		producedCacheData?: boolean;
	};

	export type ReleaseOptions = {
		/**
		 * If true release() will automatically be called on this instance.
		 */
		release?: boolean;
	};

	export type RunOptions = {
		/**
		 * Maximum amount of time in milliseconds this script is allowed to run before execution is
		 * canceled. Default is no timeout.
		 */
		timeout?: number;
	};

	/**
	 * You may optionally specify information on compiled code's filename. This is used in various
	 * debugging contexts within v8, including stack traces and the inspector. It is recommended to
	 * use a valid URI scheme, for example: `{ filename: 'file:///test.js' }`, otherwise some devtools
	 * may malfunction.
	 */
	export type ScriptOrigin = {
		/**
		 * Filename of this source code
		 */
		filename?: string;

		/**
		 * Column offset of this source code
		 */
		columnOffset?: number;

		/**
		 * Line offset of this source code
		 */
		lineOffset?: number;
	};

	export type SnapshotScriptInfo = ScriptOrigin & {
		/**
		 * Source code to set up this snapshot
		 */
		code: string;
	};
	export type ScriptInfo = CachedDataOptions & ScriptOrigin;

	/**
	 * Any function which moves data between isolates will accept these transfer options. By default
	 * only *[transferable]* values may pass between isolates. Without specifying one of these options
	 * the function may ignore the value, throw, or wrap it in a reference depending on the context.
	 *
	 * More advanced situations like transferring ownership of `ArrayBuffer` instances will require
	 * direct use of `ExternalCopy` or `Reference`.
	 */
	export type TransferOptions = {
		/**
		 * Automatically proxy any returned promises between isolates. This can be used in combination
		 * with the other transfer options.
		 */
		promise?: boolean;

		/**
		 * Automatically deep copy value
		 */
		copy?: boolean;

		/**
		 * Automatically wrap value in `ExternalCopy` instance
		 */
		externalCopy?: boolean;

		/**
		 * Automatically wrap value in `Reference` instance
		 */
		reference?: boolean;
	};

	export type TransferOptionsBidirectional = {
		/**
		 * `TransferOptions` for the values going *into* this isolate.
		 */
		arguments?: TransferOptions;
		/**
		 * `TransferOptions` for the values coming *out* of this isolate.
		 */
		result?: TransferOptions;
	};

	// Discriminating types for TransferOptions
	type WithPromise = { promise: true };
	type AsCopy = { copy: true };
	type AsExternal = { externalCopy: true };
	type AsReference = { reference: true };
	type WithTransfer = AsCopy | AsExternal | AsReference;

	// Arguments type for functions that accept TransferOptions
	type ArgumentsType<Options extends TransferOptions> =
		Options extends WithTransfer ?
			Options extends WithPromise ? Promise<any> : any :
		Options extends WithPromise ? Promise<Transferable> : Transferable;

	// Return type for functions that accept TransferOptions
	type ResultTypeBase<Options extends TransferOptions> =
		Options extends AsCopy ? any :
		Options extends AsExternal ? ExternalCopy<any> :
		Options extends AsReference ? Reference<any> :
		Transferable;
	type ResultTypeAsync<Options extends TransferOptions> = Promise<ResultTypeBase<Options>>;
	type ResultTypeSync<Options extends TransferOptions> =
		Options extends WithPromise ? Promise<ResultTypeBase<Options>> : ResultTypeBase<Options>;

	// Arguments type for functions that accept TransferOptionsBidirectional
	type ArgumentsTypeBidirectional<Options extends TransferOptionsBidirectional> =
		ArgumentsType<Options['arguments'] extends TransferOptions ? Options['arguments'] : {}>;

	// Result type for functions that accept TransferOptionsBidirectional
	type ResultTypeBidirectionalBase<Options extends TransferOptionsBidirectional> =
		ResultTypeBase<Options['result'] extends TransferOptions ? Options['result'] : {}>;
	type ResultTypeBidirectionalAsync<Options extends TransferOptionsBidirectional> = Promise<ResultTypeBidirectionalBase<Options>>;
	type ResultTypeBidirectionalSync<Options extends TransferOptionsBidirectional> =
		Options['result'] extends WithPromise ? Promise<ResultTypeBidirectionalBase<Options>> : ResultTypeBidirectionalBase<Options>;
}
