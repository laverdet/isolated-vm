const os = require('node:os');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const targetArgs = [
  '--strip',
  '--no-napi',
  '--target',
  '24.14.0',
  '--target',
  '26.0.0',
];

if (process.platform === 'linux') {
  targetArgs.push('--tag-libc');
}

const env = {
  ...process.env,
  MAKEFLAGS: `-j${os.cpus().length}`,
};

fs.rmSync(path.join(__dirname, '..', 'prebuilds'), { recursive: true, force: true });

let command = process.execPath;
let args;

try {
  args = [require.resolve('prebuildify/bin.js'), ...targetArgs];
} catch (error) {
  if (error && error.code === 'MODULE_NOT_FOUND' && error.message.includes('prebuildify/bin.js')) {
    command = process.platform === 'win32' ? 'node-gyp.cmd' : 'node-gyp';
    args = ['rebuild', '--release', '-j', 'max'];
  } else {
    throw error;
  }
}

const result = spawnSync(command, args, { stdio: 'inherit', env });

if (result.error) {
  throw result.error;
}

process.exit(result.status ?? 1);