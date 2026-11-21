import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, Capability, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeCompositeLinker, makeDirectResolver, makeFileSystemCompilationLoader, makeLinker, makeLocalResolver, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

/** Installs the streams and compression runtime in a fresh realm and runs `body`, which reports via `notify()`. */
async function runCompression(body: string) {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const resolvers = Promise.withResolvers<unknown>();
	const capabilities = makeStaticLoader({
		"isolated-vm:runtime": expect(await realm.instantiateRuntime()),
		// `AbortSignal.timeout` links against the timers capability, though nothing here schedules
		"isolated-vm:capability/timers": expect(await realm.createCapability(
			{
				initialize: Capability.void(() => {}),
				schedule: Capability.void(() => {}),
			},
			{ origin: "isolated-vm:capability/timers" })),
		"notify-test": expect(await realm.createCapability(
			{
				default: Capability.void((message: unknown) => { resolvers.resolve(message); }),
			},
			{ origin: "notify-test" })),
	});
	const linker = makeCompositeLinker(
		makeLinker(makeDirectResolver(), capabilities),
		makeLinker(makeLocalResolver(import.meta), makeFileSystemCompilationLoader(agent)));
	const module = expectComplete(await agent.compileModule(`
		import "@isolated-vm/intrinsics/install";
		import "@isolated-vm/runtime/dom/install/event";
		import "@isolated-vm/runtime/dom/install/abort";
		import "@isolated-vm/runtime/streams/install";
		import "@isolated-vm/runtime/compression/install";
		import notify from "notify-test";
		const text = string => Uint8Array.from(string, character => character.charCodeAt(0));
		// Pipes \`chunks\` through \`transform\` and collects the output
		const collect = async (chunks, transform) => {
			const source = new ReadableStream({
				start(controller) {
					chunks.forEach(chunk => controller.enqueue(chunk));
					controller.close();
				},
			});
			const output = [];
			for await (const chunk of source.pipeThrough(transform)) {
				output.push(chunk);
			}
			return output;
		};
		(async () => { ${body} })().then(result => notify(JSON.stringify(result)), error => notify(JSON.stringify({ error: String(error) })));
	`));
	await module.link(realm, linker);
	await module.evaluate(realm);
	const timeout = setTimeout(() => resolvers.reject(new Error("Timed out")), 5000);
	try {
		return JSON.parse(String(await resolvers.promise)) as unknown;
	} finally {
		clearTimeout(timeout);
	}
}

await test("compression: round trip", async () => {
	const result = await runCompression(`
		const chunks = [ "hello ", "", "hello ", "world" ].map(text);
		const results = {};
		for (const format of [ "deflate", "deflate-raw", "gzip" ]) {
			const compressed = await collect(chunks, new CompressionStream(format));
			const decompressed = await collect(compressed, new DecompressionStream(format));
			results[format] = {
				types: [ ...compressed, ...decompressed ].every(chunk => chunk.constructor === Uint8Array && !chunk.buffer.resizable),
				// A raw stream has no header
				header: format === "deflate-raw" ? null : compressed[0][0],
				text: String.fromCharCode(...decompressed.flatMap(chunk => [ ...chunk ])),
			};
		}
		return results;`);
	assert.deepEqual(result, {
		deflate: { types: true, header: 0x78, text: "hello hello world" },
		"deflate-raw": { types: true, header: null, text: "hello hello world" },
		gzip: { types: true, header: 0x1f, text: "hello hello world" },
	});
});

await test("decompression: invalid data errors the stream", async () => {
	const result = await runCompression(`
		const gzip = (await collect([ text("hello") ], new CompressionStream("gzip"))).flatMap(chunk => [ ...chunk ]);
		const attempt = async (format, chunks) => {
			const stream = new DecompressionStream(format);
			const writer = stream.writable.getWriter();
			const reader = stream.readable.getReader();
			const writes = chunks.map(chunk => writer.write(chunk).catch(() => {}));
			const close = writer.close().catch(() => {});
			const output = [];
			try {
				while (true) {
					const { done, value } = await reader.read();
					if (done) {
						break;
					}
					output.push(...value);
				}
			} catch (error) {
				return error.constructor.name;
			} finally {
				await Promise.all([ ...writes, close ]);
			}
			return String.fromCharCode(...output);
		};
		return {
			garbage: await attempt("deflate", [ Uint8Array.from([ 1, 2, 3, 4 ]) ]),
			// The error follows valid output, which the short count from the native leaves for a second write
			trailing: await attempt("gzip", [ Uint8Array.from([ ...gzip, 0 ]) ]),
		};`);
	assert.deepEqual(result, {
		garbage: "TypeError",
		trailing: "TypeError",
	});
});
