/**
 * Type declarations `realm.instantiateRuntime()` which can be linked in the runtime.
 */

/**
 * Runtime-side `Reference`. It is transferrable to the host but otherwise has no methods.
 */
export declare class Reference<Type = unknown> {
	// eslint-disable-next-line no-unused-private-class-members
	readonly #value: Type;
	constructor(value: Type);
}

/**
 * Runtime support for promise-tracking context data
 */
export declare class Zone {
	// eslint-disable-next-line no-unused-private-class-members
	readonly #private;
	constructor();

	/** The zone which is currently running, if any. */
	static get current(): Zone | undefined;

	/** Invokes `callback` with this zone current, and returns the result. */
	run<Result>(callback: () => Result): Result;
}

/** System clock time in milliseconds. */
export declare function clockTime(): number;

/** Monotonic time in milliseconds. */
export declare function performanceTime(): number;

/**
 * Marks a value for transfer, along with an optional transfer list. The result can be returned by a
 * function which was invoked by `reference.invoke()`.
 */
export declare function transfer(value: unknown, transferList?: readonly unknown[]): unknown;
