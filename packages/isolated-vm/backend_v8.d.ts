declare module "backend_v8.node" {
	type Callback<Type> = (error: unknown | null, value?: Type) => void;
	type CompileModuleOptions = import("./agent.ts").Agent.CompileModuleOptions;
	type CompileScriptOptions = import("./agent.ts").Agent.CompileScriptOptions;
	type CreateAgentOptions = import("./agent.ts").Agent.CreateOptions;
	type CreateCapabilityOptions = import("./agent.ts").Agent.CreateCapabilityOptions;
	type ImportAttributes = import("./module.ts").ImportAttributes;
	type ModuleLinker = (specifier: string, referrer: string | undefined, attributes: ImportAttributes | undefined, callback: Callback<Module>) => void;
	type ModuleRequest = import("./module.ts").ModuleRequest;
	type RunScriptOptions = import("./script.ts").Script.RunScriptOptions;

	export class Agent {
		readonly #private;
		compileModule(code: string, options: CompileModuleOptions | undefined): Promise<[ Module, ModuleRequest[] ]>;
		compileScript(code: string, options: CompileScriptOptions | undefined): Promise<MaybeCompletionOf<Script>>;
		createCapability(callback: (...args: unknown[]) => void, options: CreateCapabilityOptions): Promise<Module>;
		createRealm(): Promise<Realm>;
		static create(options: CreateAgentOptions | undefined): Promise<Agent>;
	}

	export class Module {
		readonly #private;
		evaluate(realm: Realm): Promise<unknown>;
		link(realm: Realm, linker: ModuleLinker): Promise<void>;
	}

	export class Realm {
		readonly #private;
		instantiateRuntime(): Promise<Module>;
	}

	export class Script {
		readonly #private;
		run(realm: Realm, options: RunScriptOptions | undefined): Promise<MaybeCompletionOf<unknown>>;
	}

	export type CompletionOf<Type> = NormalCompletion<Type> | ThrowCompletion;
	export type MaybeCompletionOf<Type> = CompletionOf<Type> | undefined;

	export interface NormalCompletion<Type> {
		complete: true;
		error?: never;
		result: Type;
	}

	export interface ThrowCompletion {
		complete: false;
		error: unknown;
		result?: never;
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
