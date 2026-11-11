import type { AbortSignal } from "@isolated-vm/experimental/host/dom/abort";

interface AbortSignalBrand {
	isAbortSignal: (value: unknown) => value is AbortSignal;
	isAborted: (signal: AbortSignal) => boolean;
	// https://dom.spec.whatwg.org/#abortsignal-add
	addAbortAlgorithm: (signal: AbortSignal, algorithm: () => void) => void;
}

/** @internal */ export let isAbortSignal: AbortSignalBrand["isAbortSignal"] = (_value: unknown): _value is AbortSignal => false;
/** @internal */ export let isAborted: AbortSignalBrand["isAborted"];
/** @internal */ export let addAbortAlgorithm: AbortSignalBrand["addAbortAlgorithm"];

/** @internal */
export function defineAbortSignalBrand(brand: AbortSignalBrand): void {
	({ isAbortSignal, isAborted, addAbortAlgorithm } = brand);
}
