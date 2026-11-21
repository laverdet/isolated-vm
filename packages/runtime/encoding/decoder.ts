// https://encoding.spec.whatwg.org/#interface-textdecoder
import type { AllowSharedBufferSource } from "isolated-vm:runtime";
import { BooleanConstructor, TypeErrorConstructor, kEmptyObject } from "@isolated-vm/intrinsics";
import { assertDictionary, defineInterface } from "@isolated-vm/runtime/webidl";
import { StringReader } from "isolated-vm:runtime";

// A missing `input` decodes as an empty byte sequence
const kEmptyBytes = new Uint8Array(0);

// https://encoding.spec.whatwg.org/#dictdef-textdecoderoptions
export interface TextDecoderOptions {
	fatal?: boolean;
	ignoreBOM?: boolean;
}

// https://encoding.spec.whatwg.org/#dictdef-textdecodeoptions
export interface TextDecodeOptions {
	stream?: boolean;
}

export class TextDecoder {
	#encoding: string | undefined;
	readonly #reader: StringReader;
	readonly #fatal: boolean;
	readonly #ignoreBOM: boolean;

	static { defineInterface(this); }

	// https://encoding.spec.whatwg.org/#dom-textdecoder
	constructor(label?: string, options?: TextDecoderOptions);
	constructor(label: unknown = "utf-8", options: unknown = null) {
		// eslint-disable-next-line @typescript-eslint/restrict-template-expressions
		const labelString = `${label}`;
		// https://encoding.spec.whatwg.org/#textdecoderoptions
		assertDictionary(options);
		const { fatal, ignoreBOM } = options ?? kEmptyObject;
		this.#fatal = BooleanConstructor(fatal);
		this.#ignoreBOM = BooleanConstructor(ignoreBOM);
		// The runtime resolves the label, and throws a `RangeError` for an unknown one
		this.#reader = new StringReader(labelString, this.#fatal, this.#ignoreBOM);
	}

	get encoding(): string { return this.#encoding ??= this.#reader.encoding; }
	get fatal(): boolean { return this.#fatal; }
	get ignoreBOM(): boolean { return this.#ignoreBOM; }

	// https://encoding.spec.whatwg.org/#dom-textdecoder-decode
	decode(input?: AllowSharedBufferSource, options?: TextDecodeOptions): string;
	decode(input: unknown = kEmptyBytes, options: unknown = null): string {
		const reader = this.#reader;
		// https://encoding.spec.whatwg.org/#textdecodeoptions
		assertDictionary(options);
		const { stream } = options ?? kEmptyObject;
		// The runtime rejects anything but a buffer source, and keeps the stream state
		const output = reader.read(input as AllowSharedBufferSource, !stream);
		if (output === undefined) {
			throw new TypeErrorConstructor(`The encoded data was not valid for encoding ${reader.encoding}`);
		}
		return output;
	}
}
