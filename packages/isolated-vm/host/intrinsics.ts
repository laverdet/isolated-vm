/* eslint-disable @typescript-eslint/unbound-method */
/** @internal */ export const ReflectApply = Reflect.apply;
/** @internal */ export const ReflectDeleteProperty = Reflect.deleteProperty;
/** @internal */ export const ReflectOwnKeys = Reflect.ownKeys;

function makeFreeFunction<This, Result, Args extends unknown[]>(
	instanceMethod: (this: This, ...args: Args) => Result,
) {
	return (thisArg: This, ...args: Args) => ReflectApply(instanceMethod, thisArg, args);
}

/** @internal */ export const GlobalIsNaN = isNaN;

/** @internal */ export const ArrayIncludes: (thisArg: readonly unknown[], searchElement: unknown, fromIndex?: number) => boolean = makeFreeFunction(Array.prototype.includes);
/** @internal */ export const ArrayIndexOf = makeFreeFunction(Array.prototype.indexOf);
/** @internal */ export const ArrayPop = makeFreeFunction(Array.prototype.pop);
/** @internal */ export const ArrayPush = makeFreeFunction(Array.prototype.push);
/** @internal */ export const ArrayShift: <Type>(thisArg: Type[]) => Type | undefined = makeFreeFunction(Array.prototype.shift);
/** @internal */ export const ArraySlice: <Type>(thisArg: Type[], start?: number, end?: number) => Type[] = makeFreeFunction(Array.prototype.slice);
/** @internal */ export const ArraySplice = makeFreeFunction(Array.prototype.splice);
/** @internal */ export const ArrayUnshift = makeFreeFunction(Array.prototype.unshift);

// %AsyncIteratorPrototype% is only reachable through an async generator
const AsyncGeneratorFunctionPrototype = Object.getPrototypeOf(async function*() {}) as { prototype: object };
/** @internal */ export const AsyncIteratorPrototype = Object.getPrototypeOf(AsyncGeneratorFunctionPrototype.prototype) as object;

/** @internal */ export const BooleanConstructor = Boolean;

/** @internal */ export const ConsoleError = console.error;

/** @internal */ export const DateNow = Date.now;

/** @internal */ export const FunctionConstructor = Function;

/** @internal */ export const MapConstructor = Map;
/** @internal */ export const MapDelete: <K, V>(thisArg: Map<K, V>, key: K) => boolean = makeFreeFunction(Map.prototype.delete);
/** @internal */ export const MapGet: <K, V>(thisArg: Map<K, V>, key: K) => V | undefined = makeFreeFunction(Map.prototype.get);
/** @internal */ export const MapSet: <K, V>(thisArg: Map<K, V>, key: K, value: V) => Map<K, V> = makeFreeFunction(Map.prototype.set);

/** @internal */ export const MathTrunc = Math.trunc;

/** @internal */ export const NumberConstructor = Number;
/** @internal */ export const NumberIsFinite = Number.isFinite;
/** @internal */ export const NumberIsNaN = Number.isNaN;
/** @internal */ export const { MAX_SAFE_INTEGER: Number_MAX_SAFE_INTEGER } = Number;

/** @internal */ export const ObjectDefineProperty = Object.defineProperty;
/** @internal */ export const ObjectFreeze = Object.freeze;
/** @internal */ export const ObjectGetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
/** @internal */ export const ObjectGetPrototypeOf = Object.getPrototypeOf;
/** @internal */ export const ObjectSetPrototypeOf = Object.setPrototypeOf;

/** @internal */ export const PromiseConstructor = Promise;
/** @internal */ export const PromiseReject: <Type = never>(reason: unknown) => Promise<Type> = Promise.reject.bind(Promise);
/** @internal */ export const PromiseResolve: <Type>(value: Type | PromiseLike<Type>) => Promise<Awaited<Type>> = Promise.resolve.bind(Promise);
/** @internal */ export const PromiseWithResolvers: <Type>() => PromiseWithResolvers<Type> = Promise.withResolvers.bind(Promise);
/** @internal */ export const PromiseThen: <Type, Fulfilled = Type, Rejected = never>(
	thisArg: Promise<Type>,
	onFulfilled?: (value: Type) => Fulfilled | PromiseLike<Fulfilled>,
	onRejected?: (reason: unknown) => Rejected | PromiseLike<Rejected>,
) => Promise<Fulfilled | Rejected> = makeFreeFunction(Promise.prototype.then);
/** `Promise.all` looks up `Promise.resolve`, the array iterator, and `then`, so it is hand-rolled. */
/** @internal */ export function PromiseAll<Type>(promises: readonly Promise<Type>[]): Promise<Type[]> {
	return new PromiseConstructor<Type[]>((resolve, reject) => {
		const results: Type[] = [];
		let remaining = promises.length;
		if (remaining === 0) {
			resolve(results);
		}
		for (let ii = 0; ii < promises.length; ++ii) {
			void PromiseThen(promises[ii]!, result => {
				results[ii] = result;
				if (--remaining === 0) {
					resolve(results);
				}
			}, reject);
		}
	});
}

/** @internal */ export const RangeErrorConstructor = RangeError;

/** @internal */ export const StringConstructor = String;
/** @internal */ export const StringCharCodeAt = makeFreeFunction(String.prototype.charCodeAt);
/** @internal */ export const StringFromCharCode = String.fromCharCode;

/** @internal */ export const SymbolAsyncIterator = Symbol.asyncIterator;
/** @internal */ export const SymbolIterator = Symbol.iterator;
/** @internal */ export const SymbolToStringTag = Symbol.toStringTag;

/** @internal */ export const TypeErrorConstructor = TypeError;

/** @internal */ export const WeakRefConstructor = WeakRef;
/** @internal */ export const WeakRefDeref: <Type extends WeakKey>(thisArg: WeakRef<Type>) => Type | undefined = makeFreeFunction(WeakRef.prototype.deref);
