declare module "backend_v8.node" {
	class Agent { readonly #private; }
	class Module { readonly #private; }
	class Realm { readonly #private; }
	class Script { readonly #private; }

	type Callback<Type> = [
		(error: unknown | null, value?: Type) => void,
	];

	export type { Agent, Module, Realm, Script };

	type CompileModuleOptions = import("./agent.ts").Agent.CompileModuleOptions;
	type CompileScriptOptions = import("./agent.ts").Agent.CompileScriptOptions;
	type CreateAgentOptions = import("./agent.ts").Agent.CreateOptions;
	type ImportAttributes = import("./module.ts").ImportAttributes;
	type ModuleLinker = (specifier: string, referrer: string | undefined, attributes: ImportAttributes | undefined, ...callback: Callback<Module>) => void;
	type ModuleRequest = import("./module.ts").ModuleRequest;

	/** @internal */
	const exports: {
		compileModule: (agent: Agent, realm: Realm, code: string, options: CompileModuleOptions | undefined) => Promise<[ Module, ModuleRequest[] ]>;
		compileScript: (agent: Agent, code: string, options: CompileScriptOptions | undefined) => Promise<Script>;
		createAgent: (options: CreateAgentOptions | undefined) => Promise<Agent>;
		createRealm: (agent: Agent) => Promise<Realm>;
		linkModule: (agent: Agent, realm: Realm, module: Module, linker: ModuleLinker) => Promise<void>;
		runScript: (agent: Agent, script: Script, realm: Realm) => Promise<Script>;
	};
	export default exports;
}
