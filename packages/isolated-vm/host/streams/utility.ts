// https://streams.spec.whatwg.org/#misc-abstract-ops
import { ArrayPush, ArrayShift, NumberIsNaN, PromiseReject, PromiseResolve, PromiseThen, RangeErrorConstructor } from "@isolated-vm/experimental/host/intrinsics";
import { defineInternal } from "@isolated-vm/experimental/host/webidl";

function noop() {}

// https://webidl.spec.whatwg.org/#mark-a-promise-as-handled
/** @internal */
export function markAsHandled(promise: Promise<unknown>): void {
	void PromiseThen(promise, undefined, noop);
}

// https://webidl.spec.whatwg.org/#js-operations (promise return types)
/** Promise-returning operations convert argument & brand check exceptions into rejections. */
/** @internal */
export function promiseTry<Type>(operation: () => Promise<Type>): Promise<Type> {
	try {
		return operation();
	} catch (error) {
		return PromiseReject(error);
	}
}

/** @internal */
export function resolvedPromiseCapability(): PromiseWithResolvers<void> {
	return { promise: PromiseResolve(undefined), resolve: noop, reject: noop };
}

/** A rejected promise with `[[PromiseIsHandled]]` already set. */
/** @internal */
export function rejectedPromiseCapability<Type>(reason: unknown): PromiseWithResolvers<Type> {
	const promise = PromiseReject<Type>(reason);
	markAsHandled(promise);
	return { promise, resolve: noop, reject: noop };
}

// https://streams.spec.whatwg.org/#queue-with-sizes
/** @internal */
export class QueueWithSizes<Type> {
	queue: { readonly value: Type; readonly size: number }[] = [];
	queueTotalSize = 0;

	static { defineInternal(this); }

	// https://streams.spec.whatwg.org/#enqueue-value-with-size
	enqueueValueWithSize(value: Type, size: number): void {
		if (typeof size !== "number" || NumberIsNaN(size) || size < 0 || size === Infinity) {
			throw new RangeErrorConstructor("Size must be a finite, non-negative number");
		}
		ArrayPush(this.queue, { value, size });
		this.queueTotalSize += size;
	}

	// https://streams.spec.whatwg.org/#dequeue-value
	dequeueValue(): Type {
		const { value, size } = ArrayShift(this.queue)!;
		this.queueTotalSize -= size;
		if (this.queueTotalSize < 0) {
			this.queueTotalSize = 0;
		}
		return value;
	}

	// https://streams.spec.whatwg.org/#peek-queue-value
	peekQueueValue(): Type {
		return this.queue[0]!.value;
	}

	// https://streams.spec.whatwg.org/#reset-queue
	resetQueue(): void {
		this.queue = [];
		this.queueTotalSize = 0;
	}
}
