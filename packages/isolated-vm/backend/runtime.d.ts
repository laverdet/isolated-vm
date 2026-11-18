// Type declarations `realm.instantiateRuntime()` which can be linked in the runtime.
declare module "isolated-vm:runtime" {
	export type BufferSource = ArrayBufferLike | ArrayBufferView<ArrayBufferLike>;

	/**
	 * Runtime-side `Reference`. It is transferrable to the host but otherwise has no methods.
	 */
	export class Reference<Type = unknown> {
		readonly #value: Type;
		constructor(value: Type);
	}

	/** Streaming decoder behind `TextDecoder` */
	export class StringReader {
		readonly #private;
		constructor(label: string, fatal: boolean, ignoreBOM: boolean);

		/** Resolved encoding name */
		get encoding(): string;

		/**
		 * Decodes `input`, holding incomplete sequences unless `flush` is set. `undefined` means invalid input in fatal
		 * mode.
		 */
		read(input: BufferSource, flush: boolean): string | undefined;
	}

	export interface StringWriterResult {
		/** Code points consumed */
		read: number;
		/** Bytes written */
		written: number;
		destination: Uint8Array;
	}

	/** Streaming UTF-8 encoder behind `TextEncoder` */
	export class StringWriter {
		readonly #private;
		constructor();

		/**
		 * Encodes `input` into `destination`, or a new `Uint8Array`. Without `flush`, a trailing lead surrogate is held.
		 */
		write(input: string, flush: boolean, destination?: Uint8Array): StringWriterResult;
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
	export class Zone {
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
	export function clockTime(): number;

	/** Enqueues `callback` as a zone-respecting microtask. */
	export function enqueueMicrotask(callback: () => void): void;

	/** Fetch settled state and value. */
	export function getPromiseInternal<Type>(promise: Promise<Type>): [ true, Type ] | [ false, unknown ] | undefined;

	/** Monotonic time in milliseconds. */
	export function performanceTime(): number;

	/**
	 * Marks a value for transfer, along with an optional transfer list. The result can be returned by a
	 * function which was invoked by `reference.invoke()`.
	 */
	export function transfer(value: unknown, transferList?: readonly unknown[]): unknown;
}
