import type * as ivm from "./backend.js";

export class Realm {
	readonly #realm;

	/** @internal */
	constructor(realm: ivm.Realm) {
		this.#realm = realm;
	}

	/** @internal */
	static extractRealmInternal(this: void, realm: Realm) {
		return realm.#realm;
	}
}
