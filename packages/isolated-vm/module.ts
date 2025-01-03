import * as ivm from "./backend.js";

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
	readonly #realm;

	/** @internal */
	constructor(agent: ivm.Agent, realm: ivm.Realm, module: ivm.Module, requests: readonly ModuleRequest[]) {
		this.#agent = agent;
		this.#module = module;
		this.#realm = realm;
		this.requests = requests;
	}

	link(linker: ModuleLinker): Promise<void> {
		return ivm.linkModule(this.#agent, this.#realm, this.#module, (specifier, parentName, attributes, internal, callback) => {
			void async function() {
				try {
					const module = await linker(specifier, parentName, attributes);
					callback(internal, null, module.#module);
				} catch (error) {
					callback(internal, error);
				}
			}();
		});
	}
}
