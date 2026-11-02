#!/usr/bin/env node
process.stdout.write(new URL(".", import.meta.url).pathname);
