import { makeFreeFunction } from "@isolated-vm/intrinsics/utility";

export const ArrayPrototype = Array.prototype;

export const ArrayIncludes: (thisArg: readonly unknown[], searchElement: unknown, fromIndex?: number) => boolean = makeFreeFunction(Array.prototype.includes);
export const ArrayIndexOf = makeFreeFunction(Array.prototype.indexOf);
export const ArrayMap: <Type, Result>(thisArg: readonly Type[], callback: (value: Type, index: number) => Result) => Result[] = makeFreeFunction(Array.prototype.map);
export const ArrayPush = makeFreeFunction(Array.prototype.push);
export const ArrayShift: <Type>(thisArg: Type[]) => Type | undefined = makeFreeFunction(Array.prototype.shift);
export const ArraySlice: <Type>(thisArg: Type[], start?: number, end?: number) => Type[] = makeFreeFunction(Array.prototype.slice);
export const ArraySort: <Type>(thisArg: Type[], compare: (left: Type, right: Type) => number) => Type[] = makeFreeFunction(Array.prototype.sort);
export const ArraySplice = makeFreeFunction(Array.prototype.splice);
