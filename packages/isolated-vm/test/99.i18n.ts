import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { unsafeEvalAsString } from "./fixtures.js";

await test("i18n locales", async () => {
	await using agent = await ivm.Agent.create();
	const supported = await unsafeEvalAsString(agent, () =>
		Intl.NumberFormat.supportedLocalesOf([ "ar-EG", "fa-IR", "zh-Hans", "hi-IN", "th-TH", "km-KH" ]));
	// no "km-KH"
	assert.deepEqual(supported, [ "ar-EG", "fa-IR", "zh-Hans", "hi-IN", "th-TH" ]);
});

await test("i18n format", async () => {
	await using agent = await ivm.Agent.create();
	const german = await unsafeEvalAsString(agent, () => new Intl.NumberFormat("de-DE").format(1234567.89));
	assert.equal(german, "1.234.567,89");
});
