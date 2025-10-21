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

export namespace Script {
	export interface RunScriptOptions {
		/**
		 * Maximum amount of time in milliseconds this script is allowed to run before execution is
		 * canceled. Default is no timeout.
		 */
		timeout?: number;
	}
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
	readonly #script;

	/** @internal */
	constructor(script: ivm.Script) {
		this.#script = script;
	}

	run(realm: Realm, options?: Script.RunScriptOptions): Promise<unknown> {
		return runScript(this.#script, __extractRealm(realm), options);
	}
}
