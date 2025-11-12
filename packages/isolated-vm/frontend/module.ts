import * as backend from "#backend_v8";

export const AbstractModule: typeof backend.Module = backend.Module;
export type AbstractModule = backend.Module;

export namespace Module {
	export type ImportAttributes = Record<string, string>;

	export type Linker = (
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

	export interface Request {
		/**
		 * The specifier of this import: `import {} from "specifier"`
		 */
		specifier: string;

		/**
		 * Import attributes of this import: `import {} from "specifier" with { type: 'json' }`
		 */
		attributes?: ImportAttributes;
	}
}

/**
 * A callback into nodejs which grants some kind of low-level capability to internal APIs.
 */
export class Capability extends AbstractModule {}

/**
 * Compiled `SourceTextModule`. Created with `agent.compileModule`.
 */
export class Module extends backend.Module {
	readonly requests: readonly Module.Request[];

	constructor(secret: backend.Secret, requests: readonly Module.Request[]) {
		super(secret);
		this.requests = requests;
	}

	async link(realm: backend.Realm, linker: Module.Linker): Promise<void> {
		return this._link(
			realm,
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
						callback(null, module);
					} catch (error) {
						callback(error);
					}
				}();
			});
	}
}
