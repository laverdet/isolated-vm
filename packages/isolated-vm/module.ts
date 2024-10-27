import type * as ivm from "./backend.js";

export interface ModuleRequest {
	/**
	 * The specifier of this import: `import {} from "specifier"`
	 */
	specifier: string;

	/**
	 * Import assertions of this import: `import {} from "specifier" assert { type: 'json' }`
	 */
	assertions?: ImportAssertions;
}

export type ImportAssertions = Record<string, string>;

export class Module {
	readonly requests: readonly ModuleRequest[];
	readonly #agent;
	readonly #module;

	/** @internal */
	constructor(agent: ivm.Agent, module: ivm.Module, requests: readonly ModuleRequest[]) {
		this.#agent = agent;
		this.#module = module;
		this.requests = requests;
	}
}
