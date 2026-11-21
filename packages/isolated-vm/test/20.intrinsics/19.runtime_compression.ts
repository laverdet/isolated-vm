import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as zlib from "node:zlib";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

type AnyFunction = (...args: unknown[]) => unknown;
type Format = "deflate" | "deflate-raw" | "gzip";

const formats: Format[] = [ "deflate", "deflate-raw", "gzip" ];
const sample = "hello hello hello hello hello";

// zlib comes from the host process, and bun doesn't export it
const isBun =
	// @ts-expect-error
	process.isBun !== undefined;
const options = { skip: isBun && "zlib is not available" };

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

/** Returns a reference to a function defined with the runtime `Compressor` & `Decompressor` classes in scope */
async function makeRuntimeFunction(agent: Agent, realm: Realm, body: string) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { Compressor, Decompressor } from "isolated-vm:runtime";
		const bytes = array => Uint8Array.from(array);
		const text = string => Uint8Array.from(string, character => character.charCodeAt(0));
		const attempt = fn => { try { return fn(); } catch (error) { return { error: error.constructor.name }; } };
		const views = buffers => buffers.map(buffer => new Uint8Array(buffer));
		// Writes each chunk, flushes, and concatenates the output
		const run = (codec, chunks) => {
			const outputs = [ ...chunks.flatMap(chunk => views(codec.write(chunk).output)), ...views(codec.flush()) ];
			const result = new Uint8Array(outputs.reduce((total, output) => total + output.length, 0));
			outputs.reduce((offset, output) => (result.set(output, offset), offset + output.length), 0);
			return result;
		};
		globalThis.fn = () => { ${body} };
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime })));
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("fn")) as Reference<AnyFunction>;
}

await test("round trip", options, async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const compressed = Object.fromEntries(formats.map(format => [ format, hostCompress(format, sample) ]));
	const fn = await makeRuntimeFunction(agent, realm, `
		const chunks = [ "hello ", "hello hello ", "", "hello hello" ].map(text);
		const compressed = ${JSON.stringify(compressed)};
		// Enough output to fill several chunks, which double in size
		const large = new Uint8Array(1 << 20).map((_, ii) => (ii * 7 + (ii >> 10)) & 0xff);
		const output = run(new Decompressor("gzip"), [ run(new Compressor("gzip"), [ large.subarray(0, 300000), large.subarray(300000) ]) ]);
		return {
			large: output.length === large.length && output.every((byte, ii) => byte === large[ii]),
			...Object.fromEntries(${JSON.stringify(formats)}.map(format => {
				const input = bytes(compressed[format]);
				return [ format, {
					compressed: Array.from(run(new Compressor(format), chunks)),
					whole: String.fromCharCode(...run(new Decompressor(format), [ input ])),
					split: String.fromCharCode(...run(new Decompressor(format), Array.from(input, byte => bytes([ byte ])))),
				} ];
			})),
		};`);
	const result = expectComplete(await fn.invoke([])) as { large: boolean } & Record<Format, { compressed: number[]; whole: string; split: string }>;
	assert.equal(result.large, true);
	for (const format of formats) {
		assert.equal(hostDecompress(format, result[format].compressed), sample, format);
		assert.equal(result[format].whole, sample, format);
		assert.equal(result[format].split, sample, format);
	}
});

await test("decompressor stops short at an error", options, async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const gzip = bytes(${JSON.stringify(hostCompress("gzip", sample))});
		const input = bytes([ ...gzip, 0 ]);
		// Output which precedes the error is delivered, the count stops at the error, and the rest throws
		const codec = new Decompressor("gzip");
		const { output, bytesRead } = codec.write(input);
		return {
			output: String.fromCharCode(...views(output).flatMap(view => Array.from(view))),
			bytesRead: bytesRead === gzip.length,
			rest: attempt(() => codec.write(input.subarray(bytesRead))),
			// An empty write is always fine
			empty: codec.write(bytes([])).output.length,
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		output: sample,
		bytesRead: true,
		rest: { error: "TypeError" },
		empty: 0,
	});
});

await test("decompressor output fills a chunk exactly", options, async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	// A sync flush ends the input without ending the stream, so the write runs out of input just as its first chunk
	// fills, and zlib has nothing to do for the next one.
	const size = 1 << 14;
	const synced = Array.from(zlib.deflateSync(Buffer.alloc(size, 0x61), { finishFlush: zlib.constants.Z_SYNC_FLUSH }));
	const fn = await makeRuntimeFunction(agent, realm, `
		const input = bytes(${JSON.stringify(synced)});
		const { output, bytesRead } = new Decompressor("deflate").write(input);
		return {
			chunks: output.length,
			length: views(output).reduce((total, view) => total + view.length, 0),
			bytesRead,
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		chunks: 1,
		length: size,
		bytesRead: synced.length,
	});
});
