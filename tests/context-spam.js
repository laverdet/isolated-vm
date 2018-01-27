let ivm = require('isolated-vm')
let isolate = new ivm.Isolate({ memoryLimit: 32 });;

(async function () {
  while (true) {
    await isolate.createContext()
  }
}()).catch(() => console.log('pass'));
