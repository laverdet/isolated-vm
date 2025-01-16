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
) => AbstractModule | Promise<AbstractModule>;

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

class AbstractModule {
	readonly #agent;
	readonly #module;

	/** @internal */
	constructor(agent: ivm.Agent, module: ivm.Module) {
		this.#agent = agent;
		this.#module = module;
	}

	/** @internal */
	static __extractAgent(this: void, module: AbstractModule) {
		return module.#agent;
	}

	/** @internal */
	static __extractModule(this: void, module: AbstractModule) {
		return module.#module;
	}
}

const { __extractAgent, __extractModule } = AbstractModule;

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
	constructor(agent: ivm.Agent, module: ivm.Module, requests: readonly ModuleRequest[]) {
		super(agent, module);
		this.requests = requests;
	}

	evaluate(realm: Realm): Promise<unknown> {
		return ivm.evaluateModule(__extractAgent(this), __extractRealm(realm), __extractModule(this));
	}

	link(realm: Realm, linker: ModuleLinker): Promise<void> {
		return ivm.linkModule(
			__extractAgent(this),
			__extractRealm(realm),
			__extractModule(this),
			(specifier, parentName, attributes, callback) => {
				void async function() {
					try {
						const module = await linker(specifier, parentName, attributes);
						callback(null, __extractModule(module));
					} catch (error) {
						callback(error);
					}
				}();
			});
	}
}
