import * as ivm from "./backend.js";
import { Realm } from "./realm.js";

const { __extractRealm } = Realm;

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
) => AbstractModule | undefined | Promise<AbstractModule | undefined>;

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

export class AbstractModule {
	readonly #module;

	/** @internal */
	constructor(module: ivm.Module) {
		this.#module = module;
	}

	/** @internal */
	static __extractModule(this: void, module: AbstractModule) {
		return module.#module;
	}
}

const { __extractModule } = AbstractModule;

/**
 * A callback into nodejs which grants some kind of low-level capability to internal APIs.
 */
export class Capability extends AbstractModule {}

/**
 * Compiled `SourceTextModule`. Created with `agent.compileModule`.
 */
export class Module extends AbstractModule {
	readonly requests: readonly ModuleRequest[];

	/** @internal */
	constructor(module: ivm.Module, requests: readonly ModuleRequest[]) {
		super(module);
		this.requests = requests;
	}

	evaluate(realm: Realm): Promise<unknown> {
		return __extractModule(this).evaluate(__extractRealm(realm));
	}

	link(realm: Realm, linker: ModuleLinker): Promise<void> {
		return __extractModule(this).link(
			__extractRealm(realm),
			(specifier, parentName, attributes, callback) => {
				void async function() {
					try {
						const module = await linker(specifier, parentName, attributes);
						if (module === undefined) {
							let message = `Cannot find module '${specifier}'`;
							if (parentName !== undefined) {
								message += ` imported from ${parentName}`;
							}
							if (attributes !== undefined) {
								message += ` with attributes ${JSON.stringify(attributes)}`;
							}
							throw new Error(message);
						}
						callback(null, __extractModule(module));
					} catch (error) {
						callback(error);
					}
				}();
			});
	}
}
