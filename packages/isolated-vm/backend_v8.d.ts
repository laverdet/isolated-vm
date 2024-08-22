declare module "*.backend_v8.node" {
	class Agent { readonly #private; }
	export namespace Agent {
		type Clock = Clock.Deterministic | Clock.Microtask | Clock.Realtime | Clock.System;
		namespace Clock {
			export interface Deterministic {
				type: "deterministic";
				epoch: Date;
				interval: number;
			}

			export interface Microtask {
				type: "microtask";
				epoch?: Date | undefined;
			}

			export interface Realtime {
				type: "realtime";
				epoch: Date;
			}

			export interface System {
				type: "system";
			}
		}

		interface CreateOptions {
			clock?: Clock | undefined;
			randomSeed?: number | undefined;
		}
	}

	class Realm { readonly #private; }
	class Script { readonly #private; }

	export type { Agent, Realm, Script };

	/** @internal */
	const exports: {
		compileScript: (agent: Agent, code: string) => Promise<Script>;
		createAgent: (options: Agent.CreateOptions) => Promise<Agent>;
		createRealm: (agent: Agent) => Promise<Realm>;
		runScript: (agent: Agent, script: Script, realm: Realm) => Promise<Script>;
	};
	export default exports;
}
