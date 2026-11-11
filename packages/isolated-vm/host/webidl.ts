// https://webidl.spec.whatwg.org/
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import type { Constructor } from "@isolated-vm/experimental/utility/object";
import { MathTrunc, NumberIsFinite, Number_MAX_SAFE_INTEGER, ObjectDefineProperty, ObjectGetOwnPropertyDescriptor, ReflectOwnKeys, SymbolToStringTag, TypeErrorConstructor } from "@isolated-vm/experimental/host/intrinsics";

/** @internal */
export type Dictionary = Record<string, unknown>;

// https://webidl.spec.whatwg.org/#es-dictionary
/** @internal */
export function assertDictionary(value: unknown): asserts value is Dictionary | null | undefined {
	if (value != null && typeof value !== "object" && typeof value !== "function") {
		throw new TypeErrorConstructor("Provided value is not of type 'dictionary'");
	}
}

// https://webidl.spec.whatwg.org/#abstract-opdef-converttoint
/** `[EnforceRange] unsigned long long`: a finite integer in `[0, 2^53 - 1]`, or a TypeError. */
/** @internal */
export function convertEnforceRangeUnsignedLongLong(value: unknown): number {
	// `Math.trunc` applies `ToNumber`, which rejects BigInt and Symbol where `Number()` would not
	const number = MathTrunc(value as number);
	if (!NumberIsFinite(number) || number < 0 || number > Number_MAX_SAFE_INTEGER) {
		throw new TypeErrorConstructor("Provided value is outside the range of 'unsigned long long'");
	}
	return number || 0;
}

// https://webidl.spec.whatwg.org/#dfn-overload-resolution-algorithm (step 4)
/** @internal */
export function requireArguments(count: number, required: number): void {
	if (count < required) {
		throw new TypeErrorConstructor(`${required} argument(s) required, but only ${count} present`);
	}
}

// https://webidl.spec.whatwg.org/#interface-prototype-object
/**
 * Adds `toStringTag` and makes all regular and static members enumerable.
 * @internal
 */
export function defineInterface<Type extends object>(constructor: Constructor<Type>): void {
	const { name, prototype } = constructor;
	makeEnumerable(prototype, "constructor");
	makeEnumerable(constructor, "length", "name", "prototype");
	// https://webidl.spec.whatwg.org/#dfn-class-string
	ObjectDefineProperty(prototype, SymbolToStringTag, { value: name, writable: false, enumerable: false, configurable: true });
}

function makeEnumerable(object: object, ...skip: string[]) {
	const keys = ReflectOwnKeys(object);
	for (let ii = 0; ii < keys.length; ++ii) {
		const key = keys[ii]!;
		let skipped = false;
		for (let jj = 0; jj < skip.length; ++jj) {
			skipped ||= key === skip[jj];
		}
		if (!skipped) {
			const descriptor = ObjectGetOwnPropertyDescriptor(object, key)!;
			descriptor.enumerable = true;
			ObjectDefineProperty(object, key, descriptor);
		}
	}
}

// https://webidl.spec.whatwg.org/#define-the-constants
/**
 * Freezes constants onto both the interface object and its prototype.
 * @internal
 */
export function defineConstants<Type extends object>(constructor: Constructor<Type>, constants: Record<string, number>): void {
	const { prototype } = constructor;
	const keys = ReflectOwnKeys(constants) as string[];
	for (let ii = 0; ii < keys.length; ++ii) {
		const key = keys[ii]!;
		const descriptor = { value: constants[key], writable: false, enumerable: true, configurable: false };
		ObjectDefineProperty(constructor, key, descriptor);
		ObjectDefineProperty(prototype, key, descriptor);
	}
}

// https://webidl.spec.whatwg.org/#define-the-global-property-references
/** @internal */
export function exposeInterface(constructor: Constructor<object>): void {
	ObjectDefineProperty(globalThis, constructor.name, { value: constructor, writable: true, enumerable: false, configurable: true });
}
