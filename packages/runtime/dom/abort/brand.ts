import type { AbortSignal } from "@isolated-vm/runtime/dom/abort";

interface AbortSignalBrand {
	isAbortSignal: (value: unknown) => value is AbortSignal;
	isAborted: (signal: AbortSignal) => boolean;
	getAbortReason: (signal: AbortSignal) => unknown;
	// https://dom.spec.whatwg.org/#abortsignal-add
	addAbortAlgorithm: (signal: AbortSignal, algorithm: () => void) => void;
	// https://dom.spec.whatwg.org/#abortsignal-remove
	removeAbortAlgorithm: (signal: AbortSignal, algorithm: () => void) => void;
}

/** @internal */ export let isAbortSignal: AbortSignalBrand["isAbortSignal"] = (_value: unknown): _value is AbortSignal => false;
/** @internal */ export let isAborted: AbortSignalBrand["isAborted"];
/** @internal */ export let getAbortReason: AbortSignalBrand["getAbortReason"];
/** @internal */ export let addAbortAlgorithm: AbortSignalBrand["addAbortAlgorithm"];
/** @internal */ export let removeAbortAlgorithm: AbortSignalBrand["removeAbortAlgorithm"];

/** @internal */
export function defineAbortSignalBrand(brand: AbortSignalBrand): void {
	({ isAbortSignal, isAborted, getAbortReason, addAbortAlgorithm, removeAbortAlgorithm } = brand);
}
