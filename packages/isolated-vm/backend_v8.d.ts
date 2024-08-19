declare module "*.backend_v8.node" {
	declare class Agent { readonly #private; }
	declare class Realm { readonly #private; }
	declare class Script { readonly #private; }

	/** @internal */
	export type { Agent, Realm, Script };

	declare const compileScript: (agent: Agent, code: string) => Promise<Script>;
	declare const createAgent: () => Promise<Agent>;
	declare const createRealm: (agent: Agent) => Promise<Realm>;
	declare const runScript: (agent: Agent, script: Script, realm: Realm) => Promise<Script>;

	/** @internal */
	export default module = {
		compileScript,
		createAgent,
		createRealm,
		runScript,
	};
}
