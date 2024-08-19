import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
// eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
const backend: typeof import("./libivm.backend_v8.node")["default"] = require("./libivm.backend_v8.node");

export type * from "./libivm.backend_v8.node";

// Exports must be enumerated because this imports from the native module which cannot declare ESM
// exports.
export const compileScript: typeof backend.compileScript = backend.compileScript;
export const createAgent: typeof backend.createAgent = backend.createAgent;
export const createRealm: typeof backend.createRealm = backend.createRealm;
export const runScript: typeof backend.runScript = backend.runScript;
