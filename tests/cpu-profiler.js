const ivm = require('isolated-vm');
const assert = require('assert');

const isolate = new ivm.Isolate();

const context = isolate.createContextSync();

const profiler = isolate.createCpuProfiler();

profiler.startProfiling('foo', true);
profiler.setSamplingInterval(10);
profiler.setUsePreciseSampling(false);

context.evalSync(`
    function loopFn(${new Array(1024).fill(0).map((_, idx) => 'arg' + idx).join(',')}) {
        let i = 0;
        let result = 0;
        for (i = 0; i < arguments.length; i++) {
            result += arguments[i];
        }
        return result;
    }

    const foo = {};

    for (let i = 1024; i > 0; i--) {
        loopFn.bind(foo).call(new Array(i).fill(0).map((_, idx) => { 
            if (i % 2 === 0) {
                return idx + Math.random(i);
            } else {
                return idx + String(i);
            }
        }));
    }
`, {
    filename: 'foo.js'
});

const profile = profiler.stopProfiling('foo');

assert.equal(profile.title, 'foo', 'profile should have title `foo`');
assert.ok(profile.nodes.length > 0, 'profile should have node length > 0');
assert.ok(typeof profile.startTime === 'number', 'startTime should be a number');
assert.ok(JSON.stringify(profile).includes('loopFn'), 'loopFn should be in the result');
assert.ok(JSON.stringify(profile).includes('foo.js'), 'foo.js filename should be in the result');
assert.ok(profile.samples.length === profile.timeDeltas.length && profile.samples.length > 0, 'sample and time delta should have same length');

console.log('pass');


