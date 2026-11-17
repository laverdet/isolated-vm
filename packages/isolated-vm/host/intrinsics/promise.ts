/* eslint-disable @typescript-eslint/unbound-method */
import { ArrayMap } from "@isolated-vm/experimental/host/intrinsics/array";
import { makeFreeFunction } from "@isolated-vm/experimental/host/intrinsics/function";

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
		// Dense from the start, so each result lands on an existing index
		const results = ArrayMap(promises, (): Type | undefined => undefined);
		let remaining = promises.length;
		if (remaining === 0) {
			resolve(results as Type[]);
		}
		for (let ii = 0; ii < promises.length; ++ii) {
			void PromiseThen(promises[ii]!, result => {
				results[ii] = result;
				if (--remaining === 0) {
					resolve(results as Type[]);
				}
			}, reject);
		}
	});
}
