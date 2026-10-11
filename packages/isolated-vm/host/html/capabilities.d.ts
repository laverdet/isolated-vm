declare module "isolated-vm:capability/timers" {
	import type { Reference } from "@isolated-vm/experimental/runtime";

	/** Hands the host a reference to the function which runs the timers that are due. */
	export function initialize(wake: Reference<() => void>): void;

	/** Asks the host to invoke the wake function at `timeout`, an absolute `Date.now()` timestamp. */
	export function schedule(timeout: number): void;
}
