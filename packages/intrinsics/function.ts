export const FunctionConstructor = Function;

// eslint-disable-next-line @typescript-eslint/unbound-method
export const FunctionCall = Function.prototype.call.bind(Function.prototype.call) as <This, Result, Args extends unknown[]>(
	fn: (this: This, ...args: Args) => Result,
	thisArg: This,
	...args: Args
) => Result;
