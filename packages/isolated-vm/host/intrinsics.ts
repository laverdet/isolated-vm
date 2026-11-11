/* eslint-disable @typescript-eslint/unbound-method */
/** @internal */ export const ReflectApply = Reflect.apply;
/** @internal */ export const ReflectOwnKeys = Reflect.ownKeys;

function makeFreeFunction<This, Result, Args extends unknown[]>(
	instanceMethod: (this: This, ...args: Args) => Result,
) {
	return (thisArg: This, ...args: Args) => ReflectApply(instanceMethod, thisArg, args);
}

/** @internal */ export const GlobalIsNaN = isNaN;

/** @internal */ export const ArrayIndexOf = makeFreeFunction(Array.prototype.indexOf);
/** @internal */ export const ArrayPop = makeFreeFunction(Array.prototype.pop);
/** @internal */ export const ArrayPush = makeFreeFunction(Array.prototype.push);
/** @internal */ export const ArrayShift = makeFreeFunction(Array.prototype.shift);
/** @internal */ export const ArraySlice: <Type>(thisArg: Type[], start?: number, end?: number) => Type[] = makeFreeFunction(Array.prototype.slice);
/** @internal */ export const ArraySplice = makeFreeFunction(Array.prototype.splice);
/** @internal */ export const ArrayUnshift = makeFreeFunction(Array.prototype.unshift);

/** @internal */ export const BooleanConstructor = Boolean;

/** @internal */ export const ConsoleError = console.error;

/** @internal */ export const DateNow = Date.now;

/** @internal */ export const FunctionConstructor = Function;

/** @internal */ export const NumberConstructor = Number;
/** @internal */ export const { MAX_SAFE_INTEGER: Number_MAX_SAFE_INTEGER } = Number;

/** @internal */ export const MapConstructor = Map;
/** @internal */ export const MapDelete: <K, V>(thisArg: Map<K, V>, key: K) => boolean = makeFreeFunction(Map.prototype.delete);
/** @internal */ export const MapGet: <K, V>(thisArg: Map<K, V>, key: K) => V | undefined = makeFreeFunction(Map.prototype.get);
/** @internal */ export const MapSet: <K, V>(thisArg: Map<K, V>, key: K, value: V) => Map<K, V> = makeFreeFunction(Map.prototype.set);

/** @internal */ export const ObjectDefineProperty = Object.defineProperty;
/** @internal */ export const ObjectGetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;

/** @internal */ export const StringConstructor = String;
/** @internal */ export const StringCharCodeAt = makeFreeFunction(String.prototype.charCodeAt);
/** @internal */ export const StringFromCharCode = String.fromCharCode;

/** @internal */ export const SymbolToStringTag = Symbol.toStringTag;

/** @internal */ export const TypeErrorConstructor = TypeError;
