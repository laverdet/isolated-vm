// https://streams.spec.whatwg.org/#qs
import type { CallbackFunction } from "@isolated-vm/runtime/webidl";
import { NumberConstructor, NumberIsNaN, RangeErrorConstructor, ReflectApply, TypeErrorConstructor, kEmptyObject } from "@isolated-vm/intrinsics";
import { assertCallbackFunction, assertDictionary, defineInterface, requireArguments } from "@isolated-vm/runtime/webidl";

// https://streams.spec.whatwg.org/#callbackdef-queuingstrategysize
/** @internal */
export type QueuingStrategySize<Type = unknown> = (chunk: Type) => number;

/** @internal */
export type SizeAlgorithm = QueuingStrategySize;

// https://streams.spec.whatwg.org/#dictdef-queuingstrategy
/** @internal */
export class QueuingStrategy {
	readonly #highWaterMark: number | undefined;
	readonly #size: CallbackFunction | undefined;

	constructor(value: unknown) {
		assertDictionary(value);
		const { highWaterMark, size } = value ?? kEmptyObject;
		this.#highWaterMark = highWaterMark === undefined ? undefined : NumberConstructor(highWaterMark);
		assertCallbackFunction(size);
		this.#size = size;
	}

	// https://streams.spec.whatwg.org/#validate-and-normalize-high-water-mark
	extractHighWaterMark(defaultHWM: number): number {
		const highWaterMark = this.#highWaterMark;
		if (highWaterMark === undefined) {
			return defaultHWM;
		} else if (NumberIsNaN(highWaterMark) || highWaterMark < 0) {
			throw new RangeErrorConstructor("Invalid highWaterMark");
		}
		return highWaterMark;
	}

	// https://streams.spec.whatwg.org/#make-size-algorithm-from-size-function
	extractSizeAlgorithm(): SizeAlgorithm {
		const size = this.#size;
		if (size === undefined) {
			return () => 1;
		}
		return chunk => NumberConstructor(ReflectApply(size, undefined, [ chunk ]));
	}
}

// https://streams.spec.whatwg.org/#dictdef-queuingstrategyinit
function convertQueuingStrategyInit(value: unknown) {
	assertDictionary(value);
	const { highWaterMark } = value ?? kEmptyObject;
	if (highWaterMark === undefined) {
		throw new TypeErrorConstructor("Required member 'highWaterMark' is missing");
	}
	return NumberConstructor(highWaterMark);
}

// https://streams.spec.whatwg.org/#byte-length-queuing-strategy-size-function
const { size: byteLengthSizeFunction }: { size: QueuingStrategySize<ArrayBufferView> } = { size: chunk => chunk.byteLength };
// https://streams.spec.whatwg.org/#count-queuing-strategy-size-function
const { size: countSizeFunction }: { size: QueuingStrategySize } = { size: () => 1 };

// https://streams.spec.whatwg.org/#blqs-class
export class ByteLengthQueuingStrategy {
	readonly #highWaterMark: number;

	static { defineInterface(this); }

	constructor(init: unknown) {
		requireArguments(arguments.length, 1);
		this.#highWaterMark = convertQueuingStrategyInit(init);
	}

	get highWaterMark(): number { return this.#highWaterMark; }

	get size(): QueuingStrategySize<ArrayBufferView> {
		// eslint-disable-next-line @typescript-eslint/no-unused-expressions
		this.#highWaterMark;
		return byteLengthSizeFunction;
	}
}

// https://streams.spec.whatwg.org/#cqs-class
export class CountQueuingStrategy {
	readonly #highWaterMark: number;

	static { defineInterface(this); }

	constructor(init: unknown) {
		requireArguments(arguments.length, 1);
		this.#highWaterMark = convertQueuingStrategyInit(init);
	}

	get highWaterMark(): number { return this.#highWaterMark; }

	get size(): QueuingStrategySize {
		// eslint-disable-next-line @typescript-eslint/no-unused-expressions
		this.#highWaterMark;
		return countSizeFunction;
	}
}
