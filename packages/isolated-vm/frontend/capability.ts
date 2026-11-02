import { extend } from "@isolated-vm/experimental/utility/object";
import * as backend from "#backend";

export type Capability = backend.Capability;
export const Capability: CapabilityConstructor = backend.Capability as never;

interface CapabilityConstructor extends backend.CapabilityConstructor {
	/**
	 * Creates a synchronous capability from the given callback. When invoked from an isolate the calling task is
	 * suspended until the value, or promise, returned by `callback` settles. The settled value is returned, or thrown,
	 * synchronously inside the isolate.
	 *
	 * Capabilities are one-shot objects. They are consumed by `realm.createCapability`.
	 */
	sync: (callback: (...args: any[]) => unknown) => backend.Capability;
}

const { _sync } = backend.Capability;

extend(Capability, {}, {
	sync: callback => {
		if (typeof callback !== "function") {
			throw new TypeError("Capability callback must be a function");
		}
		return _sync((resolve, reject, args) => {
			void async function() {
				try {
					resolve(await callback(...args));
				} catch (error) {
					reject(error);
				}
			}();
		});
	},
});
