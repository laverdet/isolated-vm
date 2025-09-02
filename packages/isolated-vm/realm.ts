import type * as ivm from "./backend.js";
import { instantiateRuntime } from "./backend.js";
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
		const module = await instantiateRuntime(this.#realm);
		return new Capability(module);
	}
}
