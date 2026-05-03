import * as fs from "node:fs";
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

// eslint-disable-next-line @typescript-eslint/no-inferrable-types
const path: string = `@isolated-vm/experimental-${backendPlatform}`;
export default path;
