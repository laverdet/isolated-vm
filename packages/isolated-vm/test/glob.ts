import { glob } from "node:fs/promises";

// Bun's `node:test` implementation is broken
// https://github.com/oven-sh/bun/issues/23077
// https://github.com/oven-sh/bun/issues/5090 (red herring)
const cwd = new URL(".", import.meta.url);
for await (const file of glob("**/*.ts", { cwd })) {
	if (/^[0-9]{2}/.test(file)) {
		await import(new URL(file, cwd).href);
	}
}
