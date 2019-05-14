'use strict';
module.exports = {
	V8_AT_LEAST(major, minor, patch) {
		let versions = process.versions.v8.split(/\./g).map(Number);
		return (major < versions[0]) ||
			(major === versions[0] && minor < versions[1]) ||
			(major === versions[0] && minor === versions[1] && patch <= versions[2]);
	},
};
