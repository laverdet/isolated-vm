import * as assert from "node:assert/strict";
import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("localCompare", async () => {
	await using vm = await makeTestAgent();
	const result = await vm.unsafeScript(() => "a".localeCompare("b"));
	assert.equal(result, -1);
});

await test("i18n locales", async () => {
	await using vm = await makeTestAgent();
	const supported = await vm.unsafeScript(() =>
		Intl.NumberFormat.supportedLocalesOf([ "ar-EG", "fa-IR", "zh-Hans", "hi-IN", "th-TH", "km-KH" ]));
	// no "km-KH"
	assert.deepEqual(supported, [ "ar-EG", "fa-IR", "zh-Hans", "hi-IN", "th-TH" ]);
});

await test("i18n format", async () => {
	await using vm = await makeTestAgent();
	const german = await vm.unsafeScript(() => new Intl.NumberFormat("de-DE").format(1234567.89));
	assert.equal(german, "1.234.567,89");
});
