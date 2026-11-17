// https://streams.spec.whatwg.org/#ws-model
/* eslint-disable @typescript-eslint/prefer-for-of -- `for-of` goes through the patchable array iterator */
import type { AbortSignal } from "@isolated-vm/runtime/dom/abort";
import type { SizeAlgorithm } from "@isolated-vm/runtime/streams/queuing-strategy";
import type { Dictionary } from "@isolated-vm/runtime/webidl";
import { ArrayPush, ArrayShift, PromiseConstructor, PromiseReject, PromiseResolve, PromiseWithResolvers, RangeErrorConstructor, ReflectApply, TypeErrorConstructor, kEmptyObject } from "@isolated-vm/intrinsics";
import { createAbortSignal, signalAbort } from "@isolated-vm/runtime/dom/abort";
import { QueuingStrategy } from "@isolated-vm/runtime/streams/queuing-strategy";
import { QueueWithSizes, markAsHandled, promiseTry, rejectedPromiseCapability, resolvedPromiseCapability } from "@isolated-vm/runtime/streams/utility";
import { InternalParameter, assertCallbackFunction, assertObject, defineInterface, defineInternal, requireArguments } from "@isolated-vm/runtime/webidl";
import { getPromiseInternal } from "isolated-vm:runtime";

type WritableStreamState = "writable" | "closed" | "erroring" | "errored";
type StartAlgorithm = () => unknown;
type WriteAlgorithm = (chunk: unknown) => Promise<unknown>;
type CloseAlgorithm = () => Promise<unknown>;
type AbortAlgorithm = (reason: unknown) => Promise<unknown>;
type ControllerFactory = (stream: WritableStreamInternal) => WritableStreamDefaultControllerInternal;

interface PendingAbortRequest {
	readonly promise: PromiseWithResolvers<void>;
	readonly reason: unknown;
	readonly wasAlreadyErroring: boolean;
}

const closeSentinel = Symbol("closeSentinel");

// Private state accessors, assigned from within the class bodies
/** @internal */ export let getWritableStreamInternal: (stream: WritableStream) => WritableStreamInternal;
/** @internal */ export let isWritableStream: (value: unknown) => value is WritableStream;

// https://streams.spec.whatwg.org/#create-writable-stream
/** @internal */
export function createWritableStream(
	startAlgorithm: StartAlgorithm,
	writeAlgorithm: WriteAlgorithm,
	closeAlgorithm: CloseAlgorithm,
	abortAlgorithm: AbortAlgorithm,
	highWaterMark: number,
	sizeAlgorithm: SizeAlgorithm,
): WritableStream {
	const controller: ControllerFactory = stream =>
		new WritableStreamDefaultControllerInternal(stream, startAlgorithm, writeAlgorithm, closeAlgorithm, abortAlgorithm, highWaterMark, sizeAlgorithm);
	return new WritableStream(new InternalParameter(controller));
}

/** @internal */
export class WritableStreamInternal {
	readonly controller: WritableStreamDefaultControllerInternal;
	state: WritableStreamState = "writable";
	storedError: unknown = undefined;
	writer: WritableStreamDefaultWriterInternal | undefined = undefined;
	backpressure = false;
	closeRequest: PromiseWithResolvers<void> | undefined = undefined;
	inFlightWriteRequest: PromiseWithResolvers<void> | undefined = undefined;
	inFlightCloseRequest: PromiseWithResolvers<void> | undefined = undefined;
	pendingAbortRequest: PendingAbortRequest | undefined = undefined;
	writeRequests: PromiseWithResolvers<void>[] = [];

	static { defineInternal(this); }

	constructor(controller: ControllerFactory) {
		this.controller = controller(this);
		this.controller.start();
	}

	// https://streams.spec.whatwg.org/#writable-stream-close-queued-or-in-flight
	get closeQueuedOrInFlight(): boolean {
		return this.closeRequest !== undefined || this.inFlightCloseRequest !== undefined;
	}

