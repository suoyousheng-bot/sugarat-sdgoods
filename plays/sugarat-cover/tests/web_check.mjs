/** Exercise the shipped web page against the real firmware HTTP handlers and photo store on a host adapter. */
import { createRequire } from 'node:module';
import { spawn } from 'node:child_process';
import { mkdtemp, mkdir, rm, writeFile, readFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { once } from 'node:events';
import assert from 'node:assert/strict';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const repo=resolve(root,'../../..');
const require=createRequire(join(repo,'apps/web/package.json'));
const {chromium}=require('playwright');
const output=resolve(process.argv[2]||join(repo,'.thinkready/sugarat-cover'));
await mkdir(output,{recursive:true});
const storage=await mkdtemp(join(output,'web-storage-'));
const env=Object.fromEntries(Object.entries(process.env).filter(([key])=>!/(KEY|SECRET|TOKEN|PASSWORD)/i.test(key)));
const server=spawn('python3',[join(root,'tests/http_server.py'),'--bridge',join(output,'host/cover-http-bridge'),'--storage',storage],{cwd:repo,env,stdio:['ignore','pipe','pipe']});
let stderr='';server.stderr.on('data',chunk=>{stderr+=chunk.toString();});
const serverExit=once(server,'exit');
let browser;
try {
  const url=await new Promise((resolveURL,reject)=>{
    let buffer='';const deadline=setTimeout(()=>reject(Error('Local handler server did not become ready')),20000);
    server.stdout.on('data',chunk=>{buffer+=chunk;const end=buffer.indexOf('\n');if(end>=0){clearTimeout(deadline);try{resolveURL(JSON.parse(buffer.slice(0,end)).url);}catch(error){reject(error);}}});
    server.once('error',error=>{clearTimeout(deadline);reject(error);});
    server.once('exit',code=>{clearTimeout(deadline);reject(Error('Local handler server exited: '+code+' '+stderr));});
  });
  const origin=new URL(url).origin,token='1'.repeat(32);
  for(const path of ['/', '/generate_204', '/hotspot-detect.html', '/connecttest.txt', '/ncsi.txt']) {
    const probe=await fetch(origin+path,{redirect:'manual'});assert.equal(probe.status,302);assert.equal(probe.headers.get('location'),origin+'/gallery#'+token);
  }
  const captive=await fetch(origin+'/captive-portal');assert.equal(captive.headers.get('content-type'),'application/captive+json');
  assert.deepEqual(await captive.json(),{captive:true,'user-portal-url':'http://192.168.4.1/gallery#'+token});
  assert.equal((await fetch(origin+'/api/state')).status,401);
  assert.equal((await fetch(origin+'/api/state',{headers:{'X-Cover-Token':'0'.repeat(32)}})).status,401);
  const invalid=await fetch(origin+'/api/photos',{method:'POST',headers:{'X-Cover-Token':token},body:new Uint8Array([1,2,3,4])});assert.equal((await invalid.json()).error,2);
  const initial=await(await fetch(origin+'/api/state',{headers:{'X-Cover-Token':token}})).json();assert.equal(initial.locale,'en');assert.equal(initial.photos.length,1);
  const traversal=await fetch(origin+'/api/current/not-a-photo',{method:'PUT',headers:{'X-Cover-Token':token}});assert.equal((await traversal.json()).error,1);
  browser=await chromium.launch({headless:true});
  const page=await browser.newPage({viewport:{width:390,height:844},deviceScaleFactor:2});
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto(url);await page.locator('.tile').first().waitFor();
  assert.equal(page.url(),origin+'/gallery#'+token);
  assert.equal(await page.locator('#language').inputValue(),'en');
  await page.screenshot({path:join(output,'phone-en.png'),fullPage:true});
  await page.locator('#file').setInputFiles(join(root,'assets/sample-source.png'));
  await page.locator('#crop-page').waitFor({state:'visible'});
  await page.locator('#rotate').click();await page.locator('#zoom').fill('1.4');await page.locator('#zoom').dispatchEvent('input');
  await page.screenshot({path:join(output,'phone-crop-en.png'),fullPage:true});
  await page.locator('#reset').click();assert.equal(await page.locator('#zoom').inputValue(),'1');
  await page.locator('#zoom').fill('1.2');await page.locator('#zoom').dispatchEvent('input');
  await page.locator('#upload').click();await page.locator('#photos .tile').nth(1).waitFor();
  const uploaded=await(await fetch(origin+'/api/state',{headers:{'X-Cover-Token':token}})).json();assert.equal(uploaded.photos.length,2);assert.equal(uploaded.current,initial.current);
  const added=uploaded.photos.find(photo=>photo.id!==initial.current);assert.ok(added);
  await page.locator('.tile').filter({hasNot:page.locator('.tag')}).click();
  await page.locator('#show-photo').click();await page.locator('#photo-dialog').waitFor({state:'hidden'});
  const selected=await(await fetch(origin+'/api/state',{headers:{'X-Cover-Token':token}})).json();assert.equal(selected.current,added.id);
  await page.locator('#language').selectOption('zh');await page.getByRole('heading',{name:'把喜欢，带在身上'}).waitFor();
  await page.reload();await page.getByRole('heading',{name:'把喜欢，带在身上'}).waitFor();assert.equal(await page.locator('#language').inputValue(),'zh');
  await page.screenshot({path:join(output,'phone-zh.png'),fullPage:true});
  await page.locator('#file').setInputFiles(join(root,'assets/sample-source.png'));await page.locator('#crop-page').waitFor({state:'visible'});await page.screenshot({path:join(output,'phone-crop-zh.png'),fullPage:true});
  for(const width of [320,390,430]){await page.setViewportSize({width,height:844});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);}
  await page.locator('#back').click();await page.locator('#manage').click();
  await page.locator('.tile').filter({hasNot:page.locator('.tag')}).click();
  page.once('dialog',dialog=>dialog.accept());await page.locator('#delete-photo').click();await page.locator('#photo-dialog').waitFor({state:'hidden'});
  const final=await(await fetch(origin+'/api/state',{headers:{'X-Cover-Token':token}})).json();assert.equal(final.photos.length,1);assert.equal(final.current,added.id);assert.equal(final.locale,'zh');
  const rejectCurrent=await fetch(origin+'/api/image/'+added.id,{method:'DELETE',headers:{'X-Cover-Token':token}});assert.equal((await rejectCurrent.json()).error,6);
  const bytes=await readFile(join(storage,added.id+'.jpg'));assert.equal(bytes[0],255);assert.equal(bytes[1],216);
  const android=await browser.newPage({viewport:{width:390,height:844},userAgent:'Mozilla/5.0 (Linux; Android 14; Pixel 8; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/124.0.0.0 Mobile Safari/537.36'});
  android.on('pageerror',error=>errors.push(error.message));
  await android.goto(url);await android.locator('.tile').first().waitFor();
  const chooser=android.waitForEvent('filechooser');await android.locator('#choose').click();
  await(await chooser).setFiles(join(root,'assets/sample-source.png'));await android.locator('#crop-page').waitFor({state:'visible'});
  await android.waitForTimeout(1800);assert.equal(await android.locator('#notice').isHidden(),true);
  await android.locator('#back').click();android.removeAllListeners('filechooser');
  await android.locator('#choose').click();await android.locator('#notice',{hasText:'192.168.4.1'}).waitFor({timeout:4000});
  await android.close();
  assert.deepEqual(errors,[]);
  await writeFile(join(output,'web-validation.json'),JSON.stringify({passed:true,adapter:'Actual cv_http.c handlers and cv_store.c; host transport and settings adapter',checks:['Root URL enters the management session without a scanned token','Android/iOS/Windows captive HTTP probe redirects','DHCP captive API response','English default','Chinese persistence after reload','token required for gallery and mutations','malformed JPEG rejected','invalid identifiers rejected','real browser crop/export/upload','upload preserves current cover','explicit display selection','delete retains current cover','Android picker that never opens shows the browser instruction','320/390/430 px layouts','no browser errors']},null,2)+'\n');
  console.log('PASS: captive HTTP probes and API, root session entry, bilingual page, real firmware handlers, crop/upload/select/delete, saved settings, auth failures and responsive widths');
} finally {
  if(browser)await browser.close();
  if(server.exitCode===null)server.kill('SIGTERM');
  const deadline=new Promise((_,reject)=>setTimeout(()=>reject(Error('Handler server did not stop')),15000).unref());
  const [code]=await Promise.race([serverExit,deadline]);
  await rm(storage,{recursive:true,force:true});
  if(code!==0)throw Error('Handler server failed: '+code+' '+stderr);
}
