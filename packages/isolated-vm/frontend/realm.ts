import type { Capability } from "./capability.js";

export { Realm } from "#backend";

export namespace Realm {
	export type CapabilityInterface = Record<string, CapabilitySignature>;
	export type CapabilitySignature =
		Capability |
		string;

	export interface CreateCapabilityOptions {
		origin: string;
	}
}