	// https://streams.spec.whatwg.org/#acquire-writable-stream-default-writer
	acquireDefaultWriter(): WritableStreamDefaultWriterInternal {
		if (this.writer !== undefined) {
			throw new TypeErrorConstructor("WritableStream is locked");
		}
		return this.writer = new WritableStreamDefaultWriterInternal(this);
	}

	// https://streams.spec.whatwg.org/#writable-stream-abort
	abort(reason: unknown): Promise<void> {
		const { state: initialState } = this;
		if (initialState === "closed" || initialState === "errored") {
			return PromiseResolve(undefined);
		}
		signalAbort(this.controller.signal, reason);
		// Abort listeners may have changed the state
		const { state } = this;
		if (state === "closed" || state === "errored") {
			return PromiseResolve(undefined);
		} else if (this.pendingAbortRequest !== undefined) {
			return this.pendingAbortRequest.promise.promise;
		}
		const wasAlreadyErroring = state === "erroring";
		const promise: PromiseWithResolvers<void> = PromiseWithResolvers();
		this.pendingAbortRequest = { promise, reason: wasAlreadyErroring ? undefined : reason, wasAlreadyErroring };
		if (!wasAlreadyErroring) {
			this.startErroring(reason);
		}
		return promise.promise;
	}

	// https://streams.spec.whatwg.org/#writable-stream-close
	close(): Promise<void> {
		const { state } = this;
		if (state === "closed" || state === "errored") {
			return PromiseReject(new TypeErrorConstructor("Stream is closed or errored"));
		}
		const promise: PromiseWithResolvers<void> = PromiseWithResolvers();
		this.closeRequest = promise;
		const { writer } = this;
		if (writer !== undefined && this.backpressure && state === "writable") {
			writer.readyPromise.resolve(undefined);
		}
		this.controller.close();
		return promise.promise;
	}

