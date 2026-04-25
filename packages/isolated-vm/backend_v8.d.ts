declare module "#backend_v8" {
	type CapabilityMake = import("./frontend/realm.ts").Realm.CapabilityMake;
	type CompileModuleOptions = import("./frontend/agent.ts").Agent.CompileModuleOptions;
	type CompileScriptOptions = import("./frontend/agent.ts").Agent.CompileScriptOptions;
	type CreateAgentOptions = import("./frontend/agent.ts").Agent.CreateOptions;
	type CreateCapabilityOptions = import("./frontend/realm.ts").Realm.CreateCapabilityOptions;
	type MaybeCompletionOf<T> = import("./utility/completion.ts").MaybeCompletionOf<T>;
	type ModuleLinkRecord = import("./frontend/module.ts").Module.LinkRecord;
	type RunScriptOptions = import("./frontend/script.ts").Script.RunScriptOptions;
	type _Agent = import("./frontend/agent.ts").Agent;
	type _Module = import("./frontend/module.ts").Module;
	type _Reference<T> = import("./frontend/reference.ts").Reference<T>;

	interface InheritedClasses {
		Agent: object;
		Module: object;
	}

	export const initialize: (inherited: InheritedClasses) => void;

	const Secret: unique symbol;
	/** @internal */
	export type Secret = typeof Secret;

	export class Agent {
		readonly #private;
		protected constructor(secret: Secret, ...args: unknown[]);
		compileModule(code: string, options?: CompileModuleOptions | undefined): Promise<MaybeCompletionOf<_Module>>;
		compileScript(code: string, options?: CompileScriptOptions | undefined): Promise<MaybeCompletionOf<Script>>;
		createRealm(): Promise<Realm>;
		static create(options?: CreateAgentOptions | undefined): Promise<_Agent>;
	}

	export class Module {
		readonly #private;
		protected constructor(secret: Secret, ...args: unknown[]);
		/** @internal */ _link(realm: Realm, linker: ModuleLinkRecord): Promise<void>;
		evaluate(realm: Realm): Promise<MaybeCompletionOf<unknown>>;
	}

	export class NativeModule {
		readonly #private;
		protected constructor(secret: Secret, ...args: unknown[]);
		static create(filename: string): Promise<NativeModule>;
	}

	export class Realm {
		readonly #private;
		protected constructor(secret: Secret, ...args: unknown[]);
		acquireGlobalObject(): Promise<_Reference<Record<string, unknown>>>;
		createCapability(make: CapabilityMake, options: CreateCapabilityOptions): Promise<Module>;
		instantiateRuntime(): Promise<Module>;
	}

	export class Reference {
		readonly #private;
		protected constructor(secret: Secret, ...args: unknown[]);
		copy(): Promise<unknown>;
		get(property: string): Promise<Reference>;
		set(property: string, value: unknown): Promise<void>;
		invoke(args: unknown[]): Promise<MaybeCompletionOf<unknown>>;
	}

	export class Script {
		readonly #private;
		protected constructor(secret: Secret, ...args: unknown[]);
		run(realm: Realm, options?: RunScriptOptions | undefined): Promise<MaybeCompletionOf<unknown>>;
	}

	export class SubscriberCapability {
		readonly #private;
		protected constructor(secret: Secret, ...args: unknown[]);
		send(message: unknown): Promise<boolean>;
	}

	/** @internal */
	const exports: {
		initialize: typeof initialize;
		Agent: typeof Agent;
		Module: typeof Module;
		NativeModule: typeof NativeModule;
		Realm: typeof Realm;
		Script: typeof Script;
	};
	export default exports;
}
