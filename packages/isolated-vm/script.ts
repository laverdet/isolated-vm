import type * as ivm from "./backend.js";
import { runScript } from "./backend.js";
import { Realm } from "./realm.js";

const { extractRealmInternal } = Realm;

export class Script {
	readonly #agent;
	readonly #script;

	/** @internal */
	constructor(agent: ivm.Agent, script: ivm.Script) {
		this.#agent = agent;
		this.#script = script;
	}

	run(realm: Realm): Promise<unknown> {
		return runScript(this.#agent, this.#script, extractRealmInternal(realm));
	}
}
