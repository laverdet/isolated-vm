// https://streams.spec.whatwg.org/#ts-model
import type { QueuingStrategy, SizeAlgorithm } from "@isolated-vm/runtime/streams/queuing-strategy";
import type { ReadableStreamDefaultControllerInternal, ReadableStreamInternal, ReadableWritablePair } from "@isolated-vm/runtime/streams/readable";
import type { WritableStreamInternal } from "@isolated-vm/runtime/streams/writable";
import type { Dictionary } from "@isolated-vm/runtime/webidl";
import { PromiseWithResolvers, RangeErrorConstructor, ReflectApply, TypeErrorConstructor, kEmptyObject } from "@isolated-vm/intrinsics";
import { QueuingStrategyInternal } from "@isolated-vm/runtime/streams/queuing-strategy";
import { ReadableStream, createReadableStream, getReadableStreamInternal } from "@isolated-vm/runtime/streams/readable";
import { WritableStream, createWritableStream, getWritableStreamInternal } from "@isolated-vm/runtime/streams/writable";
import { InternalParameter, assertCallbackFunction, assertObject, defineInterface, defineInternal } from "@isolated-vm/runtime/webidl";

type TransformAlgorithm = (chunk: unknown) => Promise<unknown>;
type FlushAlgorithm = () => Promise<unknown>;
type CancelAlgorithm = (reason: unknown) => Promise<unknown>;

// https://streams.spec.whatwg.org/#dictdef-transformer
export interface Transformer {
	start?: (controller: TransformStreamDefaultController) => unknown;
	transform?: (chunk: unknown, controller: TransformStreamDefaultController) => unknown;
	flush?: (controller: TransformStreamDefaultController) => unknown;
	cancel?: (reason: unknown) => unknown;
}

/** @internal */ export let getTransformStreamInternal: (stream: TransformStream) => TransformStreamInternal;

/** @internal */
export class TransformStreamInternal {
	declare controller: TransformStreamDefaultControllerInternal;
	readonly readable: ReadableStream;
	readonly readableInternal: ReadableStreamInternal;
	readonly writable: WritableStream;
	readonly writableInternal: WritableStreamInternal;
	backpressure = true;
	backpressureChangePromise: PromiseWithResolvers<void> = PromiseWithResolvers();

	static { defineInternal(this); }

	// https://streams.spec.whatwg.org/#initialize-transform-stream
	constructor(
		startPromise: Promise<unknown>,
		writableHighWaterMark: number,
		writableSizeAlgorithm: SizeAlgorithm,
		readableHighWaterMark: number,
		readableSizeAlgorithm: SizeAlgorithm,
	) {
		const startAlgorithm = () => startPromise;
		this.writable = createWritableStream(
			startAlgorithm,
			chunk => this.sinkWrite(chunk),
			() => this.sinkClose(),
			reason => this.sinkAbort(reason),
			writableHighWaterMark,
			writableSizeAlgorithm);
		this.readable = createReadableStream(
			startAlgorithm,
			() => this.sourcePull(),
			reason => this.sourceCancel(reason),
			readableHighWaterMark,
			readableSizeAlgorithm);
		this.readableInternal = getReadableStreamInternal(this.readable);
		this.writableInternal = getWritableStreamInternal(this.writable);
	}

	get readableController(): ReadableStreamDefaultControllerInternal {
		return this.readableInternal.controller;
	}

	// https://streams.spec.whatwg.org/#transform-stream-error
	error(error: unknown): void {
		this.readableController.error(error);
		this.errorWritableAndUnblockWrite(error);
	}

	// https://streams.spec.whatwg.org/#transform-stream-error-writable-and-unblock-write
	errorWritableAndUnblockWrite(error: unknown): void {
		this.writableInternal.controller.errorIfNeeded(error);
		this.unblockWrite();
	}

	// https://streams.spec.whatwg.org/#transform-stream-unblock-write
	unblockWrite(): void {
		if (this.backpressure) {
			this.setBackpressure(false);
		}
	}

