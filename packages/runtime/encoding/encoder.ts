// https://encoding.spec.whatwg.org/#interface-textencoder
import { defineInterface, requireArguments } from "@isolated-vm/runtime/webidl";
import { StringWriter } from "isolated-vm:runtime";

// https://encoding.spec.whatwg.org/#dictdef-textencoderencodeintoresult
export interface TextEncoderEncodeIntoResult {
	read: number;
	written: number;
}

export class TextEncoder {
	readonly #writer = new StringWriter();

	static { defineInterface(this); }

	get encoding(): string {
		// eslint-disable-next-line @typescript-eslint/no-unused-expressions
		this.#writer;
		return "utf-8";
	}

	// https://encoding.spec.whatwg.org/#dom-textencoder-encode
	encode(input?: string): Uint8Array;
	encode(input: unknown = ""): Uint8Array {
		// eslint-disable-next-line @typescript-eslint/restrict-template-expressions
		return this.#writer.write(`${input}`, true).destination;
	}

	// https://encoding.spec.whatwg.org/#dom-textencoder-encodeinto
	encodeInto(source: string, destination: Uint8Array): TextEncoderEncodeIntoResult;
	encodeInto(source: unknown, destination: unknown): TextEncoderEncodeIntoResult {
		const writer = this.#writer;
		requireArguments(arguments.length, 2);
		// eslint-disable-next-line @typescript-eslint/restrict-template-expressions
		const { read, written } = writer.write(`${source}`, true, destination as Uint8Array);
		return { read, written };
	}
}
