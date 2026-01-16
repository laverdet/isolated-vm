// @ts-check
import * as fs from "node:fs";
import { createRequire } from "node:module";
import { arch, platform } from "node:process";

// Detect libc version
const backendPlatform = function() {
	if (platform === "linux") {
		try {
			if (fs.readFileSync("/usr/bin/ldd", "latin1").includes("ld-musl-")) {
				return `linux-${arch}-musl`;
			}
		} catch {}
		return `linux-${arch}-gnu`;
	} else {
		return `${platform}-${arch}`;
	}
}();

// Import compiled module
const require = createRequire(import.meta.url);

/** @type {typeof import("#backend_v8")} */
const cjs = require(`@isolated-vm/experimental-${backendPlatform}`);

// Exports must be enumerated because this imports from the native module which cannot declare ESM
// exports.
export const Agent = cjs.Agent;
export const Module = cjs.Module;
export const Realm = cjs.Realm;
export const Script = cjs.Script;
