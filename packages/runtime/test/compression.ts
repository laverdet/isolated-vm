import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

/** `lib.dom` lacks `ReadableStream.from` */
type ReadableStreamFrom = <Type>(iterable: Iterable<Type>) => ReadableStream<Type>;

// The realm has the streams & compression runtime installed
const options = {
	packages: import.meta,
	install: `
		import "@isolated-vm/intrinsics/install";
		import "@isolated-vm/runtime/dom/install/event";
		import "@isolated-vm/runtime/dom/install/abort";
		import "@isolated-vm/runtime/streams/install";
		import "@isolated-vm/runtime/compression/install";`,
};

await test("compression: round trip", async () => {
	await using vm = await makeTestAgent(options);
	await vm.unsafeModule(async () => {
		const from = (ReadableStream as unknown as { from: ReadableStreamFrom }).from;
		const text = (string: string) => Uint8Array.from(string, character => character.charCodeAt(0));
		const chunks = [ "hello ", "", "hello ", "world" ].map(text);
		for (const format of [ "deflate", "deflate-raw", "gzip" ] as const) {
			const compressed = await Array.fromAsync(from(chunks).pipeThrough(new CompressionStream(format)));
			const decompressed = await Array.fromAsync(from(compressed).pipeThrough(new DecompressionStream(format)));
			for (const chunk of [ ...compressed, ...decompressed ]) {
				assert.equal(chunk.constructor, Uint8Array);
				assert.equal(chunk.buffer.resizable, false);
			}
			// A raw stream has no header
			if (format !== "deflate-raw") {
				assert.equal(compressed[0]?.[0], { deflate: 0x78, gzip: 0x1f }[format]);
			}
			assert.equal(String.fromCharCode(...decompressed.flatMap(chunk => [ ...chunk ])), "hello hello world");
		}
	});
});

await test("decompression: invalid data errors the stream", async () => {
	await using vm = await makeTestAgent(options);
	await vm.unsafeModule(async () => {
		const from = (ReadableStream as unknown as { from: ReadableStreamFrom }).from;
		const hello = Uint8Array.from("hello", character => character.charCodeAt(0));
		const compressed = await Array.fromAsync(from([ hello ]).pipeThrough(new CompressionStream("gzip")));
		const gzip = compressed.flatMap(chunk => [ ...chunk ]);
		const attempt = async (format: CompressionFormat, chunks: Uint8Array<ArrayBuffer>[]) => {
			const stream = new DecompressionStream(format);
			const writer = stream.writable.getWriter();
			const reader = stream.readable.getReader();
			const writes = chunks.map(chunk => writer.write(chunk).catch(() => {}));
			const close = writer.close().catch(() => {});
			try {
				// Drains the stream until it closes, or errors
				while (!(await reader.read()).done) { /* ... */ }
			} finally {
				await Promise.all([ ...writes, close ]);
			}
		};
		await assert.rejects(attempt("deflate", [ Uint8Array.from([ 1, 2, 3, 4 ]) ]), TypeError);
		// The error follows valid output, which the short count from the native leaves for a second write
		await assert.rejects(attempt("gzip", [ Uint8Array.from([ ...gzip, 0 ]) ]), TypeError);
	});
});
