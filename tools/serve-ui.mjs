import { readFile, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';

const source = await readFile(new URL('../firmware/AtomNeoPixel/WebUi.h', import.meta.url), 'utf8');
const page = source.match(/R"ATOMHTML\(([\s\S]+?)\)ATOMHTML";/)?.[1];
if (!page) throw new Error('Could not locate the firmware UI');

const args = process.argv.slice(2);
if (args.length === 2 && args[0] === '--export') {
  await writeFile(args[1], page, { flag: 'wx' });
} else if (args.length === 0) {
  const address = '127.0.0.1';
  const port = 8080;
  createServer((request, response) => {
    if (request.method !== 'GET' || request.url !== '/' || request.headers.host !== `${address}:${port}`) {
      response.writeHead(404).end();
      return;
    }
    response.writeHead(200, {
      'Content-Type': 'text/html; charset=utf-8',
      'Cache-Control': 'no-store',
    }).end(page);
  }).listen(port, address, () => console.log(`Open http://${address}:${port}/ in Chrome`));
} else {
  throw new Error('Usage: node tools/serve-ui.mjs [--export path-to-new-index.html]');
}
