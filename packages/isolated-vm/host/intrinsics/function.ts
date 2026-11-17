import { ReflectApply } from "@isolated-vm/experimental/host/intrinsics/reflect";

/** @internal */ export const FunctionConstructor = Function;

/**
 * Converts a method into a function which takes its receiver as the first argument.
 * @internal
 */
export function makeFreeFunction<This, Result, Args extends unknown[]>(
	instanceMethod: (this: This, ...args: Args) => Result,
): (thisArg: This, ...args: Args) => Result {
	return (thisArg, ...args) => ReflectApply(instanceMethod, thisArg, args);
}
