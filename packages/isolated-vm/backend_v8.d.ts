declare module "backend_v8.node" {
	class Agent { readonly #private; }
	class Module { readonly #private; }
	class Realm { readonly #private; }
	class Script { readonly #private; }

	export type { Agent, Module, Realm, Script };

	type CreateAgentOptions = import("./agent.ts").Agent.CreateOptions;
	type CompileScriptOptions = import("./agent.ts").Agent.CompileScriptOptions;
	type ModuleRequest = import("./module.ts").ModuleRequest;

	/** @internal */
	const exports: {
		compileModule: (agent: Agent, realm: Realm, code: string) => Promise<ModuleRequest[]>;
		compileScript: (agent: Agent, code: string, options: CompileScriptOptions | undefined) => Promise<Script>;
		createAgent: (options: CreateAgentOptions | undefined) => Promise<Agent>;
		createRealm: (agent: Agent) => Promise<Realm>;
		runScript: (agent: Agent, script: Script, realm: Realm) => Promise<Script>;
	};
	export default exports;
}