	// https://streams.spec.whatwg.org/#writable-stream-deal-with-rejection
	dealWithRejection(error: unknown): void {
		if (this.state === "writable") {
			this.startErroring(error);
		} else {
			this.finishErroring();
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-start-erroring
	startErroring(reason: unknown): void {
		const { controller, writer } = this;
		this.state = "erroring";
		this.storedError = reason;
		if (writer !== undefined) {
			writer.ensureReadyPromiseRejected(reason);
		}
		if (this.inFlightWriteRequest === undefined && this.inFlightCloseRequest === undefined && controller.started) {
			this.finishErroring();
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-finish-erroring
	finishErroring(): void {
		this.state = "errored";
		this.controller.resetQueue();
		const { storedError, writeRequests } = this;
		this.writeRequests = [];
		for (let ii = 0; ii < writeRequests.length; ++ii) {
			writeRequests[ii]!.reject(storedError);
		}
		const abortRequest = this.pendingAbortRequest;
		if (abortRequest === undefined) {
			this.rejectCloseAndClosedPromiseIfNeeded();
			return;
		}
		this.pendingAbortRequest = undefined;
		if (abortRequest.wasAlreadyErroring) {
			abortRequest.promise.reject(storedError);
			this.rejectCloseAndClosedPromiseIfNeeded();
			return;
		}
		void (async () => {
			try {
				await this.controller.abortAlgorithm(abortRequest.reason);
				abortRequest.promise.resolve(undefined);
			} catch (error) {
				abortRequest.promise.reject(error);
			}
			this.rejectCloseAndClosedPromiseIfNeeded();
		})();
	}

	// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-close
	finishInFlightClose(): void {
		this.inFlightCloseRequest!.resolve(undefined);
		this.inFlightCloseRequest = undefined;
		if (this.state === "erroring") {
			this.storedError = undefined;
			if (this.pendingAbortRequest !== undefined) {
				this.pendingAbortRequest.promise.resolve(undefined);
				this.pendingAbortRequest = undefined;
			}
		}
		this.state = "closed";
		this.writer?.closedPromise.resolve(undefined);
	}

	// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-close-with-error
	finishInFlightCloseWithError(error: unknown): void {
		this.inFlightCloseRequest!.reject(error);
		this.inFlightCloseRequest = undefined;
		if (this.pendingAbortRequest !== undefined) {
			this.pendingAbortRequest.promise.reject(error);
			this.pendingAbortRequest = undefined;
		}
		this.dealWithRejection(error);
	}

	// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-write
	finishInFlightWrite(): void {
		this.inFlightWriteRequest!.resolve(undefined);
		this.inFlightWriteRequest = undefined;
	}

	// https://streams.spec.whatwg.org/#writable-stream-finish-in-flight-write-with-error
	finishInFlightWriteWithError(error: unknown): void {
		this.inFlightWriteRequest!.reject(error);
		this.inFlightWriteRequest = undefined;
		this.dealWithRejection(error);
	}

	// https://streams.spec.whatwg.org/#writable-stream-reject-close-and-closed-promise-if-needed
	rejectCloseAndClosedPromiseIfNeeded(): void {
		const { closeRequest, storedError, writer } = this;
		if (closeRequest !== undefined) {
			closeRequest.reject(storedError);
			this.closeRequest = undefined;
		}
		if (writer !== undefined) {
			writer.closedPromise.reject(storedError);
			markAsHandled(writer.closedPromise.promise);
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-update-backpressure
	updateBackpressure(backpressure: boolean): void {
		const { writer } = this;
		if (writer !== undefined && backpressure !== this.backpressure) {
			if (backpressure) {
				writer.readyPromise = PromiseWithResolvers();
			} else {
				writer.readyPromise.resolve(undefined);
			}
		}
		this.backpressure = backpressure;
	}
}

/** @internal */
export class WritableStreamDefaultWriterInternal {
	stream: WritableStreamInternal | undefined;
	closedPromise: PromiseWithResolvers<void>;
	readyPromise: PromiseWithResolvers<void>;

	static { defineInternal(this); }

	// https://streams.spec.whatwg.org/#set-up-writable-stream-default-writer
	constructor(stream: WritableStreamInternal) {
		this.stream = stream;
		const { state, storedError } = stream;
		if (state === "writable") {
			this.readyPromise = !stream.closeQueuedOrInFlight && stream.backpressure ? PromiseWithResolvers() : resolvedPromiseCapability();
			this.closedPromise = PromiseWithResolvers();
		} else if (state === "erroring") {
			this.readyPromise = rejectedPromiseCapability(storedError);
			this.closedPromise = PromiseWithResolvers();
		} else if (state === "closed") {
			this.readyPromise = resolvedPromiseCapability();
			this.closedPromise = resolvedPromiseCapability();
		} else {
			this.readyPromise = rejectedPromiseCapability(storedError);
			this.closedPromise = rejectedPromiseCapability(storedError);
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-writer-get-desired-size
	get desiredSize(): number | null {
		const stream = this.stream!;
		const { state } = stream;
		if (state === "errored" || state === "erroring") {
			return null;
		} else if (state === "closed") {
			return 0;
		}
		return stream.controller.desiredSize;
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-writer-close-with-error-propagation
	closeWithErrorPropagation(): Promise<void> {
		const stream = this.stream!;
		const { state } = stream;
		if (stream.closeQueuedOrInFlight || state === "closed") {
			return PromiseResolve(undefined);
		} else if (state === "errored") {
			return PromiseReject(stream.storedError);
		}
		return stream.close();
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-closed-promise-rejected
	ensureClosedPromiseRejected(error: unknown): void {
		if (getPromiseInternal(this.closedPromise.promise) === undefined) {
			this.closedPromise.reject(error);
			markAsHandled(this.closedPromise.promise);
		} else {
			this.closedPromise = rejectedPromiseCapability(error);
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-ready-promise-rejected
	ensureReadyPromiseRejected(error: unknown): void {
		if (getPromiseInternal(this.readyPromise.promise) === undefined) {
			this.readyPromise.reject(error);
			markAsHandled(this.readyPromise.promise);
		} else {
			this.readyPromise = rejectedPromiseCapability(error);
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-writer-release
	release(): void {
		const stream = this.stream!;
		const releasedError = new TypeErrorConstructor("Writer was released");
		this.ensureReadyPromiseRejected(releasedError);
		this.ensureClosedPromiseRejected(releasedError);
		stream.writer = undefined;
		this.stream = undefined;
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-writer-write
	write(chunk: unknown): Promise<void> {
		const stream = this.stream!;
		const { controller } = stream;
		const chunkSize = controller.getChunkSize(chunk);
		if (stream !== this.stream) {
			return PromiseReject(new TypeErrorConstructor("Writer was released"));
		}
		const { state } = stream;
		if (state === "errored") {
			return PromiseReject(stream.storedError);
		} else if (stream.closeQueuedOrInFlight || state === "closed") {
			return PromiseReject(new TypeErrorConstructor("Stream is closing or closed"));
		} else if (state === "erroring") {
			return PromiseReject(stream.storedError);
		}
		const promise: PromiseWithResolvers<void> = PromiseWithResolvers();
		ArrayPush(stream.writeRequests, promise);
		controller.write(chunk, chunkSize);
		return promise.promise;
	}
}

/** @internal */
export class WritableStreamDefaultControllerInternal extends QueueWithSizes<unknown> {
	readonly controller: WritableStreamDefaultController;
	readonly stream: WritableStreamInternal;
	readonly signal = createAbortSignal();
	readonly startAlgorithm: StartAlgorithm;
	readonly writeAlgorithm: WriteAlgorithm;
	readonly closeAlgorithm: CloseAlgorithm;
	readonly abortAlgorithm: AbortAlgorithm;
	readonly strategyHWM: number;
	readonly strategySizeAlgorithm: SizeAlgorithm;
	started = false;

	constructor(
		stream: WritableStreamInternal,
		startAlgorithm: StartAlgorithm,
		writeAlgorithm: WriteAlgorithm,
		closeAlgorithm: CloseAlgorithm,
		abortAlgorithm: AbortAlgorithm,
		highWaterMark: number,
		sizeAlgorithm: SizeAlgorithm,
	) {
		super();
		this.stream = stream;
		this.startAlgorithm = startAlgorithm;
		this.writeAlgorithm = writeAlgorithm;
		this.closeAlgorithm = closeAlgorithm;
		this.abortAlgorithm = abortAlgorithm;
		this.strategyHWM = highWaterMark;
		this.strategySizeAlgorithm = sizeAlgorithm;
		this.controller = new WritableStreamDefaultController(new InternalParameter(this));
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-get-backpressure
	get backpressure(): boolean {
		return this.desiredSize <= 0;
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-get-desired-size
	get desiredSize(): number {
		return this.strategyHWM - this.queueTotalSize;
	}

	// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller
	start(): void {
		const { stream } = this;
		stream.updateBackpressure(this.backpressure);
		const startResult = this.startAlgorithm();
		const startPromise = new PromiseConstructor(resolve => resolve(startResult));
		void (async () => {
			try {
				await startPromise;
			} catch (error) {
				this.started = true;
				stream.dealWithRejection(error);
				return;
			}
			this.started = true;
			this.advanceQueueIfNeeded();
		})();
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-advance-queue-if-needed
	advanceQueueIfNeeded(): void {
		const { stream } = this;
		if (!this.started || stream.inFlightWriteRequest !== undefined) {
			return;
		}
		const { state } = stream;
		if (state === "closed" || state === "errored") {
			return;
		} else if (state === "erroring") {
			stream.finishErroring();
			return;
		} else if (this.queue.length === 0) {
			return;
		}
		const value = this.peekQueueValue();
		if (value === closeSentinel) {
			this.processClose();
		} else {
			this.processWrite(value);
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-close
	close(): void {
		this.enqueueValueWithSize(closeSentinel, 0);
		this.advanceQueueIfNeeded();
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-error-if-needed
	errorIfNeeded(error: unknown): void {
		if (this.stream.state === "writable") {
			this.stream.startErroring(error);
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-get-chunk-size
	getChunkSize(chunk: unknown): number {
		try {
			return this.strategySizeAlgorithm(chunk);
		} catch (error) {
			this.errorIfNeeded(error);
			return 1;
		}
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-process-close
	processClose(): void {
		const { stream } = this;
		stream.inFlightCloseRequest = stream.closeRequest;
		stream.closeRequest = undefined;
		this.dequeueValue();
		void (async () => {
			try {
				await this.closeAlgorithm();
			} catch (error) {
				stream.finishInFlightCloseWithError(error);
				return;
			}
			stream.finishInFlightClose();
		})();
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-process-write
	processWrite(chunk: unknown): void {
		const { stream } = this;
		stream.inFlightWriteRequest = ArrayShift(stream.writeRequests);
		void (async () => {
			try {
				await this.writeAlgorithm(chunk);
			} catch (error) {
				stream.finishInFlightWriteWithError(error);
				return;
			}
			stream.finishInFlightWrite();
			this.dequeueValue();
			if (!stream.closeQueuedOrInFlight && stream.state === "writable") {
				stream.updateBackpressure(this.backpressure);
			}
			this.advanceQueueIfNeeded();
		})();
	}

	// https://streams.spec.whatwg.org/#writable-stream-default-controller-write
	write(chunk: unknown, chunkSize: number): void {
		try {
			this.enqueueValueWithSize(chunk, chunkSize);
		} catch (error) {
			this.errorIfNeeded(error);
			return;
		}
		const { stream } = this;
		if (!stream.closeQueuedOrInFlight && stream.state === "writable") {
			stream.updateBackpressure(this.backpressure);
		}
		this.advanceQueueIfNeeded();
	}
}

// https://streams.spec.whatwg.org/#ws-class
export class WritableStream {
	readonly #internal: WritableStreamInternal;

	static {
		defineInterface(this);
		getWritableStreamInternal = stream => stream.#internal;
		isWritableStream = (value: unknown): value is WritableStream => {
			try {
				// @ts-expect-error
				return #internal in value;
			} catch {
				return false;
			}
		};
	}

	// https://streams.spec.whatwg.org/#ws-constructor
	constructor(underlyingSink: unknown = undefined, strategy: unknown = kEmptyObject) {
		this.#internal = new WritableStreamInternal(function(): ControllerFactory {
			const factory = InternalParameter.unwrap<ControllerFactory>(underlyingSink);
			if (factory !== undefined) {
				return factory;
			}
			if (underlyingSink !== undefined) {
				assertObject(underlyingSink);
			}
			const queuingStrategy = new QueuingStrategy(strategy);
			const { abort, close, start, type, write } = (underlyingSink ?? kEmptyObject) as Dictionary;
			assertCallbackFunction(abort);
			assertCallbackFunction(close);
			assertCallbackFunction(start);
			assertCallbackFunction(write);
			if (type !== undefined) {
				throw new RangeErrorConstructor("Invalid type is specified");
			}
			const sizeAlgorithm = queuingStrategy.extractSizeAlgorithm();
			const highWaterMark = queuingStrategy.extractHighWaterMark(1);
			// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
			return stream => {
				const controller: WritableStreamDefaultControllerInternal = new WritableStreamDefaultControllerInternal(
					stream,
					start === undefined
						? () => undefined
						: (): unknown => ReflectApply(start, underlyingSink, [ controller.controller ]),
					write === undefined
						? async () => {}
						// eslint-disable-next-line @typescript-eslint/require-await
						: async (chunk): Promise<unknown> => ReflectApply(write, underlyingSink, [ chunk, controller.controller ]),
					close === undefined
						? async () => {}
						// eslint-disable-next-line @typescript-eslint/require-await
						: async (): Promise<unknown> => ReflectApply(close, underlyingSink, []),
					abort === undefined
						? async () => {}
						// eslint-disable-next-line @typescript-eslint/require-await
						: async (reason): Promise<unknown> => ReflectApply(abort, underlyingSink, [ reason ]),
					highWaterMark,
					sizeAlgorithm);
				return controller;
			};
		}());
	}

	get locked(): boolean { return this.#internal.writer !== undefined; }

	// https://streams.spec.whatwg.org/#ws-abort
	abort(reason: unknown = undefined): Promise<void> {
		return promiseTry(() => {
			const internal = this.#internal;
			if (internal.writer !== undefined) {
				throw new TypeErrorConstructor("WritableStream is locked");
			}
			return internal.abort(reason);
		});
	}

	// https://streams.spec.whatwg.org/#ws-close
	close(): Promise<void> {
		return promiseTry(() => {
			const internal = this.#internal;
			if (internal.writer !== undefined) {
				throw new TypeErrorConstructor("WritableStream is locked");
			} else if (internal.closeQueuedOrInFlight) {
				throw new TypeErrorConstructor("Stream is already closing");
			}
			return internal.close();
		});
	}

	// https://streams.spec.whatwg.org/#ws-get-writer
	getWriter(): WritableStreamDefaultWriter {
		return new WritableStreamDefaultWriter(this);
	}
}

// https://streams.spec.whatwg.org/#default-writer-class
export class WritableStreamDefaultWriter {
	readonly #internal: WritableStreamDefaultWriterInternal;

	static { defineInterface(this); }

	constructor(stream: unknown) {
		requireArguments(arguments.length, 1);
		if (!isWritableStream(stream)) {
			throw new TypeErrorConstructor("Provided value is not of type 'WritableStream'");
		}
		this.#internal = getWritableStreamInternal(stream).acquireDefaultWriter();
	}

	get closed(): Promise<void> { return promiseTry(() => this.#internal.closedPromise.promise); }
	get ready(): Promise<void> { return promiseTry(() => this.#internal.readyPromise.promise); }

	get desiredSize(): number | null {
		const internal = this.#internal;
		if (internal.stream === undefined) {
			throw new TypeErrorConstructor("Writer has no associated stream");
		}
		return internal.desiredSize;
	}

	// https://streams.spec.whatwg.org/#default-writer-abort
	abort(reason: unknown = undefined): Promise<void> {
		return promiseTry(() => {
			const { stream } = this.#internal;
			if (stream === undefined) {
				throw new TypeErrorConstructor("Writer has no associated stream");
			}
			return stream.abort(reason);
		});
	}

	// https://streams.spec.whatwg.org/#default-writer-close
	close(): Promise<void> {
		return promiseTry(() => {
			const { stream } = this.#internal;
			if (stream === undefined) {
				throw new TypeErrorConstructor("Writer has no associated stream");
			} else if (stream.closeQueuedOrInFlight) {
				throw new TypeErrorConstructor("Stream is already closing");
			}
			return stream.close();
		});
	}

	// https://streams.spec.whatwg.org/#default-writer-release-lock
	releaseLock(): void {
		const internal = this.#internal;
		if (internal.stream !== undefined) {
			internal.release();
		}
	}

	// https://streams.spec.whatwg.org/#default-writer-write
	write(chunk: unknown = undefined): Promise<void> {
		return promiseTry(() => {
			const internal = this.#internal;
			if (internal.stream === undefined) {
				throw new TypeErrorConstructor("Writer has no associated stream");
			}
			return internal.write(chunk);
		});
	}
}

// https://streams.spec.whatwg.org/#ws-default-controller-class
export class WritableStreamDefaultController {
	readonly #internal: WritableStreamDefaultControllerInternal;

	static { defineInterface(this); }

	constructor(internal: unknown = undefined) {
		const unwrapped = InternalParameter.unwrap<WritableStreamDefaultControllerInternal>(internal);
		if (unwrapped === undefined) {
			throw new TypeErrorConstructor("Illegal constructor");
		}
		this.#internal = unwrapped;
	}

	get signal(): AbortSignal { return this.#internal.signal; }

	// https://streams.spec.whatwg.org/#ws-default-controller-error
	error(error: unknown = undefined): void {
		this.#internal.errorIfNeeded(error);
	}
}