	// https://streams.spec.whatwg.org/#transform-stream-set-backpressure
	setBackpressure(backpressure: boolean): void {
		this.backpressureChangePromise.resolve(undefined);
		this.backpressureChangePromise = PromiseWithResolvers();
		this.backpressure = backpressure;
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-sink-write-algorithm
	async sinkWrite(chunk: unknown): Promise<unknown> {
		if (this.backpressure) {
			await this.backpressureChangePromise.promise;
			const { writableInternal } = this;
			if (writableInternal.state === "erroring") {
				throw writableInternal.storedError;
			}
		}
		return this.controller.performTransform(chunk);
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-sink-abort-algorithm
	// `finishPromise` is set before the transformer is called, since it may re-enter through the other side
	sinkAbort(reason: unknown): Promise<void> {
		const { controller, readableInternal } = this;
		if (controller.finishPromise !== undefined) {
			return controller.finishPromise.promise;
		}
		const finishPromise = controller.finishPromise = PromiseWithResolvers();
		void async function() {
			try {
				await controller.cancelAlgorithm(reason);
			} catch (error) {
				readableInternal.controller.error(error);
				finishPromise.reject(error);
				return;
			}
			if (readableInternal.state === "errored") {
				finishPromise.reject(readableInternal.storedError);
			} else {
				readableInternal.controller.error(reason);
				finishPromise.resolve(undefined);
			}
		}();
		return finishPromise.promise;
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-sink-close-algorithm
	sinkClose(): Promise<void> {
		const { controller, readableInternal } = this;
		if (controller.finishPromise !== undefined) {
			return controller.finishPromise.promise;
		}
		const finishPromise = controller.finishPromise = PromiseWithResolvers();
		void async function() {
			try {
				await controller.flushAlgorithm();
			} catch (error) {
				readableInternal.controller.error(error);
				finishPromise.reject(error);
				return;
			}
			if (readableInternal.state === "errored") {
				finishPromise.reject(readableInternal.storedError);
			} else {
				readableInternal.controller.close();
				finishPromise.resolve(undefined);
			}
		}();
		return finishPromise.promise;
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-source-pull
	sourcePull(): Promise<void> {
		this.setBackpressure(false);
		return this.backpressureChangePromise.promise;
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-source-cancel
	sourceCancel(reason: unknown): Promise<void> {
		const { controller, writableInternal } = this;
		if (controller.finishPromise !== undefined) {
			return controller.finishPromise.promise;
		}
		const finishPromise = controller.finishPromise = PromiseWithResolvers();
		void (async () => {
			try {
				await controller.cancelAlgorithm(reason);
			} catch (error) {
				this.errorWritableAndUnblockWrite(error);
				finishPromise.reject(error);
				return;
			}
			if (writableInternal.state === "errored") {
				finishPromise.reject(writableInternal.storedError);
			} else {
				this.errorWritableAndUnblockWrite(reason);
				finishPromise.resolve(undefined);
			}
		})();
		return finishPromise.promise;
	}
}

class TransformStreamDefaultControllerInternal {
	readonly controller: TransformStreamDefaultController;
	readonly stream: TransformStreamInternal;
	readonly transformAlgorithm: TransformAlgorithm;
	readonly flushAlgorithm: FlushAlgorithm;
	readonly cancelAlgorithm: CancelAlgorithm;
	finishPromise: PromiseWithResolvers<void> | undefined = undefined;

	static { defineInternal(this); }

	constructor(
		stream: TransformStreamInternal,
		transformAlgorithm: TransformAlgorithm,
		flushAlgorithm: FlushAlgorithm,
		cancelAlgorithm: CancelAlgorithm,
	) {
		this.stream = stream;
		this.transformAlgorithm = transformAlgorithm;
		this.flushAlgorithm = flushAlgorithm;
		this.cancelAlgorithm = cancelAlgorithm;
		this.controller = new TransformStreamDefaultController(new InternalParameter(this));
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-controller-enqueue
	enqueue(chunk: unknown): void {
		const { stream } = this;
		const readable = stream.readableController;
		if (!readable.canCloseOrEnqueue) {
			throw new TypeErrorConstructor("Readable side is not in a state that permits enqueue");
		}
		try {
			readable.enqueue(chunk);
		} catch (error) {
			stream.errorWritableAndUnblockWrite(error);
			throw stream.readableInternal.storedError;
		}
		if (readable.hasBackpressure !== stream.backpressure) {
			stream.setBackpressure(true);
		}
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-controller-perform-transform
	async performTransform(chunk: unknown): Promise<unknown> {
		try {
			return await this.transformAlgorithm(chunk);
		} catch (error) {
			this.stream.error(error);
			throw error;
		}
	}

	// https://streams.spec.whatwg.org/#transform-stream-default-controller-terminate
	terminate(): void {
		const { stream } = this;
		stream.readableController.close();
		stream.errorWritableAndUnblockWrite(new TypeErrorConstructor("TransformStream terminated"));
	}
}

// https://streams.spec.whatwg.org/#ts-class
export class TransformStream {
	readonly #internal: TransformStreamInternal;

	static {
		defineInterface(this);
		getTransformStreamInternal = stream => stream.#internal;
	}

	// https://streams.spec.whatwg.org/#ts-constructor
	constructor(transformer?: Transformer, writableStrategy?: QueuingStrategy, readableStrategy?: QueuingStrategy);
	constructor(transformer: unknown = undefined, writableStrategy: unknown = kEmptyObject, readableStrategy: unknown = kEmptyObject) {
		if (transformer !== undefined) {
			assertObject(transformer);
		}
		const writableQueuingStrategy = new QueuingStrategyInternal(writableStrategy);
		const readableQueuingStrategy = new QueuingStrategyInternal(readableStrategy);
		const { cancel, flush, readableType, start, transform, writableType } = (transformer ?? kEmptyObject) as Dictionary;
		assertCallbackFunction(cancel);
		assertCallbackFunction(flush);
		assertCallbackFunction(start);
		assertCallbackFunction(transform);
		if (readableType !== undefined) {
			throw new RangeErrorConstructor("Invalid readableType is specified");
		} else if (writableType !== undefined) {
			throw new RangeErrorConstructor("Invalid writableType is specified");
		}
		const readableHighWaterMark = readableQueuingStrategy.extractHighWaterMark(0);
		const readableSizeAlgorithm = readableQueuingStrategy.extractSizeAlgorithm();
		const writableHighWaterMark = writableQueuingStrategy.extractHighWaterMark(1);
		const writableSizeAlgorithm = writableQueuingStrategy.extractSizeAlgorithm();
		const startPromise = PromiseWithResolvers<unknown>();
		const internal = this.#internal =
			new TransformStreamInternal(startPromise.promise, writableHighWaterMark, writableSizeAlgorithm, readableHighWaterMark, readableSizeAlgorithm);
		// https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
		const controller = internal.controller = new TransformStreamDefaultControllerInternal(
			internal,
			transform === undefined
				// eslint-disable-next-line @typescript-eslint/require-await
				? async (chunk): Promise<unknown> => controller.enqueue(chunk)
				// eslint-disable-next-line @typescript-eslint/require-await
				: async (chunk): Promise<unknown> => ReflectApply(transform, transformer, [ chunk, controller.controller ]),
			flush === undefined
				? async () => {}
				// eslint-disable-next-line @typescript-eslint/require-await
				: async (): Promise<unknown> => ReflectApply(flush, transformer, [ controller.controller ]),
			cancel === undefined
				? async () => {}
				// eslint-disable-next-line @typescript-eslint/require-await
				: async (reason): Promise<unknown> => ReflectApply(cancel, transformer, [ reason ]));
		startPromise.resolve(start === undefined ? undefined : ReflectApply(start, transformer, [ controller.controller ]));
	}

	get readable(): ReadableStream { return this.#internal.readable; }
	get writable(): WritableStream { return this.#internal.writable; }
}

// https://streams.spec.whatwg.org/#other-specs-ts-wrapping
/** @internal */
export function createTransformStream(transformer: Transformer): ReadableWritablePair {
	const { readable, writable } = getTransformStreamInternal(new TransformStream(transformer));
	return { readable, writable };
}

// https://streams.spec.whatwg.org/#ts-default-controller-class
export class TransformStreamDefaultController {
	readonly #internal: TransformStreamDefaultControllerInternal;

	static { defineInterface(this); }

	/** @internal */
	constructor(internal: unknown = undefined) {
		const unwrapped = InternalParameter.unwrap<TransformStreamDefaultControllerInternal>(internal);
		if (unwrapped === undefined) {
			throw new TypeErrorConstructor("Illegal constructor");
		}
		this.#internal = unwrapped;
	}

	get desiredSize(): number | null { return this.#internal.stream.readableController.desiredSize; }

	// https://streams.spec.whatwg.org/#ts-default-controller-enqueue
	enqueue(chunk: unknown = undefined): void {
		this.#internal.enqueue(chunk);
	}

	// https://streams.spec.whatwg.org/#ts-default-controller-error
	error(reason: unknown = undefined): void {
		this.#internal.stream.error(reason);
	}

	// https://streams.spec.whatwg.org/#ts-default-controller-terminate
	terminate(): void {
		this.#internal.terminate();
	}
}
