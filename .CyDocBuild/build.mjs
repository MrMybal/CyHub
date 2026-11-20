import { spawnSync } from 'node:child_process';
import { copyFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const workspace = fileURLToPath(new URL('.', import.meta.url));
const command = process.argv[2] ?? 'build';
const source = process.env.CYDOC_SOURCE_DIR;
if (!['build', 'check'].includes(command) || !source) {
    console.error('Set CYDOC_SOURCE_DIR to a prepared CyDoc 0.1.0 installation. Run build.mjs [build|check].');
    process.exit(1);
}

const result = spawnSync(process.execPath, [
    resolve(source, 'tools/cli.mjs'), command, resolve(workspace, 'cydoc.config.json')
], { cwd: workspace, stdio: 'inherit' });
if (result.error)
    throw result.error;
if (result.status !== 0)
    process.exit(result.status ?? 1);

const docs = resolve(workspace, '../Documentation');
await copyFile(resolve(workspace, 'DocAI.md'), resolve(docs, 'DocAI.md'));
await copyFile(resolve(source, 'LICENSE'), resolve(docs, 'DocData/CyDoc-MIT.txt'));
