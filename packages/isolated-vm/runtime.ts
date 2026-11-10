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

export interface ZoneOptions {
	/**
	 * Execution time budget in milliseconds. Time counts only while the zone is current, and execution
	 * is terminated when the budget is spent. `undefined` or `Infinity` means no budget.
	 */
	timeout?: number | undefined;
}

/**
 * Runtime support for promise-tracking context data and timeout budget.
 */
export declare class Zone {
	// eslint-disable-next-line no-unused-private-class-members
	readonly #private;
	constructor(options?: ZoneOptions);

	/** The zone which is currently running. */
	static get current(): Zone | undefined;

	/** Schedules `callback` in a new unrelated zone. */
	static detach(callback: () => void, options?: ZoneOptions): void;

	/** Arbitrary data attached to this zone. */
	get data(): Map<unknown, unknown>;

	/** Remaining time budget in milliseconds, or `undefined` if the zone has no budget. */
	get timeout(): number | undefined;

	/** Adds `delta` milliseconds, which may be negative, to the remaining time budget. */
	adjustTimeout(delta: number): boolean;

	/** Replaces the remaining time budget with `timeout` milliseconds. */
	applyTimeout(timeout: number): boolean;

	/** Creates a zone with a snapshot of `data`, sharing the time budget of this zone. */
	clone(): Zone;

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
