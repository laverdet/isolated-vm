import type { Constructor } from "@isolated-vm/experimental/utility/object";
import { extend } from "@isolated-vm/experimental/utility/object";
import * as backend from "#backend";

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
		attributes?: ImportAttributes,
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
export interface Module extends AbstractModule {
	link: (realm: backend.Realm | null, linker: Module.Linker) => Promise<void>;
}

extend(backend.Module as unknown as Constructor<Module>, {
	async link(this: Module, realm, linker) {
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
			const { requests, specifier: referrer } = module;
			const payloadIndex = payload.length + 1;
			payload.push(requests.length, ...requests.map(() => -1));
			await Promise.all(requests.map(async ({ specifier, attributes }, ii) => {
				const result = await linker(specifier, referrer, attributes);
				if (result === undefined) {
					let message = `Cannot find module '${specifier}'`;
					if (referrer !== undefined) {
						message += ` imported from ${referrer}`;
					}
					if (attributes !== undefined) {
						message += ` with attributes ${JSON.stringify(attributes)}`;
					}
					throw new Error(message);
				}
				payload[payloadIndex + ii] = await link(result);
			}));
			return moduleId;
		};
		await link(this);
		return this._link(realm, { modules, payload });
	},
});
