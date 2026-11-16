// https://webidl.spec.whatwg.org/#js-type-mapping
import { MathTrunc, NumberIsFinite, Number_MAX_SAFE_INTEGER, TypeErrorConstructor } from "@isolated-vm/experimental/host/intrinsics";

/** @internal */
export type Dictionary = Record<string, unknown>;

/** @internal */
export type CallbackFunction = (...args: never[]) => unknown;

// https://webidl.spec.whatwg.org/#dfn-overload-resolution-algorithm (step 4)
/** @internal */
export function requireArguments(count: number, required: number): void {
	if (count < required) {
		throw new TypeErrorConstructor(`${required} argument(s) required, but only ${count} present`);
	}
}

// https://webidl.spec.whatwg.org/#es-dictionary
/** @internal */
export function assertDictionary(value: unknown): asserts value is Dictionary | null | undefined {
	if (value != null && typeof value !== "object" && typeof value !== "function") {
		throw new TypeErrorConstructor("Provided value is not of type 'dictionary'");
	}
}

// https://webidl.spec.whatwg.org/#es-object
/** @internal */
export function assertObject(value: unknown): asserts value is object {
	if (value == null || (typeof value !== "object" && typeof value !== "function")) {
		throw new TypeErrorConstructor("Provided value is not of type 'object'");
	}
}

// https://webidl.spec.whatwg.org/#es-callback-function
/** An optional dictionary member, so `undefined` passes. */
/** @internal */
export function assertCallbackFunction(value: unknown): asserts value is CallbackFunction | undefined {
	if (value !== undefined && typeof value !== "function") {
		throw new TypeErrorConstructor("Provided value is not a function");
	}
}

// https://webidl.spec.whatwg.org/#abstract-opdef-converttoint
/** `[EnforceRange] unsigned long long`: a finite integer in `[0, 2^53 - 1]`, or a TypeError. */
/** @internal */
export function coerceEnforceRangeUnsignedLongLong(value: unknown): number {
	// `Math.trunc` applies `ToNumber`, which rejects BigInt and Symbol where `Number()` would not
	const number = MathTrunc(value as number);
	if (!NumberIsFinite(number) || number < 0 || number > Number_MAX_SAFE_INTEGER) {
		throw new TypeErrorConstructor("Provided value is outside the range of 'unsigned long long'");
	}
	return number || 0;
}
