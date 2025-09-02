import * as fs from "node:fs";
import { createRequire } from "node:module";
import { platform } from "node:process";

export type * from "backend_v8.node";

// Detect libc version
const backendPlatform = function() {
	if (platform === "linux") {
		try {
			if (fs.readFileSync("/usr/bin/ldd", "latin1").includes("ld-musl-")) {
				return "linux-musl";
			}
		} catch {}
		return "linux-gnu";
	} else {
		return platform;
	}
}();

// Import compiled module
const require = createRequire(import.meta.url);
// eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
const backend: typeof import("backend_v8.node")["default"] = require(`./backend_v8/${backendPlatform}-${process.arch}/backend_v8.node`);

// Exports must be enumerated because this imports from the native module which cannot declare ESM
// exports.
export const compileModule: typeof backend.compileModule = backend.compileModule;
export const compileScript: typeof backend.compileScript = backend.compileScript;
export const createAgent: typeof backend.createAgent = backend.createAgent;
export const createCapability: typeof backend.createCapability = backend.createCapability;
export const createRealm: typeof backend.createRealm = backend.createRealm;
export const evaluateModule: typeof backend.evaluateModule = backend.evaluateModule;
export const instantiateRuntime: typeof backend.instantiateRuntime = backend.instantiateRuntime;
export const linkModule: typeof backend.linkModule = backend.linkModule;
export const runScript: typeof backend.runScript = backend.runScript;
