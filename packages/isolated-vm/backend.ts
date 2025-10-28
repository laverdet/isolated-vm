import type * as ivm from "backend_v8.node";
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
// eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
const backend: typeof import("backend_v8.node")["default"] = require(`./backend_v8/${backendPlatform}-${process.arch}/backend_v8.node`);

// Exports must be enumerated because this imports from the native module which cannot declare ESM
// exports.
export type Agent = ivm.Agent;
export const Agent: typeof ivm.Agent = backend.Agent;
export type Module = ivm.Module;
export const Module: typeof ivm.Module = backend.Module;
export type Realm = ivm.Realm;
export const Realm: typeof ivm.Realm = backend.Realm;
export type Script = ivm.Script;
export const Script: typeof ivm.Script = backend.Script;
