// @ts-check
import { createRequire } from "node:module";
import path from "@isolated-vm/experimental/backend_path";

// Import compiled module
const require = createRequire(import.meta.url);

/** @type {typeof import("#backend_v8")} */
const cjs = require(path);

// Exports must be enumerated because this imports from the native module which cannot declare ESM
// exports.
export const initialize = cjs.initialize;
export const Agent = cjs.Agent;
export const Module = cjs.Module;
export const NativeModule = cjs.NativeModule;
export const Realm = cjs.Realm;
export const Script = cjs.Script;
