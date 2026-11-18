#!/usr/bin/env python3
"""Exercise the actual mobile editor against native C parsing and persistence."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import tempfile
import threading
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[2]
TOKEN = '0123456789abcdef0123456789abcdef'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    executable = (args.build / 'card-profile').resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    lock = threading.Lock()
    operation = {'id': 0, 'state': 'idle'}
    with tempfile.TemporaryDirectory(prefix='sugarat-card-editor-') as directory:
        def run(mode, body=None):
            return subprocess.run([str(executable), mode, directory], input=body, capture_output=True, check=True, timeout=10).stdout

        class Handler(BaseHTTPRequestHandler):
            def setup(self):
                super().setup()
                self.connection.settimeout(5)

            def log_message(self, *args):
                pass

            def reply(self, code, body, content_type='application/json'):
                if isinstance(body, dict):
                    body = json.dumps(body, ensure_ascii=False).encode()
                self.send_response(code)
                self.send_header('Content-Type', content_type)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                if self.path == '/':
                    self.send_response(302)
                    self.send_header('Location', '/edit#' + TOKEN)
                    self.send_header('Cache-Control', 'no-store')
                    self.end_headers()
                elif urlsplit(self.path).path == '/edit':
                    self.reply(200, (ROOT / 'main/card/editor.html').read_bytes(), 'text/html; charset=utf-8')
                elif self.headers.get('Authorization') != 'Bearer ' + TOKEN:
                    self.reply(401, {'error': 'unauthorized'})
                elif self.path == '/api/profile':
                    with lock:
                        self.reply(200, run('get'))
                elif self.path == '/api/status':
                    with lock:
                        self.reply(200, dict(operation))
                else:
                    self.reply(404, {})

            def do_POST(self):
                if self.headers.get('Authorization') != 'Bearer ' + TOKEN:
                    self.reply(401, {'error': 'unauthorized'})
                    return
                size = int(self.headers.get('Content-Length', '0'))
                if size < 1 or size > 1024:
                    self.reply(400, {'error': 'too_large'})
                    return
                body = self.rfile.read(size)
                with lock:
                    try:
                        run('save', body)
                    except subprocess.CalledProcessError:
                        self.reply(400, {'error': 'invalid'})
                        return
                    operation.update(id=operation['id'] + 1, state='saved')
                    self.reply(202, {'id': operation['id'], 'state': 'queued'})

        script = output / 'editor-check.cjs'
        script.write_text('''const {chromium}=require(require.resolve('playwright',{paths:[process.argv[2]+'/apps/web']}));
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true});
 try{
  const context=await browser.newContext({viewport:{width:390,height:844},deviceScaleFactor:1,isMobile:true,hasTouch:true,locale:'zh-CN'});
  const page=await context.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const origin=process.argv[3],out=process.argv[4],token='0123456789abcdef0123456789abcdef';
  const status=text=>page.waitForFunction(text=>document.querySelector('#status').textContent.includes(text),text);
  const ready=()=>page.waitForFunction(()=>document.querySelector('#save').disabled===false);
  await page.goto(origin+'/');assert.ok(page.url().endsWith('/edit#'+token));await ready();
  assert.equal(await page.evaluate(()=>document.activeElement.tagName),'BODY');
  assert.equal(await page.getAttribute('html','lang'),'en');await page.click('#language');
  await page.fill('#name','小林');await page.fill('#company','Deotaland');await page.fill('#role','硬件开发者');
  await page.fill('#contact','lin@example.org');await page.fill('#link','javascript:alert(1)');await page.click('#save');await status('完整');
  await page.click('#language');await status('complete http');assert.equal(await page.inputValue('#name'),'小林');
  assert.equal(await page.getAttribute('html','lang'),'en');
  await page.fill('#company','A'.repeat(65));await page.fill('#link','https://example.org/lin');await page.click('#save');await status('too long');
  await page.click('#language');await status('过长');await page.fill('#company','Deotaland');await page.click('#save');await status('已保存到设备');
  assert.equal(await page.textContent('#v-name'),'小林');assert.equal(await page.textContent('#v-meta'),'硬件开发者 · Deotaland');
  await page.screenshot({path:out+'/editor-saved-zh.png',fullPage:true});
  await page.click('#language');await status('Saved to badge');await page.screenshot({path:out+'/editor-saved-en.png',fullPage:true});
  await page.reload();await ready();assert.equal(await page.getAttribute('html','lang'),'en');assert.equal(await page.inputValue('#name'),'小林');
  await page.setViewportSize({width:320,height:720});
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
  await page.screenshot({path:out+'/editor-narrow.png',fullPage:true});
  await page.route('**/api/status',route=>route.fulfill({json:{id:2,state:'error',error:'save_unconfirmed'}}));
  await page.click('#save');await status('could not confirm saving');assert.ok(!(await page.textContent('#status')).includes('Saved to badge'));
  await page.click('#language');await status('未能确认保存');await page.unroute('**/api/status');
  for(const lang of ['zh','en']){
   await page.goto(origin+'/edit?lang='+lang);await status(lang==='zh'?'缺少编辑授权':'authorization is missing');assert.equal(await page.isDisabled('#save'),true);
   await page.goto(origin+'/edit?lang='+lang+'#ffffffffffffffffffffffffffffffff');await status(lang==='zh'?'授权已失效':'authorization expired');assert.equal(await page.isDisabled('#save'),true);
  }
  await page.goto(origin+'/');await ready();assert.equal(await page.inputValue('#name'),'小林');await page.click('#language');
  await page.route('**/api/profile',route=>route.fulfill({status:409,json:{error:'busy'}}));
  await page.click('#save');await status('仍在处理');await page.click('#language');await status('still processing');await page.unroute('**/api/profile');
  await page.goto(origin+'/edit#ffffffffffffffffffffffffffffffff');await status('authorization expired');
  await page.evaluate(token=>location.hash=token,token);await ready();assert.equal(await page.inputValue('#name'),'小林');
  assert.deepEqual(errors,[]);await context.close();
  console.log('PASS: bilingual mobile editor, error translation, reload/hash authorization, narrow viewport, URL/byte limits and unconfirmed save');
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
''')
        with ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
            thread = threading.Thread(target=server.serve_forever)
            try:
                thread.start()
                url = f'http://127.0.0.1:{server.server_port}'
                subprocess.run(['node', str(script), str(REPO), url, str(output)], check=True, timeout=90)
                saved = json.loads(run('get'))
                assert saved == {'name': '小林', 'company': 'Deotaland', 'role': '硬件开发者', 'contact': 'lin@example.org', 'link': 'https://example.org/lin', 'kind': 0}
                assert operation['id'] == 2
                print('PASS: saved fields independently reloaded from the actual C file store')
            finally:
                if thread.ident is not None:
                    server.shutdown()
                    thread.join()


if __name__ == '__main__':
    main()
