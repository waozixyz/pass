// Exercise the real generated app in a private headless browser.
import {spawn} from 'node:child_process';
import {readFile, writeFile, mkdir, mkdtemp} from 'node:fs/promises';
import {createServer} from 'node:http';
import {resolve, join, extname} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const root=resolve('build/site'), output=resolve('build/web-test');
await mkdir(output,{recursive:true});
const profile=await mkdtemp(join(output,'profile-'));
const server=createServer(async(req,res)=>{
  const pathname=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
  const file=join(root,pathname.endsWith('/')?pathname+'index.html':pathname);
  if(!file.startsWith(root+'/')){res.writeHead(403).end();return;}
  try {const data=await readFile(file);res.setHeader('Content-Type',({'.js':'application/javascript','.wasm':'application/wasm','.html':'text/html','.webmanifest':'application/manifest+json'})[extname(file)]||'application/octet-stream');res.end(data);}catch{res.writeHead(404).end();}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const url=process.env.PASS_WEB_TEST_URL || `http://127.0.0.1:${server.address().port}/app/`;
const env={...process.env};delete env.DISPLAY;delete env.WAYLAND_DISPLAY;
const browser=spawn(process.env.PASS_WEB_SMOKE_BROWSER||'chromium',['--headless=new','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--no-first-run','--remote-debugging-port=0','--user-data-dir='+profile,'--window-size=720,740','about:blank'],{env,detached:true,stdio:['ignore','ignore','pipe']});
let socket, diagnostics='';browser.stderr.on('data',data=>diagnostics=(diagnostics+data).slice(-4000));
try {
  let port;const deadline=Date.now()+30000;
  while(Date.now()<deadline){try{port=(await readFile(join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0];break;}catch{} await delay(100);}
  if(!port)throw Error('Chromium did not start: '+diagnostics);
  const pages=await(await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  socket=new WebSocket(pages.find(page=>page.type==='page').webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
  let sequence=0;const pending=new Map(), exceptions=[];
  socket.addEventListener('message',event=>{const m=JSON.parse(event.data);if(m.method==='Runtime.exceptionThrown')exceptions.push(m.params.exceptionDetails);if(m.method==='Runtime.exceptionThrown'||m.method==='Runtime.consoleAPICalled')diagnostics+=JSON.stringify(m.params);if(pending.has(m.id)){pending.get(m.id)(m);pending.delete(m.id);}});
  async function command(method,params={}){const id=++sequence;const answer=new Promise(resolve=>pending.set(id,resolve));socket.send(JSON.stringify({id,method,params}));const result=await answer;if(result.error)throw Error(JSON.stringify(result.error));return result.result;}
  async function evaluate(expression){const result=await command('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(result.exceptionDetails)throw Error(JSON.stringify(result.exceptionDetails));return result.result.value;}
  async function waitFor(expression){const until=Date.now()+30000;while(Date.now()<until){try{if(await evaluate(expression))return;}catch(error){if(!String(error).includes("ReferenceError")&&!String(error).includes("TypeError"))throw error;}await delay(100);}const shot=await command('Page.captureScreenshot',{format:'png'});await writeFile(join(output,'failure.png'),Buffer.from(shot.data,'base64'));throw Error('Timed out: '+expression+' '+await evaluate('JSON.stringify({paint:window.__passPaint?.slice(-30),module:typeof Module,exit:globalThis.Module?.EXITSTATUS,canvas:document.querySelector("canvas")?.width})')+' '+diagnostics);}
  await command('Runtime.enable');await command('Page.enable');
  await command('Page.addScriptToEvaluateOnNewDocument',{source:`
    window.__passDrawCount=0;
    const draw=CanvasRenderingContext2D.prototype.drawImage;
    CanvasRenderingContext2D.prototype.drawImage=function(...args){
      if(this.canvas.id==='canvas')window.__passDrawCount++;
      return draw.apply(this,args);
    };
  `});
  const painted='(()=>{const c=document.querySelector("canvas");if(!c)return false;const p=c.getContext("2d").getImageData(50,innerHeight-35,1,1).data;return p[3]===255&&p[1]<130&&window.__passDrawCount>0})()';
  await command('Browser.grantPermissions',{origin:new URL(url).origin,permissions:['clipboardReadWrite','clipboardSanitizedWrite']});
  await command('Page.navigate',{url});
  await delay(500);
  await waitFor(painted);
  // Shorten the timer in this disposable browser profile, then load it as
  // persisted settings. The user's browser data is never used.
  await evaluate('Module.FS.writeFile("/pass-data/.kryon_pass_clear_after_seconds.txt","1\\n");new Promise((resolve,reject)=>Module.FS.syncfs(false,error=>error?reject(error):resolve()))');
  await command('Page.reload');
  await waitFor(painted);
  async function click(x,y){await command('Input.dispatchMouseEvent',{type:'mouseMoved',x,y});await command('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',clickCount:1});await delay(70);await command('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'left',clickCount:1});await delay(120);}
  async function type(text){for(const char of text){await command('Input.dispatchKeyEvent',{type:'rawKeyDown',key:char,code:char===' '?'Space':undefined});await command('Input.dispatchKeyEvent',{type:'char',key:char,text:char,unmodifiedText:char});await command('Input.dispatchKeyEvent',{type:'keyUp',key:char});await delay(25);}await delay(200);}
  const dims=await evaluate('({width:innerWidth,height:innerHeight})');
  await click(90,55);await type('example.com');
  await click(90,130);await type('alice');await click(90,205);await type('test master');
  // This expected password comes from the independent LessPass reference.
  const expected='dEeEDu7/b27L#r<&';
  const offset=Math.max(0,800-(dims.height-110));
  await command('Input.dispatchMouseEvent',{type:'mouseWheel',x:120,y:300,deltaX:0,deltaY:10000});await delay(250);
  await click(120,535-offset);await delay(500);
  // Generate scrolls to the password. Copy must reach the real clipboard.
  await click(120,690-offset);
  await waitFor(`navigator.clipboard.readText().then(text=>text===${JSON.stringify(expected)})`);
  await waitFor('navigator.clipboard.readText().then(text=>text==="")');
  await click(120,690-offset);
  await waitFor(`navigator.clipboard.readText().then(text=>text===${JSON.stringify(expected)})`);
  await evaluate('navigator.clipboard.writeText("Another app test")');
  await delay(1400);
  if(await evaluate('navigator.clipboard.readText()')!=='Another app test')throw Error('Clipboard timer erased text copied elsewhere');
  await click(dims.width/2,dims.height-35); // Profiles tab
  await click(90,55);await type('Browser test');await click(120,105);
  await waitFor('Module.FS.analyzePath("/pass-data/profiles.tsv").exists');
  const profiles=await evaluate('Module.FS.readFile("/pass-data/profiles.tsv",{encoding:"utf8"})');
  if(!profiles.includes('Browser test\texample.com\talice\t'))throw Error('Saved profile does not match input: '+profiles);
  // Reload tests IDBFS population, not just the in-memory filesystem.
  await command('Page.reload');await waitFor('Module.FS.analyzePath("/pass-data/profiles.tsv").exists');
  if(await evaluate('Module.FS.readFile("/pass-data/profiles.tsv",{encoding:"utf8"})')!==profiles)throw Error('Profile did not survive reload');
  await waitFor('navigator.serviceWorker.controller !== null');
  await waitFor('caches.open("pass-ziran-v1").then(c=>c.match("index.wasm")).then(Boolean)');
  await command('Network.enable');await command('Network.emulateNetworkConditions',{offline:true,latency:0,downloadThroughput:0,uploadThroughput:0});
  await command('Page.reload');await waitFor(painted);
  await waitFor('Module.FS.analyzePath("/pass-data/profiles.tsv").exists');
  const screenshot=await command('Page.captureScreenshot',{format:'png'});
  await writeFile(join(output,'pass-offline.png'),Buffer.from(screenshot.data,'base64'));
  if(exceptions.length)throw Error('Browser exceptions: '+JSON.stringify(exceptions));
  await command('Network.emulateNetworkConditions',{offline:false,latency:0,downloadThroughput:-1,uploadThroughput:-1});
  // A denied/missing IndexedDB must not stop canvas initialization or drawing.
  // Use only this disposable profile; never change the user's storage policy.
  for(const [name,source,mini] of [
    ['denied',`IDBFactory.prototype.open=function(){throw new DOMException('Storage access denied','SecurityError')}`,false],
    ['denied-mini',`IDBFactory.prototype.open=function(){throw new DOMException('Storage access denied','SecurityError')}`,true],
    ['missing',`Object.defineProperty(globalThis,'indexedDB',{value:undefined})`,false]
  ]){
    const script=await command('Page.addScriptToEvaluateOnNewDocument',{source});
    const blockedUrl=new URL(url);
    if(mini){blockedUrl.searchParams.set('mini','1');blockedUrl.searchParams.set('theme','waozi');}
    await command('Page.navigate',{url:blockedUrl.href});
    await waitFor(mini?'window.__passDrawCount>0&&document.querySelector("canvas").getContext("2d").getImageData(5,5,1,1).data[3]===255':painted);
    await evaluate('navigator.clipboard.writeText("Storage test sentinel")');
    const frames=await evaluate('window.__passDrawCount');await delay(120);
    if(await evaluate('window.__passDrawCount')<=frames)throw Error(name+': drawing stopped');
    await click(90,mini?100:55);await type('example.com');
    await click(90,mini?176:130);await type('alice');
    await click(90,mini?252:205);await type('test master');
    const h=await evaluate('innerHeight');
    const scroll=Math.max(0,(mini?474:800)-(h-(mini?92:110)));
    await command('Input.dispatchMouseEvent',{type:'mouseWheel',x:120,y:300,deltaX:0,deltaY:10000});await delay(250);
    await click(120,(mini?338:535)-scroll);await delay(500);
    await click(120,(mini?490:690)-scroll);
    await waitFor(`navigator.clipboard.readText().then(text=>text===${JSON.stringify(expected)})`);
    if(!mini){
      await click(dims.width/2,h-35);await click(90,55);await type('Cannot persist');await click(120,105);
      if(await evaluate('Module.FS.analyzePath("/pass-data/profiles.tsv").exists'))throw Error(name+': unavailable storage accepted a profile save');
    }
    const shot=await command('Page.captureScreenshot',{format:'png'});
    await writeFile(join(output,'pass-storage-'+name+'.png'),Buffer.from(shot.data,'base64'));
    if(exceptions.length)throw Error(name+': browser exceptions '+JSON.stringify(exceptions));
    await command('Page.removeScriptToEvaluateOnNewDocument',{identifier:script.identifier});
  }
  console.log('Ziran browser: rendering, input, expected password, clipboard copy/clear/preservation, profiles, offline reload and denied/missing-storage canvas rendering/generation pass');
} finally {
  if(socket)socket.close();try{process.kill(-browser.pid,'SIGTERM');}catch{}
  await new Promise(resolve=>{if(browser.exitCode!==null||browser.signalCode!==null)resolve();else browser.once('exit',resolve);});server.close();
}
