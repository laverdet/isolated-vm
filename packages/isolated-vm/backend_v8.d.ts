declare module "backend_v8.node" {
	class Agent { readonly #private; }
	class Module { readonly #private; }
	class Realm { readonly #private; }

	class Script {
		run(realm: Realm, options: RunScriptOptions | undefined): Promise<MaybeCompletionOf<unknown>>;
	}

	type Callback<Type> = [
		(error: unknown | null, value?: Type) => void,
	];

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

	export type { Agent, Module, Realm, Script };

	type CompileModuleOptions = import("./agent.ts").Agent.CompileModuleOptions;
	type CompileScriptOptions = import("./agent.ts").Agent.CompileScriptOptions;
	type CreateAgentOptions = import("./agent.ts").Agent.CreateOptions;
	type CreateCapabilityOptions = import("./agent.ts").Agent.CreateCapabilityOptions;
	type ImportAttributes = import("./module.ts").ImportAttributes;
	type ModuleLinker = (specifier: string, referrer: string | undefined, attributes: ImportAttributes | undefined, ...callback: Callback<Module>) => void;
	type ModuleRequest = import("./module.ts").ModuleRequest;
	type RunScriptOptions = import("./script.ts").Script.RunScriptOptions;

	/** @internal */
	const exports: {
		compileModule: (agent: Agent, code: string, options: CompileModuleOptions | undefined) => Promise<[ Module, ModuleRequest[] ]>;
		compileScript: (agent: Agent, code: string, options: CompileScriptOptions | undefined) => Promise<MaybeCompletionOf<Script>>;
		createAgent: (options: CreateAgentOptions | undefined) => Promise<Agent>;
		createCapability: (agent: Agent, callback: (...args: unknown[]) => void, options: CreateCapabilityOptions) => Promise<Module>;
		createRealm: (agent: Agent) => Promise<Realm>;
		evaluateModule: (realm: Realm, module: Module) => Promise<unknown>;
		instantiateRuntime: (realm: Realm) => Promise<Module>;
		linkModule: (realm: Realm, module: Module, linker: ModuleLinker) => Promise<void>;
	};
	export default exports;
}
