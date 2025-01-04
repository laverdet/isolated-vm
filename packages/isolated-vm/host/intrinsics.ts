/* eslint-disable @typescript-eslint/unbound-method */
const ReflectApply = Reflect.apply;

function makeFreeFunction<This, Result, Args extends unknown[]>(
	instanceMethod: (this: This, ...args: Args) => Result,
) {
	return (thisArg: This, ...args: Args) => ReflectApply(instanceMethod, thisArg, args);
}

/** @internal */
export const isNaN = globalThis.isNaN;

/** @internal */
export const StringConstructor = String;
/** @internal */
export const StringFromCharCode = String.fromCharCode;
/** @internal */
export const StringPrototypeCharCodeAt = makeFreeFunction(String.prototype.charCodeAt);
