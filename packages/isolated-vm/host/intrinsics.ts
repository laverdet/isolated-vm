/* eslint-disable @typescript-eslint/unbound-method */
const ReflectApply = Reflect.apply;

function makeFreeFunction<This, Result, Args extends unknown[]>(
	instanceMethod: (this: This, ...args: Args) => Result,
) {
	return (thisArg: This, ...args: Args) => ReflectApply(instanceMethod, thisArg, args);
}

/** @internal */ export const GlobalIsNaN = globalThis.isNaN;

/** @internal */ export const ArrayIndexOf = makeFreeFunction(Array.prototype.indexOf);
/** @internal */ export const ArrayPop = makeFreeFunction(Array.prototype.pop);
/** @internal */ export const ArrayPush = makeFreeFunction(Array.prototype.push);
/** @internal */ export const ArrayShift = makeFreeFunction(Array.prototype.shift);
/** @internal */ export const ArraySplice = makeFreeFunction(Array.prototype.splice);
/** @internal */ export const ArrayUnshift = makeFreeFunction(Array.prototype.unshift);

/** @internal */ export const DateNow = Date.now;

/** @internal */ export const FunctionConstructor = Function;

/** @internal */ export const NumberConstructor = Number;

/** @internal */ export const MapConstructor = Map;
/** @internal */ export const MapDelete: <K, V>(thisArg: Map<K, V>, key: K) => boolean = makeFreeFunction(Map.prototype.delete);
/** @internal */ export const MapGet: <K, V>(thisArg: Map<K, V>, key: K) => V | undefined = makeFreeFunction(Map.prototype.get);
/** @internal */ export const MapSet: <K, V>(thisArg: Map<K, V>, key: K, value: V) => Map<K, V> = makeFreeFunction(Map.prototype.set);

/** @internal */ export const StringConstructor = String;
/** @internal */ export const StringCharCodeAt = makeFreeFunction(String.prototype.charCodeAt);
/** @internal */ export const StringFromCharCode = String.fromCharCode;
