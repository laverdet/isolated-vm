/* eslint-disable @typescript-eslint/no-require-imports */
import backend = require("#backend_cjs");

// Exports must be enumerated because this imports from the native module which cannot declare ESM
// exports.
const { Agent, Module, NativeModule, Realm, Script } = backend;
module.exports = { Agent, Module, NativeModule, Realm, Script };
