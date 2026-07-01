import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent } from "@isolated-vm/experimental";
import { unsafeEvalAsString } from "@isolated-vm/experimental/test/fixtures";

await test("localCompare", async () => {
	await using agent = await Agent.create();
	const result = await unsafeEvalAsString(agent, () => "a".localeCompare("b"));
	assert.equal(result, -1);
});

await test("i18n locales", async () => {
	await using agent = await Agent.create();
	const supported = await unsafeEvalAsString(agent, () =>
		Intl.NumberFormat.supportedLocalesOf([ "ar-EG", "fa-IR", "zh-Hans", "hi-IN", "th-TH", "km-KH" ]));
	// no "km-KH"
	assert.deepEqual(supported, [ "ar-EG", "fa-IR", "zh-Hans", "hi-IN", "th-TH" ]);
});

await test("i18n format", async () => {
	await using agent = await Agent.create();
	const german = await unsafeEvalAsString(agent, () => new Intl.NumberFormat("de-DE").format(1234567.89));
	assert.equal(german, "1.234.567,89");
});
