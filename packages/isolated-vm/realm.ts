import type * as ivm from "./backend.js";
import { Capability } from "./module.js";

namespace Realm {
	export type CapabilityMake = (subscriber: ivm.SubscriberCapability) => CapabilityInterface;
	export type CapabilitySignature = ((...args: unknown[]) => void) | ivm.SubscriberCapability;
	export type CapabilityInterface = Record<string, CapabilitySignature>;

	export interface CreateCapabilityOptions {
		origin: string;
	}
}

export class Realm {
	readonly #realm;

	/** @internal */
	constructor(realm: ivm.Realm) {
		this.#realm = realm;
	}

	/** @internal */
	static __extractRealm(this: void, realm: Realm) {
		return realm.#realm;
	}

	async createCapability(make: Realm.CapabilityMake, options: Realm.CreateCapabilityOptions): Promise<Capability> {
		const module = await this.#realm.createCapability(make, options);
		return new Capability(module);
	}

	async instantiateRuntime(): Promise<Capability> {
		const module = await this.#realm.instantiateRuntime();
		return new Capability(module);
	}
}
