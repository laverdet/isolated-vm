export interface Constructor<Type = object> {
	readonly name: string;
	readonly prototype: Type;
}

type MakeFreeFunction = <This, Result, Args extends unknown[]>(
	instanceMethod: (this: This, ...args: Args) => Result,
) => (thisArg: This, ...args: Args) => Result;

/**
 * Converts a method into a function which takes its receiver as the first argument.
 * @internal
 */
// eslint-disable-next-line @typescript-eslint/unbound-method -- `call` is the receiver being bound
export const makeFreeFunction = Function.prototype.bind.bind(Function.prototype.call) as MakeFreeFunction;
