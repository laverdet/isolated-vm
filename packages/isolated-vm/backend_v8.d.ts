declare module "#backend_v8" {
	type Callback<Type> = (error: unknown | null, value?: Type) => void;
	type CapabilityMake = import("./frontend/realm.ts").Realm.CapabilityMake;
	type CompileModuleOptions = import("./frontend/agent.ts").Agent.CompileModuleOptions;
	type CompileScriptOptions = import("./frontend/agent.ts").Agent.CompileScriptOptions;
	type CreateAgentOptions = import("./frontend/agent.ts").Agent.CreateOptions;
	type CreateCapabilityOptions = import("./frontend/realm.ts").Realm.CreateCapabilityOptions;
	type ImportAttributes = import("./frontend/module.ts").Module.ImportAttributes;
	type MaybeCompletionOf<T> = import("./utility/completion.ts").MaybeCompletionOf<T>;
	type ModuleLinker = (specifier: string, referrer: string | undefined, attributes: ImportAttributes | undefined, callback: Callback<Module>) => void;
	type RunScriptOptions = import("./frontend/script.ts").Script.RunScriptOptions;

	const Secret: unique symbol;
	/** @internal */
	export type Secret = typeof Secret;

	export class Agent {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		/** @internal */ _compileModule<As extends typeof Module>(as: As, code: string, options?: CompileModuleOptions | undefined): Promise<MaybeCompletionOf<InstanceType<As>>>;
		/** @internal */ static _create<As extends typeof Agent>(as: As, options?: CreateAgentOptions | undefined): Promise<InstanceType<As>>;
		compileScript(code: string, options?: CompileScriptOptions | undefined): Promise<MaybeCompletionOf<Script>>;
		createRealm(): Promise<Realm>;
	}

	export class Module {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		/** @internal */ _link(realm: Realm, linker: ModuleLinker): Promise<void>;
		evaluate(realm: Realm): Promise<unknown>;
	}

	export class Realm {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		createCapability(make: CapabilityMake, options: CreateCapabilityOptions): Promise<Module>;
		instantiateRuntime(): Promise<Module>;
	}

	export class Script {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		run(realm: Realm, options?: RunScriptOptions | undefined): Promise<MaybeCompletionOf<unknown>>;
	}

	export class SubscriberCapability {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		send(message: unknown): Promise<boolean>;
	}

	/** @internal */
	const exports: {
		Agent: typeof Agent;
		Module: typeof Module;
		Realm: typeof Realm;
		Script: typeof Script;
	};
	export default exports;
}
