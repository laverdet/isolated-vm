import * as ivm from "./backend.js";
import { Realm } from "./realm.js";

export type ImportAttributes = Record<string, string>;

export type ModuleLinker = (
	/**
	 * The specifier of this import: `import {} from "specifier"`
	 */
	specifier: string,

	/**
	 * The fully-resolved name of the importing module
	 */
	referrer: string | undefined,

	/**
	 * Import attributes of this import: `import {} from "specifier" with { type: 'json' }`
	 */
	attributes: ImportAttributes | undefined,
) => Module | Promise<Module>;

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

	/** @internal */
	constructor(agent: ivm.Agent, module: ivm.Module, requests: readonly ModuleRequest[]) {
		this.#agent = agent;
		this.#module = module;
		this.requests = requests;
	}

	evaluate(realm: Realm): Promise<unknown> {
		return ivm.evaluateModule(this.#agent, Realm.extractRealmInternal(realm), this.#module);
	}

	link(realm: Realm, linker: ModuleLinker): Promise<void> {
		return ivm.linkModule(this.#agent, Realm.extractRealmInternal(realm), this.#module, (specifier, parentName, attributes, callback) => {
			void async function() {
				try {
					const module = await linker(specifier, parentName, attributes);
					callback(null, module.#module);
				} catch (error) {
					callback(error);
				}
			}();
		});
	}
}
