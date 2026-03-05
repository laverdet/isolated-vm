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
		constructor(secret: Secret, ...args: unknown[]);
		compileModule(code: string, options?: CompileModuleOptions | undefined): Promise<MaybeCompletionOf<_Module>>;
		compileScript(code: string, options?: CompileScriptOptions | undefined): Promise<MaybeCompletionOf<Script>>;
		createRealm(): Promise<Realm>;
		static create(options?: CreateAgentOptions | undefined): Promise<_Agent>;
	}

	export class Module {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		/** @internal */ _link(realm: Realm, linker: ModuleLinkRecord): Promise<void>;
		evaluate(realm: Realm): Promise<unknown>;
	}

	export class Realm {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		acquireGlobalObject(): Promise<Reference>;
		createCapability(make: CapabilityMake, options: CreateCapabilityOptions): Promise<Module>;
		instantiateRuntime(): Promise<Module>;
	}

	export class Reference {
		readonly #private;
		constructor(secret: Secret, ...args: unknown[]);
		copy(): Promise<unknown>;
		get(property: string): Promise<Reference>;
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
		initialize: typeof initialize;
		Agent: typeof Agent;
		Module: typeof Module;
		Realm: typeof Realm;
		Script: typeof Script;
	};
	export default exports;
}
