// https://streams.spec.whatwg.org/#rs-model
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
/* eslint-disable @typescript-eslint/no-base-to-string, @typescript-eslint/restrict-template-expressions -- `${value}` is `ToString`, which is the DOMString conversion */
import type { AbortSignal } from "@isolated-vm/runtime/dom/abort";
import type { SizeAlgorithm } from "@isolated-vm/runtime/streams/queuing-strategy";
import type { WritableStreamInternal } from "@isolated-vm/runtime/streams/writable";
import type { Dictionary } from "@isolated-vm/runtime/webidl";
import { ArrayPush, ArrayShift, AsyncIteratorPrototype, BooleanConstructor, ObjectDefineProperty, ObjectSetPrototypeOf, PromiseAll, PromiseConstructor, PromiseResolve, PromiseWithResolvers, ReflectApply, ReflectDeleteProperty, SymbolAsyncIterator, SymbolIterator, SymbolToStringTag, TypeErrorConstructor, kEmptyObject } from "@isolated-vm/intrinsics";
import { addAbortAlgorithm, getAbortReason, isAbortSignal, isAborted, removeAbortAlgorithm } from "@isolated-vm/runtime/dom/abort/brand";
import { QueuingStrategy } from "@isolated-vm/runtime/streams/queuing-strategy";
import { QueueWithSizes, markAsHandled, promiseTry, rejectedPromiseCapability, resolvedPromiseCapability } from "@isolated-vm/runtime/streams/utility";
import { getWritableStreamInternal, isWritableStream } from "@isolated-vm/runtime/streams/writable";
import { InternalParameter, assertCallbackFunction, assertDictionary, assertObject, defineInterface, defineInternal, requireArguments } from "@isolated-vm/runtime/webidl";
import { enqueueMicrotask } from "isolated-vm:runtime";

type ReadableStreamState = "readable" | "closed" | "errored";
type StartAlgorithm = () => unknown;
type PullAlgorithm = () => Promise<unknown>;
type CancelAlgorithm = (reason: unknown) => Promise<unknown>;
type ControllerFactory = (stream: ReadableStreamInternal) => ReadableStreamDefaultControllerInternal;

// https://streams.spec.whatwg.org/#read-request
interface ReadRequest {
	readonly chunk: (chunk: unknown) => void;
	readonly close: () => void;
	readonly error: (reason: unknown) => void;
}

interface PipeOptions {
	readonly preventAbort: boolean;
	readonly preventCancel: boolean;
	readonly preventClose: boolean;
	readonly signal: AbortSignal | undefined;
}

// Every key is an own property so no lookup reaches `Object.prototype`
interface PipeShutdownFulfilled {
	readonly action: (() => Promise<unknown>) | undefined;
	readonly rejected: false;
}

interface PipeShutdownRejected {
	readonly action: (() => Promise<unknown>) | undefined;
	readonly rejected: true;
	readonly error: unknown;
}

type PipeShutdownRequest = PipeShutdownFulfilled | PipeShutdownRejected;

export interface ReadableStreamReadResult {
	value: unknown;
	done: boolean;
}

// Private state accessors, assigned from within the class bodies
let isReadableStream: (value: unknown) => value is ReadableStream;
/** @internal */ export let getReadableStreamInternal: (stream: ReadableStream) => ReadableStreamInternal;

const endOfIteration = Symbol("endOfIteration");

// https://streams.spec.whatwg.org/#create-readable-stream
/** @internal */
export function createReadableStream(
	startAlgorithm: StartAlgorithm,
	pullAlgorithm: PullAlgorithm,
	cancelAlgorithm: CancelAlgorithm,
	highWaterMark = 1,
	sizeAlgorithm: SizeAlgorithm = () => 1,
): ReadableStream {
	const controller: ControllerFactory = stream =>
		new ReadableStreamDefaultControllerInternal(stream, startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark, sizeAlgorithm);
	return new ReadableStream(new InternalParameter(controller));
}

/** @internal */
export class ReadableStreamInternal {
	readonly controller: ReadableStreamDefaultControllerInternal;
	state: ReadableStreamState = "readable";
	storedError: unknown = undefined;
	disturbed = false;
	reader: ReadableStreamDefaultReaderInternal | undefined = undefined;

