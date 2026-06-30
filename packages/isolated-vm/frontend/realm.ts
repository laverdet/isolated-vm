import * as backend from "#backend";

export { Realm } from "#backend";

export namespace Realm {
	export type CapabilityInterface = Record<string, CapabilitySignature>;
	export type CapabilityMake = (subscriber: backend.SubscriberCapability) => CapabilityInterface;
	export type CapabilitySignature = ((...args: unknown[]) => void) | backend.SubscriberCapability;

	export interface CreateCapabilityOptions {
		origin: string;
	}
}
