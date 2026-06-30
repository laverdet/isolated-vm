type CapabilityMake = import("@isolated-vm/experimental/frontend/realm", { with: { "resolution-mode": "import" }}).Realm.CapabilityMake;
type _Agent = import("@isolated-vm/experimental/frontend/agent", { with: { "resolution-mode": "import" }}).Agent;
type _Module = import("@isolated-vm/experimental/frontend/module", { with: { "resolution-mode": "import" }}).Module;
type _Reference<T> = import("@isolated-vm/experimental/frontend/reference", { with: { "resolution-mode": "import" }}).Reference<T>;
type CompileModuleOptions = import("@isolated-vm/experimental/frontend/agent", { with: { "resolution-mode": "import" }}).Agent.CompileModuleOptions;
type CompileScriptOptions = import("@isolated-vm/experimental/frontend/agent", { with: { "resolution-mode": "import" }}).Agent.CompileScriptOptions;
type CreateAgentOptions = import("@isolated-vm/experimental/frontend/agent", { with: { "resolution-mode": "import" }}).Agent.CreateOptions;
type CreateCapabilityOptions = import("@isolated-vm/experimental/frontend/realm", { with: { "resolution-mode": "import" }}).Realm.CreateCapabilityOptions;
type CreateNativeModuleOptions = import("@isolated-vm/experimental/frontend/native_module", { with: { "resolution-mode": "import" }}).NativeModule.CreateOptions;
type MaybeCompletionOf<T> = import("@isolated-vm/experimental/utility/completion", { with: { "resolution-mode": "import" }}).MaybeCompletionOf<T>;
type ModuleLinkRecord = import("@isolated-vm/experimental/frontend/module", { with: { "resolution-mode": "import" }}).Module.LinkRecord;
type RunScriptOptions = import("@isolated-vm/experimental/frontend/script", { with: { "resolution-mode": "import" }}).Script.RunScriptOptions;

interface InheritedClasses {
	Agent: object;
	Module: object;
}

export const initialize: (inherited: InheritedClasses) => void;

declare const Secret: unique symbol;
/** @internal */
export type Secret = typeof Secret;

export class Agent {
	readonly #private;
	protected constructor(secret: Secret, ...args: unknown[]);
	disposeAsync(): Promise<void>;
	compileModule(code: string, options?: CompileModuleOptions): Promise<MaybeCompletionOf<_Module>>;
	compileScript(code: string, options?: CompileScriptOptions): Promise<MaybeCompletionOf<Script>>;
	createRealm(): Promise<Realm>;
	static create(options?: CreateAgentOptions): Promise<_Agent>;
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
	instantiate(realm: Realm): Promise<Module>;
	static create(filename: string, options: CreateNativeModuleOptions): Promise<NativeModule>;
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
	run(realm: Realm, options?: RunScriptOptions): Promise<MaybeCompletionOf<unknown>>;
}

export class SubscriberCapability {
	readonly #private;
	protected constructor(secret: Secret, ...args: unknown[]);
	send(message: unknown): Promise<boolean>;
}

/** @internal */
declare const exports: {
	initialize: typeof initialize;
	Agent: typeof Agent;
	Module: typeof Module;
	NativeModule: typeof NativeModule;
	Realm: typeof Realm;
	Script: typeof Script;
};
export default exports;
