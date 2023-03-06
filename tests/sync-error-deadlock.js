const ivm = require('isolated-vm');

const isolate = new ivm.Isolate();

const task = async () => {
    const context = await isolate.createContext();

    await context.global.set("getProm", () => {
        throw new Error('test')
    }, { reference: true });

    await context.global.set("pass", () => {
        console.log('pass');
    }, { reference: true });


    timeout = setTimeout(() => {
        console.error('Deadlock check time out');
        process.exitCode = 1;
        process.exit(1);
    }, 1000);

    await context.eval(`
        try {
            getProm.applySync();
        } catch {
            pass.applySync();
        }
    `);

    clearTimeout(timeout);

    context.release();
}

task().catch(e => {
    console.error(e);
    process.exitCode = 1;
    process.exit();
});
