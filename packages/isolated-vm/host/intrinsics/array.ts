/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import { makeFreeFunction } from "@isolated-vm/experimental/host/intrinsics/function";
import { ObjectDefineProperty } from "@isolated-vm/experimental/host/intrinsics/object";

/** @internal */ export const ArrayPrototype = Array.prototype;

// These only touch own properties of a dense array, so the natives never reach the prototype chain
/** @internal */ export const ArrayIncludes: (thisArg: readonly unknown[], searchElement: unknown, fromIndex?: number) => boolean = makeFreeFunction(Array.prototype.includes);
/** @internal */ export const ArrayIndexOf = makeFreeFunction(Array.prototype.indexOf);
/** @internal */ export const ArrayShift: <Type>(thisArg: Type[]) => Type | undefined = makeFreeFunction(Array.prototype.shift);

/** Assigns `array[index]`, which may create the index. */
let setElement: <Type>(array: Type[], index: number, value: Type) => void = (array, index, value) => {
	array[index] = value;
};

function safeSetElement<Type>(array: Type[], index: number, value: Type): void {
	const descriptor = { __proto__: null, value, writable: true, enumerable: true, configurable: true };
	ObjectDefineProperty(array, index, descriptor);
}

/** @internal */
export let ArrayPush: <Type>(array: Type[], ...items: Type[]) => number = makeFreeFunction(Array.prototype.push);

function ArraySafePush<Type>(array: Type[], ...items: Type[]): number {
	for (let ii = 0; ii < items.length; ++ii) {
		setElement(array, array.length, items[ii]);
	}
	return array.length;
}

/** @internal */
export function applySafeArray(): void {
	ArrayPush = ArraySafePush;
	setElement = safeSetElement;
}

// TODO: `constructor` and `Symbol.species` lookups
/** @internal */ export const ArrayMap: <Type, Result>(thisArg: readonly Type[], callback: (value: Type, index: number) => Result) => Result[] = makeFreeFunction(Array.prototype.map);
/** @internal */ export const ArraySlice: <Type>(thisArg: Type[], start?: number, end?: number) => Type[] = makeFreeFunction(Array.prototype.slice);
/** @internal */ export const ArraySplice = makeFreeFunction(Array.prototype.splice);
