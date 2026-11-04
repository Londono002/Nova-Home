const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const HOST = '127.0.0.1';
const PORT = Number(process.env.NOVA_PORT || 8787);
const ROOT = __dirname;
const MIME = {
  '.css': 'text/css; charset=utf-8',
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.mp3': 'audio/mpeg',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.svg': 'image/svg+xml; charset=utf-8'
};

function sendJson(res, status, value) {
  res.writeHead(status, { 'Content-Type': MIME['.json'], 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(value));
}

function validSteamGridPath(value) {
  if (!value || value.includes('\\') || value.startsWith('//')) return false;
  let parsed;
  try { parsed = new URL(value, 'https://www.steamgriddb.com/api/v2'); }
  catch { return false; }
  if (parsed.origin !== 'https://www.steamgriddb.com') return false;
  const endpoint = parsed.pathname;
  const search = /^\/search\/autocomplete\/[A-Za-z0-9%._~+-]+$/;
  const art = /^\/(?:grids|heroes|icons)\/game\/\d+$/;
  if (!(search.test(endpoint) || art.test(endpoint))) return false;
  for (const [key, val] of parsed.searchParams) {
    if (key !== 'dimensions' || val !== '600x900' || !endpoint.startsWith('/grids/')) return false;
  }
  return true;
}

async function steamGridProxy(req, res, requestUrl) {
  if (req.method !== 'GET') return sendJson(res, 405, { error: 'method_not_allowed' });
  const apiPath = requestUrl.searchParams.get('path');
  const authorization = req.headers.authorization || '';
  if (!/^Bearer\s+\S+$/i.test(authorization)) return sendJson(res, 401, { error: 'api_key_required' });
  if (!validSteamGridPath(apiPath)) return sendJson(res, 400, { error: 'invalid_api_path' });

  try {
    const upstream = await fetch(`https://www.steamgriddb.com/api/v2${apiPath}`, {
      headers: { Accept: 'application/json', Authorization: authorization },
      redirect: 'error',
      signal: AbortSignal.timeout(12000)
    });
    const body = await upstream.text();
    res.writeHead(upstream.status, {
      'Content-Type': upstream.headers.get('content-type') || MIME['.json'],
      'Cache-Control': 'no-store',
      'X-Content-Type-Options': 'nosniff'
    });
    res.end(body);
  } catch (error) {
    sendJson(res, error.name === 'TimeoutError' ? 504 : 502, {
      error: error.name === 'TimeoutError' ? 'steamgriddb_timeout' : 'steamgriddb_unavailable'
    });
  }
}

function serveStatic(req, res, requestUrl) {
  if (req.method !== 'GET' && req.method !== 'HEAD') {
    res.writeHead(405); return res.end();
  }
  let pathname;
  try { pathname = decodeURIComponent(requestUrl.pathname); }
  catch { res.writeHead(400); return res.end('Bad request'); }
  const relative = pathname === '/' ? 'index.html' : pathname.replace(/^\/+/, '');
  if (relative.split(/[\\/]/).some(part => part.startsWith('.'))) {
    res.writeHead(404); return res.end('Not found');
  }
  const filename = path.resolve(ROOT, relative);
  if (!filename.startsWith(`${ROOT}${path.sep}`)) {
    res.writeHead(403); return res.end('Forbidden');
  }
  fs.stat(filename, (error, stat) => {
    if (error || !stat.isFile()) { res.writeHead(404); return res.end('Not found'); }
    res.writeHead(200, {
      'Content-Type': MIME[path.extname(filename).toLowerCase()] || 'application/octet-stream',
      'X-Content-Type-Options': 'nosniff'
    });
    if (req.method === 'HEAD') return res.end();
    fs.createReadStream(filename).pipe(res);
  });
}

const server = http.createServer((req, res) => {
  const requestUrl = new URL(req.url, `http://${HOST}:${PORT}`);
  if (requestUrl.pathname === '/api/steamgriddb') return void steamGridProxy(req, res, requestUrl);
  serveStatic(req, res, requestUrl);
});

server.listen(PORT, HOST, () => {
  console.log(`NOVA HOME disponible en http://${HOST}:${PORT}`);
  console.log('Pulsa Ctrl+C para detener el servidor.');
});