	static { defineInternal(this); }

	constructor(controller: ControllerFactory) {
		this.controller = controller(this);
		this.controller.start();
	}

	// https://streams.spec.whatwg.org/#acquire-readable-stream-reader
	acquireDefaultReader(): ReadableStreamDefaultReaderInternal {
		if (this.reader !== undefined) {
			throw new TypeErrorConstructor("ReadableStream is locked");
		}
		return this.reader = new ReadableStreamDefaultReaderInternal(this);
	}

	// https://streams.spec.whatwg.org/#readable-stream-cancel
	async cancel(reason: unknown): Promise<void> {
		this.disturbed = true;
		const { state } = this;
		if (state === "closed") {
			return;
		} else if (state === "errored") {
			throw this.storedError;
		}
		this.close();
		const { controller } = this;
		controller.resetQueue();
		await controller.cancelAlgorithm(reason);
	}

	// https://streams.spec.whatwg.org/#readable-stream-close
	close(): void {
		this.state = "closed";
		const { reader } = this;
		if (reader === undefined) {
			return;
		}
		reader.closedPromise.resolve(undefined);
		const { readRequests } = reader;
		reader.readRequests = [];
		for (let ii = 0; ii < readRequests.length; ++ii) {
			readRequests[ii]!.close();
		}
	}

	// https://streams.spec.whatwg.org/#readable-stream-error
	error(error: unknown): void {
		this.state = "errored";
		this.storedError = error;
		const { reader } = this;
		if (reader === undefined) {
			return;
		}
		reader.closedPromise.reject(error);
		markAsHandled(reader.closedPromise.promise);
		reader.errorReadRequests(error);
	}

	// https://streams.spec.whatwg.org/#readable-stream-pipe-to
	async pipeTo(dest: WritableStreamInternal, options: PipeOptions): Promise<void> {
		const { preventAbort, preventCancel, preventClose, signal } = options;
		const reader = this.acquireDefaultReader();
		const writer = dest.acquireDefaultWriter();
		this.disturbed = true;
		let currentWrite: Promise<unknown> | undefined;

		// The first request wins; an `error` key means the pipe rejects once shut down
		let shutdownRequest: PipeShutdownRequest | undefined;
		const shutdown: PromiseWithResolvers<void> = PromiseWithResolvers();
		const requestShutdown = (request: PipeShutdownRequest) => {
			shutdownRequest ??= request;
			shutdown.resolve(undefined);
		};
		const abortAlgorithm = () => {
			const error = getAbortReason(signal!);
			requestShutdown({
				action: () => {
					const actions: Promise<unknown>[] = [];
					if (!preventAbort && dest.state === "writable") {
						ArrayPush(actions, dest.abort(error));
					}
					if (!preventCancel && this.state === "readable") {
						ArrayPush(actions, this.cancel(error));
					}
					return PromiseAll(actions);
				},
				rejected: true,
				error,
			});
		};
		if (signal !== undefined) {
			if (isAborted(signal)) {
				abortAlgorithm();
			} else {
				addAbortAlgorithm(signal, abortAlgorithm);
			}
		}

		const pipeStep = async (): Promise<boolean> => {
			if (shutdownRequest !== undefined) {
				return true;
			}
			await writer.readyPromise.promise;
			return new PromiseConstructor<boolean>((resolve, reject) => {
				reader.read({
					chunk: chunk => {
						// Writing synchronously from within `enqueue()` would re-enter the developer's sink
						enqueueMicrotask(() => {
							if (writer.stream !== undefined) {
								currentWrite = async function() {
									try {
										await writer.write(chunk);
									} catch {}
								}();
							}
						});
						resolve(false);
					},
					close: () => resolve(true),
					error: reject,
				});
			});
		};
		markAsHandled(async function() {
			while (!await pipeStep());
		}());

		// Errors must be propagated forward
		const onSourceErrored = (error: unknown) => {
			requestShutdown({ action: preventAbort ? undefined : () => dest.abort(error), rejected: true, error });
		};
		if (this.state === "errored") {
			onSourceErrored(this.storedError);
		} else {
			void async function() {
				try {
					await reader.closedPromise.promise;
				} catch (error) {
					onSourceErrored(error);
				}
			}();
		}
		// Errors must be propagated backward
		const onDestErrored = (error: unknown) => {
			requestShutdown({ action: preventCancel ? undefined : () => this.cancel(error), rejected: true, error });
		};
		if (dest.state === "errored") {
			onDestErrored(dest.storedError);
		} else {
			void async function() {
				try {
					await writer.closedPromise.promise;
				} catch (error) {
					onDestErrored(error);
				}
			}();
		}
		// Closing must be propagated forward
		const onSourceClosed = () => {
			requestShutdown({ action: preventClose ? undefined : () => writer.closeWithErrorPropagation(), rejected: false });
		};
		if (this.state === "closed") {
			onSourceClosed();
		} else {
			void async function() {
				await reader.closedPromise.promise;
				onSourceClosed();
			}();
		}
		// Closing must be propagated backward
		if (dest.closeQueuedOrInFlight || dest.state === "closed") {
			onDestErrored(new TypeErrorConstructor("Destination closed before piping finished"));
		}

		if (shutdownRequest === undefined) {
			await shutdown.promise;
		}
		const request = shutdownRequest!;
		if (dest.state === "writable" && !dest.closeQueuedOrInFlight) {
			let awaitedWrite: Promise<unknown> | undefined;
			do {
				awaitedWrite = currentWrite;
				await awaitedWrite;
			} while (awaitedWrite !== currentWrite);
		}
		try {
			if (request.action !== undefined) {
				await request.action();
			}
		} finally {
			writer.release();
			reader.release();
			if (signal !== undefined) {
				removeAbortAlgorithm(signal, abortAlgorithm);
			}
		}
		if (request.rejected) {
			throw request.error;
		}
	}

	// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
	tee(): ReadableStream[] {
		const reader = this.acquireDefaultReader();
		let reading = false;
		let readAgain = false;
		let canceled1 = false;
		let canceled2 = false;
		let reason1: unknown;
		let reason2: unknown;
		const cancelPromise: PromiseWithResolvers<void> = PromiseWithResolvers();
		// Enqueuing into a branch may re-enter `pull`
		const finishRead = () => {
			reading = false;
			if (readAgain) {
				pull();
			}
		};
		const pull = () => {
			if (reading) {
				readAgain = true;
				return;
			}
			reading = true;
			reader.read({
				chunk: chunk => enqueueMicrotask(() => {
					readAgain = false;
					if (!canceled1) {
						branch1.controller.enqueue(chunk);
					}
					if (!canceled2) {
						branch2.controller.enqueue(chunk);
					}
					finishRead();
				}),
				close: () => {
					reading = false;
					if (!canceled1) {
						branch1.controller.close();
					}
					if (!canceled2) {
						branch2.controller.close();
					}
					// Closing is re-entrant from `cancel`, which resolves `cancelPromise` after this
					if (!canceled1 || !canceled2) {
						cancelPromise.resolve(undefined);
					}
				},
				error: () => {
					reading = false;
				},
			});
		};
		const pullAlgorithm = () => {
			pull();
			return PromiseResolve(undefined);
		};
		const cancel1Algorithm = (reason: unknown) => {
			canceled1 = true;
			reason1 = reason;
			if (canceled2) {
				cancelPromise.resolve(this.cancel([ reason1, reason2 ]));
			}
			return cancelPromise.promise;
		};
		const cancel2Algorithm = (reason: unknown) => {
			canceled2 = true;
			reason2 = reason;
			if (canceled1) {
				cancelPromise.resolve(this.cancel([ reason1, reason2 ]));
			}
			return cancelPromise.promise;
		};
		const startAlgorithm = () => undefined;
		const stream1 = createReadableStream(startAlgorithm, pullAlgorithm, cancel1Algorithm);
		const stream2 = createReadableStream(startAlgorithm, pullAlgorithm, cancel2Algorithm);
		const branch1 = getReadableStreamInternal(stream1);
		const branch2 = getReadableStreamInternal(stream2);
		void async function() {
			try {
				await reader.closedPromise.promise;
			} catch (error) {
				branch1.controller.error(error);
				branch2.controller.error(error);
				// Canceling both branches closes the source instead, so this never races `cancel`
				cancelPromise.resolve(undefined);
			}
		}();
		return [ stream1, stream2 ];
	}
}

