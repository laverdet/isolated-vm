import type { Compressor, Decompressor } from "@isolated-vm/experimental/runtime";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as zlib from "node:zlib";
import { expectComplete, isBun, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

type Format = "deflate" | "deflate-raw" | "gzip";

const formats: Format[] = [ "deflate", "deflate-raw", "gzip" ];
const sample = "hello hello hello hello hello";

// zlib comes from the host process, and bun doesn't export it
const hostZlib = { skip: isBun && "zlib is not available" };

/** Compresses on the host with `node:zlib` */
function hostCompress(format: Format, input: string): number[] {
	const compress = { deflate: zlib.deflateSync, "deflate-raw": zlib.deflateRawSync, gzip: zlib.gzipSync }[format];
	return Array.from(compress(input));
}

/** Decompresses on the host with `node:zlib` */
function hostDecompress(format: Format, input: number[]): string {
	const decompress = { deflate: zlib.inflateSync, "deflate-raw": zlib.inflateRawSync, gzip: zlib.gunzipSync }[format];
	return decompress(Uint8Array.from(input)).toString("latin1");
}

await test("round trip", hostZlib, async () => {
	await using vm = await makeTestAgent();
	const compressed = Object.fromEntries(
		formats.map(format => [ format, hostCompress(format, sample) ])) as Record<Format, number[]>;
	const fn = await vm.defineFunction((compressed: Record<Format, number[]>, formats: Format[]) => {
		const text = (string: string) => Uint8Array.from(string, character => character.charCodeAt(0));
		// Writes each chunk, flushes, and concatenates the output
		const run = (codec: Compressor | Decompressor, chunks: Uint8Array<ArrayBuffer>[]) => {
			const views = (buffers: ArrayBuffer[]) => buffers.map(buffer => new Uint8Array(buffer));
			const outputs = [ ...chunks.flatMap(chunk => views(codec.write(chunk).output)), ...views(codec.flush()) ];
			const result = new Uint8Array(outputs.reduce((total, output) => total + output.length, 0));
			let offset = 0;
			for (const output of outputs) {
				result.set(output, offset);
				offset += output.length;
			}
			return result;
		};
		const chunks = [ "hello ", "hello hello ", "", "hello hello" ].map(text);
		// Enough output to fill several chunks, which double in size
		const large = new Uint8Array(1 << 20).map((_, ii) => (ii * 7 + (ii >> 10)) & 0xff);
		const gzipped = run(new rt.Compressor("gzip"), [ large.subarray(0, 300000), large.subarray(300000) ]);
		const output = run(new rt.Decompressor("gzip"), [ gzipped ]);
		return {
			large: output.length === large.length && output.every((byte, ii) => byte === large[ii]),
			formats: Object.fromEntries(formats.map(format => {
				const input = Uint8Array.from(compressed[format]);
				return [ format, {
					compressed: Array.from(run(new rt.Compressor(format), chunks)),
					whole: String.fromCharCode(...run(new rt.Decompressor(format), [ input ])),
					split: String.fromCharCode(...run(
						new rt.Decompressor(format),
						Array.from(input, byte => Uint8Array.of(byte)))),
				} ];
			})) as Record<Format, { compressed: number[]; whole: string; split: string }>,
		};
	});
	const result = expectComplete(await fn.invoke([ compressed, formats ]));
	assert.equal(result.large, true);
	for (const format of formats) {
		assert.equal(hostDecompress(format, result.formats[format].compressed), sample, format);
		assert.equal(result.formats[format].whole, sample, format);
		assert.equal(result.formats[format].split, sample, format);
	}
});

await test("decompressor stops short at an error", hostZlib, async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript((gzip: number[], sample: string) => {
		const input = Uint8Array.from([ ...gzip, 0 ]);
		// Output which precedes the error is delivered, the count stops at the error, and the rest throws
		const codec = new rt.Decompressor("gzip");
		const { output, bytesRead } = codec.write(input);
		assert.equal(String.fromCharCode(...output.flatMap(buffer => [ ...new Uint8Array(buffer) ])), sample);
		assert.equal(bytesRead, gzip.length);
		assert.throws(() => codec.write(input.subarray(bytesRead)), TypeError);
		// An empty write is always fine
		assert.equal(codec.write(Uint8Array.from([])).output.length, 0);
	}, hostCompress("gzip", sample), sample);
});

await test("decompressor output fills a chunk exactly", hostZlib, async () => {
	await using vm = await makeTestAgent();
	// A sync flush ends the input without ending the stream, so the write runs out of input just as its first chunk
	// fills, and zlib has nothing to do for the next one.
	const size = 1 << 14;
	const synced = Array.from(zlib.deflateSync(Buffer.alloc(size, 0x61), { finishFlush: zlib.constants.Z_SYNC_FLUSH }));
	await vm.unsafeScript((synced: number[], size: number) => {
		const { output, bytesRead } = new rt.Decompressor("deflate").write(Uint8Array.from(synced));
		assert.equal(output.length, 1);
		assert.equal(output.reduce((total, buffer) => total + buffer.byteLength, 0), size);
		assert.equal(bytesRead, synced.length);
	}, synced, size);
});
