declare module "backend_v8.node" {
	class Agent { readonly #private; }
	class Realm { readonly #private; }
	class Script { readonly #private; }

	export type { Agent, Realm, Script };

	type CreateAgentOptions = import("./agent.ts").Agent.CreateOptions;
	type CompileScriptOptions = import("./agent.ts").Agent.CompileScriptOptions;

	/** @internal */
	const exports: {
		compileScript: (agent: Agent, code: string, options: CompileScriptOptions | undefined) => Promise<Script>;
		createAgent: (options: CreateAgentOptions | undefined) => Promise<Agent>;
		createRealm: (agent: Agent) => Promise<Realm>;
		runScript: (agent: Agent, script: Script, realm: Realm) => Promise<Script>;
	};
	export default exports;
}
