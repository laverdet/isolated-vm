// https://webidl.spec.whatwg.org/#js-interfaces
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import type { Constructor } from "@isolated-vm/intrinsics/utility";
import { ArrayIncludes, ObjectDefineProperties, ObjectDefineProperty, ObjectGetOwnPropertyDescriptor, ObjectSetPrototypeOf, ReflectOwnKeys, SymbolToStringTag } from "@isolated-vm/intrinsics";

/**
 * Severs the prototype chain of an internal class.
 * @internal
 */
export function defineInternal(constructor: Constructor): void {
	ObjectSetPrototypeOf(constructor.prototype, null);
}

/**
 * Carries an internal value through a public constructor. Sandbox code cannot obtain this class, so a
 * wrapped argument proves the caller is internal.
 * @internal
 */
export class InternalParameter<Type> {
	readonly #value: Type;

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

/**
 * Adds `toStringTag` and makes all regular and static members enumerable.
 *
 * `properties` are defined on the prototype afterwards, for the odd ones out like `@@iterator` or a
 * class string which isn't the constructor's name.
 * @internal
*/
// https://webidl.spec.whatwg.org/#interface-prototype-object
export function defineInterface(constructor: Constructor, properties: Record<PropertyKey, PropertyDescriptor> = {}): void {
	const { name, prototype } = constructor;
	makeEnumerable(prototype, "constructor");
	makeEnumerable(constructor, "length", "name", "prototype");
	// https://webidl.spec.whatwg.org/#dfn-class-string
	ObjectDefineProperty(prototype, SymbolToStringTag, { value: name, configurable: true });
	ObjectDefineProperties(prototype, properties);
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

/**
 * Freezes constants onto both the interface object and its prototype.
 * @internal
*/
// https://webidl.spec.whatwg.org/#define-the-constants
export function defineConstants(constructor: Constructor, constants: Record<string, number>): void {
	const { prototype } = constructor;
	const keys = ReflectOwnKeys(constants) as string[];
	for (let ii = 0; ii < keys.length; ++ii) {
		const key = keys[ii]!;
		const descriptor = { value: constants[key], writable: false, enumerable: true, configurable: false };
		ObjectDefineProperty(constructor, key, descriptor);
		ObjectDefineProperty(prototype, key, descriptor);
	}
}

/** @internal */
// https://webidl.spec.whatwg.org/#define-the-global-property-references
export function exposeInterface(constructor: Constructor): void {
	ObjectDefineProperty(globalThis, constructor.name, { value: constructor, writable: true, enumerable: false, configurable: true });
}
