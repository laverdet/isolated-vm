import type * as ivm from "./backend.js";

export interface ModuleRequest {
	/**
	 * The specifier of this import: `import {} from "specifier"`
	 */
	specifier: string;

	/**
	 * Import attributes of this import: `import {} from "specifier" with { type: 'json' }`
	 */
	attributes?: ImportAttributes;
}

export class Module {
	readonly requests: readonly ModuleRequest[];
	readonly #agent;
	readonly #module;
	readonly #realm;

	/** @internal */
	constructor(agent: ivm.Agent, realm: ivm.Realm, module: ivm.Module, requests: readonly ModuleRequest[]) {
		this.#agent = agent;
		this.#module = module;
		this.#realm = realm;
		this.requests = requests;
	}
}
