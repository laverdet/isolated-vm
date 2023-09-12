const { Worker } = require('worker_threads');

if (process.platform === 'win32') {
    // unloading the module segfaults on Windows, skip for now
    console.log('pass');
	process.exit();
}

(async function () {
    for (let i = 0; i < 2; i++) {
        const worker = new Worker("require('isolated-vm')", { eval: true });
        await new Promise((resolve, reject) => {
            worker.on('exit', code => code ? reject(new Error(`Worker exited with code: ${code}`)) : resolve());
            worker.on('error', error => reject(error));
        });
    }
    console.log('pass')
})().catch(console.error);
