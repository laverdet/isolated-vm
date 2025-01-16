import type * as ivm from "./backend.js";
import { runScript } from "./backend.js";
import { Realm } from "./realm.js";

const { __extractRealm } = Realm;

export interface SourceOrigin {
	/**
	 * The name of this source. This is displayed in stack traces, and also guides module resolution
	 * of `import` statements and expressions. This does not *need* to be a URL, but it really should
	 * be.
	 */
	name?: string;

	/**
	 * The location of this source within its document. You would use this to reproduce a `<script />`
	 * tag in an HTML file. The script is actually located at an offset within another document.
	 */
	location?: Location;
}

export interface CapabilityOrigin extends SourceOrigin {
	name: string;
	location?: never;
}

export interface Location {
	line: number;
	column: number;
}

export class Script {
	readonly #agent;
	readonly #script;

	/** @internal */
	constructor(agent: ivm.Agent, script: ivm.Script) {
		this.#agent = agent;
		this.#script = script;
	}

	run(realm: Realm): Promise<unknown> {
		return runScript(this.#agent, this.#script, __extractRealm(realm));
	}
}
