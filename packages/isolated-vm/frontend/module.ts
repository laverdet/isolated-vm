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

	/** Internal-ish */
	export interface LinkRecord {
		/** Modules corresponding to `payload` ids */
		modules: AbstractModule[];
		/** [[ count, module... ]... ] */
		payload: number[];
	}

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
	readonly specifier: string | undefined;

	constructor(secret: backend.Secret, specifier: string | undefined, requests: readonly Module.Request[]) {
		super(secret);
		this.specifier = specifier;
		this.requests = requests;
	}

	async link(realm: backend.Realm, linker: Module.Linker): Promise<void> {
		const modules: AbstractModule[] = [];
		const payload: number[] = [];
		const seen = new Map<AbstractModule, number>();
		const link = async (module: AbstractModule): Promise<number> => {
			const existing = seen.get(module);
			if (existing !== undefined) {
				return existing;
			}
			const moduleId = modules.length;
			seen.set(module, moduleId);
			modules.push(module);
			if (module instanceof Module) {
				const payloadIndex = payload.length + 1;
				payload.push(module.requests.length, ...module.requests.map(() => -1));
				await Promise.all(module.requests.map(async ({ specifier, attributes }, ii) => {
					const result = await linker(specifier, module.specifier, attributes);
					if (result === undefined) {
						let message = `Cannot find module '${specifier}'`;
						if (module.specifier !== undefined) {
							message += ` imported from ${module.specifier}`;
						}
						if (attributes !== undefined) {
							message += ` with attributes ${JSON.stringify(attributes)}`;
						}
						throw new Error(message);
					}
					payload[payloadIndex + ii] = await link(result);
				}));
			} else {
				payload.push(0);
			}
			return moduleId;
		};
		await link(this);
		return this._link(realm, { modules, payload });
	}
}
