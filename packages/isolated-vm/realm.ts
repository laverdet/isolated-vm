import type * as ivm from "./backend.js";
import { Capability } from "./module.js";

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

	async instantiateRuntime(): Promise<Capability> {
		const module = await this.#realm.instantiateRuntime();
		return new Capability(module);
	}
}
