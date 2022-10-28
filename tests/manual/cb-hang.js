// Hangs within a couple of minutes
// https://github.com/laverdet/isolated-vm/issues/321
const ivm = require('isolated-vm');

const vm = new ivm.Isolate({
    memoryLimit: 2048,
});

const cb = new ivm.Callback(() => '123');

async function run() {
    const context = await vm.createContext();

    await context.global.set('cb', cb);

    const result = await context.eval('cb()', {
        timeout: 5000,
    });

    context.release();

    return result;
}

const loop = () => {
    let current = 0;
    const max = 1000;
    for (let i = 0; i < max; i++) {
        run().then(() => {
            console.log(++current);

            if (current === max) {
                setTimeout(loop, 500);
            }
        });
    }
};
loop();
