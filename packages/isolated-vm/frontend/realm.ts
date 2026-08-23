export { Realm } from "#backend";

export namespace Realm {
	export type CapabilityInterface = Record<string, CapabilitySignature>;
	export type CapabilitySignature =
		((...args: unknown[]) => void) |
		string;

	export interface CreateCapabilityOptions {
		origin: string;
	}
}
