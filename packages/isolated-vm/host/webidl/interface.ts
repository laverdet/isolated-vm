// https://webidl.spec.whatwg.org/#js-interfaces
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import type { Constructor } from "@isolated-vm/experimental/utility/object";
import { ArrayIncludes, ObjectDefineProperty, ObjectGetOwnPropertyDescriptor, ObjectSetPrototypeOf, ReflectOwnKeys, SymbolToStringTag } from "@isolated-vm/experimental/host/intrinsics";

/**
 * Severs the prototype chain of an internal class.
 * @internal
 */
export function defineInternal(constructor: Constructor<object>): void {
	ObjectSetPrototypeOf(constructor.prototype, null);
}

/**
 * Carries an internal value through a public constructor. Sandbox code cannot obtain this class, so a
 * wrapped argument proves the caller is internal.
 * @internal
 */
export class InternalParameter<Type> {
	readonly #value: Type;

	static { defineInternal(this); }

	constructor(value: Type) {
		this.#value = value;
	}

	/** Returns the wrapped value, or `undefined` for anything sandbox code could have passed. */
	static unwrap<Type>(value: unknown): Type | undefined {
		if (value !== null && typeof value === "object" && #value in value) {
			return (value as InternalParameter<unknown>).#value as Type;
		}
		return undefined;
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
		if (!ArrayIncludes(skip, key)) {
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
