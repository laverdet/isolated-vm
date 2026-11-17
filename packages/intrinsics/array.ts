/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import { ObjectDefineProperty } from "@isolated-vm/intrinsics/object";
import { makeFreeFunction } from "@isolated-vm/intrinsics/utility";

/** @internal */
export function applySafeArray(): void {
	ArrayPush = ArraySafePush;
	set = safeSet;
}

export const ArrayPrototype = Array.prototype;

// These only touch own properties of a dense array, so the natives never reach the prototype chain
export const ArrayIncludes: (thisArg: readonly unknown[], searchElement: unknown, fromIndex?: number) => boolean = makeFreeFunction(Array.prototype.includes);
export const ArrayIndexOf = makeFreeFunction(Array.prototype.indexOf);
export const ArrayShift: <Type>(thisArg: Type[]) => Type | undefined = makeFreeFunction(Array.prototype.shift);

let set: <Type>(array: Type[], index: number, value: Type) => void = (array, index, value) => {
	array[index] = value;
};

function safeSet<Type>(array: Type[], index: number, value: Type): void {
	const descriptor = { __proto__: null, value, writable: true, enumerable: true, configurable: true };
	ObjectDefineProperty(array, index, descriptor);
}

// Functions with "safe" alternate implementations
export let ArrayPush: <Type>(array: Type[], ...items: Type[]) => number = makeFreeFunction(Array.prototype.push);

function ArraySafePush<Type>(array: Type[], ...items: Type[]): number {
	for (let ii = 0; ii < items.length; ++ii) {
		set(array, array.length, items[ii]);
	}
	return array.length;
}

// TODO: `constructor` and `Symbol.species` lookups
export const ArrayMap: <Type, Result>(thisArg: readonly Type[], callback: (value: Type, index: number) => Result) => Result[] = makeFreeFunction(Array.prototype.map);
export const ArraySlice: <Type>(thisArg: Type[], start?: number, end?: number) => Type[] = makeFreeFunction(Array.prototype.slice);
export const ArraySplice = makeFreeFunction(Array.prototype.splice);