class ReadableStreamDefaultReaderInternal {
	stream: ReadableStreamInternal | undefined;
	closedPromise: PromiseWithResolvers<void>;
	readRequests: ReadRequest[] = [];

	static { defineInternal(this); }

	// https://streams.spec.whatwg.org/#set-up-readable-stream-default-reader
	constructor(stream: ReadableStreamInternal) {
		this.stream = stream;
		this.closedPromise = function() {
			switch (stream.state) {
				case "readable": return PromiseWithResolvers();
				case "closed": return resolvedPromiseCapability();
				case "errored": return rejectedPromiseCapability(stream.storedError);
			}
		}();
	}

	// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
	errorReadRequests(error: unknown): void {
		const { readRequests } = this;
		this.readRequests = [];
		for (let ii = 0; ii < readRequests.length; ++ii) {
			readRequests[ii]!.error(error);
		}
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-reader-read
	read(readRequest: ReadRequest): void {
		const stream = this.stream!;
		stream.disturbed = true;
		const { state } = stream;
		if (state === "closed") {
			readRequest.close();
		} else if (state === "errored") {
			readRequest.error(stream.storedError);
		} else {
			stream.controller.pullSteps(readRequest);
		}
	}

	// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreaderrelease
	release(): void {
		const stream = this.stream!;
		const error = new TypeErrorConstructor("Reader was released");
		if (stream.state === "readable") {
			this.closedPromise.reject(error);
			markAsHandled(this.closedPromise.promise);
		} else {
			this.closedPromise = rejectedPromiseCapability(error);
		}
		stream.reader = undefined;
		this.stream = undefined;
		this.errorReadRequests(error);
	}
}

/** @internal */
export class ReadableStreamDefaultControllerInternal extends QueueWithSizes<unknown> {
	readonly controller: ReadableStreamDefaultController;
	readonly stream: ReadableStreamInternal;
	readonly startAlgorithm: StartAlgorithm;
	readonly pullAlgorithm: PullAlgorithm;
	readonly cancelAlgorithm: CancelAlgorithm;
	readonly strategyHWM: number;
	readonly strategySizeAlgorithm: SizeAlgorithm;
	started = false;
	pulling = false;
	pullAgain = false;
	closeRequested = false;

