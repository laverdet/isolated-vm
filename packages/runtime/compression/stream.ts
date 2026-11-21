/* eslint-disable @typescript-eslint/prefer-for-of */
// https://compression.spec.whatwg.org/
import type { ReadableStream } from "@isolated-vm/runtime/streams/readable";
import type { TransformStreamDefaultController } from "@isolated-vm/runtime/streams/transform";
import type { WritableStream } from "@isolated-vm/runtime/streams/writable";
import type { BufferSource } from "isolated-vm:runtime";
import { ArrayBufferIsView, Uint8ArrayConstructor } from "@isolated-vm/intrinsics";
import { createTransformStream } from "@isolated-vm/runtime/streams/transform";
import { defineInterface, requireArguments } from "@isolated-vm/runtime/webidl";
import { Compressor, Decompressor } from "isolated-vm:runtime";

// https://compression.spec.whatwg.org/#enumdef-compressionformat
export type CompressionFormat = "deflate" | "deflate-raw" | "gzip";

/** Enqueues each buffer of `output` as a `Uint8Array` */
function enqueue(controller: TransformStreamDefaultController, output: ArrayBuffer[]) {
	for (let ii = 0; ii < output.length; ++ii) {
		controller.enqueue(new Uint8ArrayConstructor(output[ii]!));
	}
}

/** The bytes of `source` from `offset` */
function remainder(source: BufferSource, offset: number): Uint8Array<ArrayBuffer> {
	return ArrayBufferIsView(source)
		? new Uint8ArrayConstructor(source.buffer, source.byteOffset + offset, source.byteLength - offset)
		: new Uint8ArrayConstructor(source, offset);
}

/** Wraps a `Compressor` or `Decompressor` in a transform stream */
function createCodecStream(codec: Compressor | Decompressor) {
	return createTransformStream({
		// https://compression.spec.whatwg.org/#compress-and-enqueue-a-chunk
		// https://compression.spec.whatwg.org/#decompress-and-enqueue-a-chunk
		transform(chunk: unknown, controller: TransformStreamDefaultController) {
			let input = chunk as BufferSource;
			while (true) {
				const { output, bytesRead } = codec.write(input);
				enqueue(controller, output);
				// A short count means the write stopped at an error, which the rest of the input throws
				if (bytesRead === input.byteLength) {
					break;
				}
				input = remainder(input, bytesRead);
			}
		},
		// https://compression.spec.whatwg.org/#compress-flush-and-enqueue
		// https://compression.spec.whatwg.org/#decompress-flush-and-enqueue
		flush(controller: TransformStreamDefaultController) {
			enqueue(controller, codec.flush());
		},
	});
}

// https://compression.spec.whatwg.org/#compression-stream
export class CompressionStream {
	readonly #readable: ReadableStream;
	readonly #writable: WritableStream;

	static { defineInterface(this); }

	// https://compression.spec.whatwg.org/#dom-compressionstream-compressionstream
	constructor(format: CompressionFormat);
	constructor(format: unknown) {
		requireArguments(arguments.length, 1);
		// eslint-disable-next-line @typescript-eslint/restrict-template-expressions
		const { readable, writable } = createCodecStream(new Compressor(`${format}`));
		this.#readable = readable;
		this.#writable = writable;
	}

	get readable(): ReadableStream { return this.#readable; }
	get writable(): WritableStream { return this.#writable; }
}

// https://compression.spec.whatwg.org/#decompression-stream
export class DecompressionStream {
	readonly #readable: ReadableStream;
	readonly #writable: WritableStream;

	static { defineInterface(this); }

	// https://compression.spec.whatwg.org/#dom-decompressionstream-decompressionstream
	constructor(format: CompressionFormat);
	constructor(format: unknown) {
		requireArguments(arguments.length, 1);
		// eslint-disable-next-line @typescript-eslint/restrict-template-expressions
		const { readable, writable } = createCodecStream(new Decompressor(`${format}`));
		this.#readable = readable;
		this.#writable = writable;
	}

	get readable(): ReadableStream { return this.#readable; }
	get writable(): WritableStream { return this.#writable; }
}
