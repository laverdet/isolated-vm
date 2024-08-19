import type * as ivm from "./backend.js";
import { compileScript, createAgent, createRealm } from "./backend.js";
import { Realm } from "./realm.js";
import { Script } from "./script.js";

export class Agent {
	readonly #agent;

	/** @internal */
	private	constructor(agent: ivm.Agent) {
		this.#agent = agent;
	}

	static async create(): Promise<Agent> {
		const agent = await createAgent();
		return new Agent(agent);
	}

	async createRealm(): Promise<Realm> {
		const realm = await createRealm(this.#agent);
		return new Realm(realm);
	}

	async compileScript(code: string): Promise<Script> {
		const script = await compileScript(this.#agent, code);
		return new Script(this.#agent, script);
	}
}
