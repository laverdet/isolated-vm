// https://encoding.spec.whatwg.org/#interface-mixin-genericTransformStream
import type { TextDecoderOptions } from "@isolated-vm/runtime/encoding/decoder";
import type { ReadableStream } from "@isolated-vm/runtime/streams/readable";
import type { TransformStreamDefaultController, Transformer } from "@isolated-vm/runtime/streams/transform";
import type { WritableStream } from "@isolated-vm/runtime/streams/writable";
import type { BufferSource } from "isolated-vm:runtime";
import { TypeErrorConstructor } from "@isolated-vm/intrinsics";
import { TextDecoder } from "@isolated-vm/runtime/encoding/decoder";
import { TransformStream, getTransformStreamInternal } from "@isolated-vm/runtime/streams/transform";
import { defineInterface } from "@isolated-vm/runtime/webidl";
import { StringWriter } from "isolated-vm:runtime";

function makeTransformStream(transformer: Transformer) {
	const { readable, writable } = getTransformStreamInternal(new TransformStream(transformer));
	return { readable, writable };
}

// https://encoding.spec.whatwg.org/#textdecoderstream
export class TextDecoderStream {
	readonly #decoder: TextDecoder;
	readonly #readable: ReadableStream;
	readonly #writable: WritableStream;

	static { defineInterface(this); }

	// https://encoding.spec.whatwg.org/#dom-textdecoderstream
	constructor(label?: string, options?: TextDecoderOptions);
	constructor(label: unknown = "utf-8", options: unknown = null) {
		const decoder = this.#decoder = new TextDecoder(label as string, options as TextDecoderOptions);
		const { readable, writable } = makeTransformStream({
			// https://encoding.spec.whatwg.org/#decode-and-enqueue-a-chunk
			transform(chunk: unknown, controller: TransformStreamDefaultController) {
				// `decode` accepts `undefined` as a missing argument, but a chunk must be a buffer source
				if (chunk === undefined) {
					throw new TypeErrorConstructor("Provided value is not of type '(ArrayBuffer or ArrayBufferView)'");
				}
				// The decoder rejects anything but a buffer source
				const output = decoder.decode(chunk as BufferSource, { stream: true });
				if (output !== "") {
					controller.enqueue(output);
				}
			},
			// https://encoding.spec.whatwg.org/#flush-and-enqueue
			flush(controller: TransformStreamDefaultController) {
				const output = decoder.decode();
				if (output !== "") {
					controller.enqueue(output);
				}
			},
		});
		this.#readable = readable;
		this.#writable = writable;
	}

	get encoding(): string { return this.#decoder.encoding; }
	get fatal(): boolean { return this.#decoder.fatal; }
	get ignoreBOM(): boolean { return this.#decoder.ignoreBOM; }
	get readable(): ReadableStream { return this.#readable; }
	get writable(): WritableStream { return this.#writable; }
}

// https://encoding.spec.whatwg.org/#textencoderstream
export class TextEncoderStream {
	readonly #readable: ReadableStream;
	readonly #writable: WritableStream;

	static { defineInterface(this); }

	// https://encoding.spec.whatwg.org/#dom-textencoderstream
	constructor() {
		// https://encoding.spec.whatwg.org/#textencoderstream-pending-high-surrogate
		const writer = new StringWriter();
		const { readable, writable } = makeTransformStream({
			// https://encoding.spec.whatwg.org/#encode-and-enqueue-a-chunk
			transform(chunk: unknown, controller: TransformStreamDefaultController) {
				// eslint-disable-next-line @typescript-eslint/restrict-template-expressions
				const { written, destination } = writer.write(`${chunk}`, false);
				if (written > 0) {
					controller.enqueue(destination);
				}
			},
			// https://encoding.spec.whatwg.org/#encode-and-flush
			flush(controller: TransformStreamDefaultController) {
				const { written, destination } = writer.write("", true);
				if (written > 0) {
					controller.enqueue(destination);
				}
			},
		});
		this.#readable = readable;
		this.#writable = writable;
	}

	// eslint-disable-next-line @typescript-eslint/class-literal-property-style
	get encoding(): string { return "utf-8"; }
	get readable(): ReadableStream { return this.#readable; }
	get writable(): WritableStream { return this.#writable; }
}