	constructor(
		stream: ReadableStreamInternal,
		startAlgorithm: StartAlgorithm,
		pullAlgorithm: PullAlgorithm,
		cancelAlgorithm: CancelAlgorithm,
		highWaterMark: number,
		sizeAlgorithm: SizeAlgorithm,
	) {
		super();
		this.stream = stream;
		this.startAlgorithm = startAlgorithm;
		this.pullAlgorithm = pullAlgorithm;
		this.cancelAlgorithm = cancelAlgorithm;
		this.strategyHWM = highWaterMark;
		this.strategySizeAlgorithm = sizeAlgorithm;
		this.controller = new ReadableStreamDefaultController(new InternalParameter(this));
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-can-close-or-enqueue
	get canCloseOrEnqueue(): boolean {
		return !this.closeRequested && this.stream.state === "readable";
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-get-desired-size
	get desiredSize(): number | null {
		const { state } = this.stream;
		if (state === "errored") {
			return null;
		} else if (state === "closed") {
			return 0;
		}
		return this.strategyHWM - this.queueTotalSize;
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-has-backpressure
	get hasBackpressure(): boolean {
		return !this.shouldCallPull;
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-should-call-pull
	get shouldCallPull(): boolean {
		if (!this.canCloseOrEnqueue || !this.started) {
			return false;
		}
		const { reader } = this.stream;
		if (reader !== undefined && reader.readRequests.length > 0) {
			return true;
		}
		return this.desiredSize! > 0;
	}

	// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller
	start(): void {
		const startResult = this.startAlgorithm();
		const startPromise = new PromiseConstructor(resolve => resolve(startResult));
		void (async () => {
			try {
				await startPromise;
			} catch (error) {
				this.error(error);
				return;
			}
			this.started = true;
			this.callPullIfNeeded();
		})();
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-call-pull-if-needed
	callPullIfNeeded(): void {
		if (!this.shouldCallPull) {
			return;
		} else if (this.pulling) {
			this.pullAgain = true;
			return;
		}
		this.pulling = true;
		void (async () => {
			try {
				await this.pullAlgorithm();
			} catch (error) {
				this.error(error);
				return;
			}
			this.pulling = false;
			if (this.pullAgain) {
				this.pullAgain = false;
				this.callPullIfNeeded();
			}
		})();
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-close
	close(): void {
		if (!this.canCloseOrEnqueue) {
			return;
		}
		this.closeRequested = true;
		if (this.queue.length === 0) {
			this.stream.close();
		}
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-enqueue
	enqueue(chunk: unknown): void {
		if (!this.canCloseOrEnqueue) {
			return;
		}
		const { reader } = this.stream;
		if (reader !== undefined && reader.readRequests.length > 0) {
			ArrayShift(reader.readRequests)!.chunk(chunk);
		} else {
			try {
				this.enqueueValueWithSize(chunk, this.strategySizeAlgorithm(chunk));
			} catch (error) {
				this.error(error);
				throw error;
			}
		}
		this.callPullIfNeeded();
	}

	// https://streams.spec.whatwg.org/#readable-stream-default-controller-error
	error(error: unknown): void {
		const { stream } = this;
		if (stream.state !== "readable") {
			return;
		}
		this.resetQueue();
		stream.error(error);
	}

	// https://streams.spec.whatwg.org/#rs-default-controller-private-pull
	pullSteps(readRequest: ReadRequest): void {
		const { stream } = this;
		if (this.queue.length > 0) {
			const chunk = this.dequeueValue();
			if (this.closeRequested && this.queue.length === 0) {
				stream.close();
			} else {
				this.callPullIfNeeded();
			}
			readRequest.chunk(chunk);
		} else {
			ArrayPush(stream.reader!.readRequests, readRequest);
			this.callPullIfNeeded();
		}
	}
}

class ReadableStreamAsyncIteratorInternal {
	readonly reader: ReadableStreamDefaultReaderInternal;
	readonly preventCancel: boolean;
	isFinished = false;
	ongoingPromise: Promise<IteratorResult<unknown>> | undefined = undefined;

	static { defineInternal(this); }

	constructor(reader: ReadableStreamDefaultReaderInternal, preventCancel: boolean) {
		this.reader = reader;
		this.preventCancel = preventCancel;
	}

	// https://streams.spec.whatwg.org/#rs-get-the-next-iteration-result
	nextIterationResult(): Promise<unknown> {
		const { reader } = this;
		const { promise, resolve, reject } = PromiseWithResolvers<unknown>();
		reader.read({
			chunk: resolve,
			close: () => {
				reader.release();
				resolve(endOfIteration);
			},
			error: error => {
				reader.release();
				reject(error);
			},
		});
		return promise;
	}

	// https://webidl.spec.whatwg.org/#es-asynchronous-iterator-prototype-object
	next(): Promise<IteratorResult<unknown>> {
		return this.ongoingPromise = (async (): Promise<IteratorResult<unknown>> => {
			const { ongoingPromise } = this;
			if (ongoingPromise !== undefined) {
				try {
					await ongoingPromise;
				} catch {}
			}
			if (this.isFinished) {
				return { value: undefined, done: true };
			}
			try {
				const result = await this.nextIterationResult();
				if (result === endOfIteration) {
					this.isFinished = true;
					return { value: undefined, done: true };
				}
				return { value: result, done: false };
			} catch (error) {
				this.isFinished = true;
				throw error;
			} finally {
				this.ongoingPromise = undefined;
			}
		})();
	}

	return(value: unknown): Promise<IteratorResult<unknown>> {
		return this.ongoingPromise = (async (): Promise<IteratorResult<unknown>> => {
			const { ongoingPromise } = this;
			if (ongoingPromise !== undefined) {
				try {
					await ongoingPromise;
				} catch {}
			}
			if (!this.isFinished) {
				this.isFinished = true;
				const { reader } = this;
				if (this.preventCancel) {
					reader.release();
				} else {
					const result = reader.stream!.cancel(value);
					reader.release();
					await result;
				}
			}
			return { value, done: true };
		})();
	}
}

// https://streams.spec.whatwg.org/#dictdef-streampipeoptions
function convertPipeOptions(options: unknown): PipeOptions {
	assertDictionary(options);
	const { preventAbort, preventCancel, preventClose, signal } = options ?? kEmptyObject;
	if (signal !== undefined && !isAbortSignal(signal)) {
		throw new TypeErrorConstructor("Provided value is not of type 'AbortSignal'");
	}
	return {
		preventAbort: BooleanConstructor(preventAbort),
		preventCancel: BooleanConstructor(preventCancel),
		preventClose: BooleanConstructor(preventClose),
		signal,
	};
}

// https://streams.spec.whatwg.org/#readable-stream-from-iterable
function readableStreamFromIterable(asyncIterable: unknown): ReadableStream {
	assertObject(asyncIterable);
	const { [SymbolAsyncIterator]: asyncMethod, [SymbolIterator]: syncMethod } = asyncIterable as Record<symbol, unknown>;
	const iterator = function(): AsyncIterator<unknown> {
		const method = asyncMethod ?? syncMethod;
		if (typeof method !== "function") {
			throw new TypeErrorConstructor("Provided value is not iterable");
		}
		const result: unknown = ReflectApply(method, asyncIterable, []);
		assertObject(result);
		if (asyncMethod != null) {
			return result as AsyncIterator<unknown>;
		}
		// eslint-disable-next-line @typescript-eslint/require-await -- `yield*` awaits each result
		return async function*() {
			yield* { [SymbolIterator]: () => result } as unknown as Iterable<unknown>;
		}();
	}();
	const stream = createReadableStream(() => undefined, async () => {
		const iterResult = await iterator.next();
		assertObject(iterResult);
		const { controller } = getReadableStreamInternal(stream);
		if (iterResult.done) {
			controller.close();
		} else {
			controller.enqueue(iterResult.value);
		}
	}, async reason => {
		if (iterator.return !== undefined) {
			assertObject(await iterator.return(reason));
		}
	}, 0);
	return stream;
}

// https://streams.spec.whatwg.org/#rs-class
export class ReadableStream {
	readonly #internal: ReadableStreamInternal;

	static {
		defineInterface(this);
		getReadableStreamInternal = stream => stream.#internal;
		isReadableStream = (value: unknown): value is ReadableStream => {
			try {
				// @ts-expect-error
				return #internal in value;
			} catch {
				return false;
			}
		};
		// https://webidl.spec.whatwg.org/#define-the-asynchronous-iteration-methods
		// eslint-disable-next-line @typescript-eslint/unbound-method -- `@@asyncIterator` is the same function object as `values`
		ObjectDefineProperty(this.prototype, SymbolAsyncIterator, { value: this.prototype.values, writable: true, enumerable: false, configurable: true });
	}

	// https://streams.spec.whatwg.org/#rs-constructor
	constructor(underlyingSource: unknown = undefined, strategy: unknown = kEmptyObject) {
		this.#internal = new ReadableStreamInternal(function(): ControllerFactory {
			const factory = InternalParameter.unwrap<ControllerFactory>(underlyingSource);
			if (factory !== undefined) {
				return factory;
			}
			if (underlyingSource !== undefined) {
				assertObject(underlyingSource);
			}
			const queuingStrategy = new QueuingStrategy(strategy);
			const { cancel, pull, start, type } = (underlyingSource ?? kEmptyObject) as Dictionary;
			assertCallbackFunction(cancel);
			assertCallbackFunction(pull);
			assertCallbackFunction(start);
			if (type !== undefined) {
				if (`${type}` !== "bytes") {
					throw new TypeErrorConstructor("Provided value is not a valid enum value of type 'ReadableStreamType'");
				}
				throw new TypeErrorConstructor("Byte streams are not supported");
			}
			const sizeAlgorithm = queuingStrategy.extractSizeAlgorithm();
			const highWaterMark = queuingStrategy.extractHighWaterMark(1);
			// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
			return stream => {
				const controller: ReadableStreamDefaultControllerInternal = new ReadableStreamDefaultControllerInternal(
					stream,
					start === undefined
						? () => undefined
						: (): unknown => ReflectApply(start, underlyingSource, [ controller.controller ]),
					pull === undefined
						? async () => {}
						// eslint-disable-next-line @typescript-eslint/require-await
						: async (): Promise<unknown> => ReflectApply(pull, underlyingSource, [ controller.controller ]),
					cancel === undefined
						? async () => {}
						// eslint-disable-next-line @typescript-eslint/require-await
						: async (reason): Promise<unknown> => ReflectApply(cancel, underlyingSource, [ reason ]),
					highWaterMark,
					sizeAlgorithm);
				return controller;
			};
		}());
	}

	// https://streams.spec.whatwg.org/#rs-from
	static from(asyncIterable: unknown): ReadableStream {
		requireArguments(arguments.length, 1);
		return readableStreamFromIterable(asyncIterable);
	}

	get locked(): boolean { return this.#internal.reader !== undefined; }

	// https://streams.spec.whatwg.org/#rs-cancel
	cancel(reason: unknown = undefined): Promise<void> {
		return promiseTry(() => {
			const internal = this.#internal;
			if (internal.reader !== undefined) {
				throw new TypeErrorConstructor("ReadableStream is locked");
			}
			return internal.cancel(reason);
		});
	}

	// https://streams.spec.whatwg.org/#rs-get-reader
	getReader(options: unknown = kEmptyObject): ReadableStreamDefaultReader {
		// eslint-disable-next-line @typescript-eslint/no-unused-expressions
		this.#internal;
		assertDictionary(options);
		const { mode } = options ?? kEmptyObject;
		if (mode !== undefined) {
			if (`${mode}` !== "byob") {
				throw new TypeErrorConstructor("Provided value is not a valid enum value of type 'ReadableStreamReaderMode'");
			}
			throw new TypeErrorConstructor("Byte streams are not supported");
		}
		return new ReadableStreamDefaultReader(this);
	}

	// https://streams.spec.whatwg.org/#rs-pipe-through
	pipeThrough(transform: unknown, options: unknown = kEmptyObject): ReadableStream {
		const internal = this.#internal;
		requireArguments(arguments.length, 1);
		assertDictionary(transform);
		const pair = transform ?? kEmptyObject;
		const { readable } = pair;
		if (!isReadableStream(readable)) {
			throw new TypeErrorConstructor("Required member 'readable' is not of type 'ReadableStream'");
		}
		const { writable } = pair;
		if (!isWritableStream(writable)) {
			throw new TypeErrorConstructor("Required member 'writable' is not of type 'WritableStream'");
		}
		const pipeOptions = convertPipeOptions(options);
		if (internal.reader !== undefined) {
			throw new TypeErrorConstructor("ReadableStream is locked");
		}
		const dest = getWritableStreamInternal(writable);
		if (dest.writer !== undefined) {
			throw new TypeErrorConstructor("WritableStream is locked");
		}
		markAsHandled(internal.pipeTo(dest, pipeOptions));
		return readable;
	}

	// https://streams.spec.whatwg.org/#rs-pipe-to
	pipeTo(destination: unknown, options: unknown = kEmptyObject): Promise<void> {
		return promiseTry(() => {
			const internal = this.#internal;
			requireArguments(arguments.length, 1);
			if (!isWritableStream(destination)) {
				throw new TypeErrorConstructor("Provided value is not of type 'WritableStream'");
			}
			const pipeOptions = convertPipeOptions(options);
			if (internal.reader !== undefined) {
				throw new TypeErrorConstructor("ReadableStream is locked");
			}
			const dest = getWritableStreamInternal(destination);
			if (dest.writer !== undefined) {
				throw new TypeErrorConstructor("WritableStream is locked");
			}
			return internal.pipeTo(dest, pipeOptions);
		});
	}

	// https://streams.spec.whatwg.org/#rs-tee
	tee(): ReadableStream[] {
		return this.#internal.tee();
	}

	// https://streams.spec.whatwg.org/#rs-asynciterator
	values(options: unknown = kEmptyObject): AsyncIterator<unknown> {
		const internal = this.#internal;
		assertDictionary(options);
		const preventCancel = BooleanConstructor(options?.preventCancel);
		const iteratorInternal = new ReadableStreamAsyncIteratorInternal(internal.acquireDefaultReader(), preventCancel);
		return new ReadableStreamAsyncIterator(new InternalParameter(iteratorInternal));
	}
}

// https://streams.spec.whatwg.org/#default-reader-class
export class ReadableStreamDefaultReader {
	readonly #internal: ReadableStreamDefaultReaderInternal;

	static { defineInterface(this); }

	constructor(stream: unknown) {
		requireArguments(arguments.length, 1);
		if (!isReadableStream(stream)) {
			throw new TypeErrorConstructor("Provided value is not of type 'ReadableStream'");
		}
		this.#internal = getReadableStreamInternal(stream).acquireDefaultReader();
	}

	get closed(): Promise<void> { return promiseTry(() => this.#internal.closedPromise.promise); }

	// https://streams.spec.whatwg.org/#default-reader-cancel
	cancel(reason: unknown = undefined): Promise<void> {
		return promiseTry(() => {
			const { stream } = this.#internal;
			if (stream === undefined) {
				throw new TypeErrorConstructor("Reader has no associated stream");
			}
			return stream.cancel(reason);
		});
	}

	// https://streams.spec.whatwg.org/#default-reader-read
	read(): Promise<ReadableStreamReadResult> {
		return promiseTry(() => {
			const internal = this.#internal;
			if (internal.stream === undefined) {
				throw new TypeErrorConstructor("Reader has no associated stream");
			}
			const { promise, resolve, reject } = PromiseWithResolvers<ReadableStreamReadResult>();
			internal.read({
				chunk: value => resolve({ value, done: false }),
				close: () => resolve({ value: undefined, done: true }),
				error: reject,
			});
			return promise;
		});
	}

	// https://streams.spec.whatwg.org/#default-reader-release-lock
	releaseLock(): void {
		const internal = this.#internal;
		if (internal.stream !== undefined) {
			internal.release();
		}
	}
}

// https://streams.spec.whatwg.org/#rs-default-controller-class
export class ReadableStreamDefaultController {
	readonly #internal: ReadableStreamDefaultControllerInternal;

	static { defineInterface(this); }

	constructor(internal: unknown = undefined) {
		const unwrapped = InternalParameter.unwrap<ReadableStreamDefaultControllerInternal>(internal);
		if (unwrapped === undefined) {
			throw new TypeErrorConstructor("Illegal constructor");
		}
		this.#internal = unwrapped;
	}

	get desiredSize(): number | null { return this.#internal.desiredSize; }

	// https://streams.spec.whatwg.org/#rs-default-controller-close
	close(): void {
		const internal = this.#internal;
		if (!internal.canCloseOrEnqueue) {
			throw new TypeErrorConstructor("Stream is not in a state that permits close");
		}
		internal.close();
	}

	// https://streams.spec.whatwg.org/#rs-default-controller-enqueue
	enqueue(chunk: unknown = undefined): void {
		const internal = this.#internal;
		if (!internal.canCloseOrEnqueue) {
			throw new TypeErrorConstructor("Stream is not in a state that permits enqueue");
		}
		internal.enqueue(chunk);
	}

	// https://streams.spec.whatwg.org/#rs-default-controller-error
	error(error: unknown = undefined): void {
		this.#internal.error(error);
	}
}

// https://streams.spec.whatwg.org/#rs-asynciterator-prototype
class ReadableStreamAsyncIterator {
	readonly #internal: ReadableStreamAsyncIteratorInternal;

	static {
		defineInterface(this);
		ObjectSetPrototypeOf(this.prototype, AsyncIteratorPrototype);
		ReflectDeleteProperty(this.prototype, "constructor");
		ObjectDefineProperty(this.prototype, SymbolToStringTag, { value: "ReadableStream AsyncIterator", writable: false, enumerable: false, configurable: true });
	}

	constructor(internal: unknown = undefined) {
		const unwrapped = InternalParameter.unwrap<ReadableStreamAsyncIteratorInternal>(internal);
		if (unwrapped === undefined) {
			throw new TypeErrorConstructor("Illegal constructor");
		}
		this.#internal = unwrapped;
	}

	// https://webidl.spec.whatwg.org/#es-asynchronous-iterator-prototype-object
	next(): Promise<IteratorResult<unknown>> {
		return promiseTry(() => this.#internal.next());
	}

	return(value: unknown): Promise<IteratorResult<unknown>> {
		return promiseTry(() => this.#internal.return(value));
	}
}
