/* eslint-disable @typescript-eslint/no-require-imports */
import host = require("#host_cjs");

// All hosts enumerated to assist with bundlers. This file can be replaced with a condition.
module.exports = function(): unknown {
	switch (host) {
		case "darwin-arm64": return require("@isolated-vm/experimental-darwin-arm64");
		case "darwin-x64": return require("@isolated-vm/experimental-darwin-x64");
		case "linux-arm64-gnu": return require("@isolated-vm/experimental-linux-arm64-gnu");
		case "linux-arm64-musl": return require("@isolated-vm/experimental-linux-arm64-musl");
		case "linux-x64-musl": return require("@isolated-vm/experimental-linux-x64-musl");
		case "linux-x64-gnu": return require("@isolated-vm/experimental-linux-x64-gnu");
		case "win32-x64": return require("@isolated-vm/experimental-win32-x64");
		case "win32-arm64":
		default: throw new Error(`Unsupported platform: ${host}`);
	}
}();
