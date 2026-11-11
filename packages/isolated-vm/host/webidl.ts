// https://webidl.spec.whatwg.org/
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import type { Constructor } from "@isolated-vm/experimental/utility/object";
import { ObjectDefineProperty, ObjectGetOwnPropertyDescriptor, ReflectOwnKeys, SymbolToStringTag, TypeErrorConstructor } from "@isolated-vm/experimental/host/intrinsics";

/** @internal */
export type Dictionary = Record<string, unknown>;

// https://webidl.spec.whatwg.org/#es-dictionary
/** @internal */
export function assertDictionary(value: unknown): asserts value is Dictionary | null | undefined {
	if (value != null && typeof value !== "object" && typeof value !== "function") {
		throw new TypeErrorConstructor("Provided value is not of type 'dictionary'");
	}
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
 * Adds `toStringTag` and makes all properties enumerable.
 * @internal
 */
export function defineInterface<Type extends object>(constructor: Constructor<Type>): void {
	const { name, prototype } = constructor;
	const keys = ReflectOwnKeys(prototype);
	for (let ii = 0; ii < keys.length; ++ii) {
		const key = keys[ii]!;
		if (key !== "constructor") {
			const descriptor = ObjectGetOwnPropertyDescriptor(prototype, key)!;
			descriptor.enumerable = true;
			ObjectDefineProperty(prototype, key, descriptor);
		}
	}
	// https://webidl.spec.whatwg.org/#dfn-class-string
	ObjectDefineProperty(prototype, SymbolToStringTag, { value: name, writable: false, enumerable: false, configurable: true });
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
