// @ts-check
import * as fs from "node:fs";
import { createRequire } from "node:module";
import { platform } from "node:process";

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

/** @type {typeof import("#backend_v8")} */
const cjs = require(`./dist/backend_v8/${backendPlatform}-${process.arch}/backend_v8.node`);

// Exports must be enumerated because this imports from the native module which cannot declare ESM
// exports.
export const Agent = cjs.Agent;
export const Module = cjs.Module;
export const Realm = cjs.Realm;
export const Script = cjs.Script;
