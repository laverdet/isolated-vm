/* eslint-disable @typescript-eslint/no-require-imports */
import fs = require("node:fs");
import process = require("node:process");

const arch = function() {
	const { arch } = process;
	// eslint-disable-next-line @typescript-eslint/switch-exhaustiveness-check
	switch (arch) {
		case "arm64":
		case "x64":
			return arch;
		default:
			throw new Error(`Unsupported architecture: ${arch}`);
	}
}();

/** @internal */
const host = function() {
	const { platform } = process;
	// eslint-disable-next-line @typescript-eslint/switch-exhaustiveness-check
	switch (platform) {
		case "linux": {
			const libc = function() {
				try {
					if (fs.readFileSync("/usr/bin/ldd", "latin1").includes("ld-musl-")) {
						return "musl" as const;
					}
				} catch {}
				return "gnu" as const;
			}();
			return `linux-${arch}-${libc}` as const;
		}
		case "darwin":
		case "win32":
			return `${platform}-${arch}` as const;
		default:
			throw new Error(`Unsupported platform: ${platform}`);
	}
}();

export = host;
