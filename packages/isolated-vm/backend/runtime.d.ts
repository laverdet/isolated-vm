// Type declarations `realm.instantiateRuntime()` which can be linked in the runtime.
declare module "isolated-vm:runtime" {
	export type BufferSource = ArrayBuffer | ArrayBufferView<ArrayBuffer>;
	export type AllowSharedBufferSource = ArrayBufferLike | ArrayBufferView<ArrayBufferLike>;

	export interface CodecResult {
		/** Whatever output is ready, in order */
		output: ArrayBuffer[];
		/**
		 * Count of input bytes consumed. A count short of the input means the write stopped at an error, and writing
		 * the remaining bytes throws it.
		 */
		bytesRead: number;
	}

	/** Streaming compressor behind `CompressionStream` */
	export class Compressor {
		readonly #private;
		constructor(format: string);

		write(input: BufferSource): CodecResult;
		flush(): ArrayBuffer[];
	}

	/** Streaming decompressor behind `DecompressionStream` */
	export class Decompressor {
		readonly #private;
		constructor(format: string);

		write(input: BufferSource): CodecResult;
		flush(): ArrayBuffer[];
	}

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
		read(input: AllowSharedBufferSource, flush: boolean): string | undefined;
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

	/** Result of `parseURL`. Contains offsets into `href` of the relevant parts. */
	export interface URLRecord {
		/** Serialized URL, or `undefined` when it is identical to the parsed input */
		href: string | undefined;
		protocolEnd: number;
		usernameEnd: number;
		hostStart: number;
		hostEnd: number;
		/** Port number, not an offset, or `undefined` when there is none */
		port: number | undefined;
		pathnameStart: number;
		/** Start of the `?`, or `hashStart` when there is no query */
		searchStart: number;
		/** Start of the `#`, or the length of `href` when there is no fragment */
		hashStart: number;
	}

	/**
	 * Parameter to `updateURL`: `protocol`, `username`, `password`, `host`, `hostname`, `port`, `pathname`
	 */
	export type URLComponent = 0 | 1 | 2 | 3 | 4 | 5 | 6;

	/** System clock time in milliseconds. */
	export function clockTime(): number;

	/** Enqueues `callback` as a zone-respecting microtask. */
	export function enqueueMicrotask(callback: () => void): void;

	/** Fetch settled state and value. */
	export function getPromiseInternal<Type>(promise: Promise<Type>): [ true, Type ] | [ false, unknown ] | undefined;

	/** Runs the basic URL parser on `input` against `base`. `undefined` is failure. */
	export function parseURL(input: string, base?: string): URLRecord | undefined;

	/** Monotonic time in milliseconds. */
	export function performanceTime(): number;

	/**
	 * Marks a value for transfer, along with an optional transfer list. The result can be returned by a
	 * function which was invoked by `reference.invoke()`.
	 */
	export function transfer(value: unknown, transferList?: readonly unknown[]): unknown;

	/** Raises a fatal v8 error. */
	export function triggerFatalForTesting(): never;

	/**
	 * Applies the setter for `component` to the URL serialized as `href`. `undefined` means the setter
	 * rejected `value`, leaving the URL unchanged.
	 */
	export function updateURL(href: string, component: URLComponent, value: string): URLRecord | undefined;
}
