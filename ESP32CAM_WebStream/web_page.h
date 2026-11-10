/* =====================================================================
 *  web_page.h —— 内嵌网页 (PROGMEM)
 *  单独放到 .h 里是必须的:
 *  Arduino 会用 ctags 扫描 .ino 文件插入函数原型, 它不认 raw string,
 *  会把 JS 里的 function xxx(){ } 当成 C++ 函数, 生成非法代码导致编译失败。
 *  放在 .h 中可完全避开该扫描。
 * ===================================================================== */
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>ESP32-CAM 图传</title>
<link rel="icon" href="data:,">
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#0b1016;--card:#131b23;--card2:#0e161d;--line:#22303d;
 --txt:#e7f1f9;--dim:#7f95ab;--acc:#22d3ee;--ok:#3ddc97;--warn:#f2c14e;--bad:#f87171}
body{background:radial-gradient(1100px 520px at 50% -240px,#123945 0,#0b1016 62%);
 color:var(--txt);min-height:100vh;padding:14px;-webkit-tap-highlight-color:transparent;
 font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","Microsoft YaHei",sans-serif}
.wrap{max-width:1180px;margin:0 auto}
header{display:flex;align-items:center;gap:11px;padding:4px 4px 12px}
.logo{width:34px;height:34px;border-radius:10px;flex:none;
 background:linear-gradient(135deg,#22d3ee,#0ea5e9);display:grid;place-items:center;
 font-weight:800;font-size:15px;color:#04121a}
h1{font-size:16px;font-weight:600;letter-spacing:.3px}
.sub{font-size:11.5px;color:var(--dim);margin-top:2px;font-variant-numeric:tabular-nums}
.tabs{display:flex;gap:6px;background:var(--card);border:1px solid var(--line);
 border-radius:12px;padding:4px;margin-bottom:14px;width:fit-content}
.tab{border:0;background:transparent;color:var(--dim);padding:8px 18px;border-radius:9px;
 font-size:13px;cursor:pointer;font-family:inherit;transition:.15s}
.tab.on{background:#22d3ee1a;color:var(--acc)}
.grid{display:flex;gap:14px;align-items:flex-start}
@media(max-width:860px){.grid{flex-direction:column}}
.viewer{flex:1;min-width:0;position:relative;background:#05090d;border:1px solid var(--line);
 border-radius:14px;overflow:hidden;aspect-ratio:4/3;display:grid;place-items:center}
#video{width:100%;height:100%;object-fit:contain;display:block}
.halt{position:absolute;inset:0;display:grid;place-items:center;text-align:center;
 background:#05090dee;color:var(--dim);font-size:13px;line-height:1.9;padding:20px}
.halt b{color:var(--txt);display:block;font-size:14px;margin-bottom:4px}
.tag{position:absolute;left:10px;top:10px;display:flex;gap:6px;align-items:center;
 background:#05090dcc;border:1px solid var(--line);border-radius:99px;padding:4px 10px;
 font-size:11px;color:var(--dim);font-variant-numeric:tabular-nums}
.dot{width:7px;height:7px;border-radius:50%;background:var(--bad);flex:none}
.dot.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.recbadge{position:absolute;right:10px;top:10px;display:none;align-items:center;gap:6px;
 background:#f8717122;border:1px solid #f8717155;color:#ff9d9d;border-radius:99px;
 padding:4px 10px;font-size:11px;font-variant-numeric:tabular-nums}
.recbadge.on{display:flex}
.recdot{width:7px;height:7px;border-radius:50%;background:#f87171;animation:blink 1s infinite}
@keyframes blink{50%{opacity:.15}}
.panel{width:300px;flex:none;display:flex;flex-direction:column;gap:12px}
@media(max-width:860px){.panel{width:100%}}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:13px}
.card h2{font-size:11px;font-weight:600;color:var(--dim);letter-spacing:1.2px;
 text-transform:uppercase;margin-bottom:10px}
.rows{display:flex;flex-direction:column;gap:7px;font-size:12.5px}
.row{display:flex;justify-content:space-between;gap:10px}
.row span:first-child{color:var(--dim)}
.row span:last-child{font-variant-numeric:tabular-nums;text-align:right;word-break:break-all}
.field{margin-bottom:11px}
.field:last-child{margin-bottom:0}
.field label{display:flex;justify-content:space-between;font-size:12.5px;color:var(--dim);margin-bottom:6px}
.field label b{color:var(--acc);font-weight:600;font-variant-numeric:tabular-nums}
select,input[type=range]{width:100%}
select{background:var(--card2);color:var(--txt);border:1px solid var(--line);border-radius:9px;
 padding:9px 10px;font-size:13px;outline:none;appearance:none;
 background-image:linear-gradient(45deg,transparent 50%,#7f95ab 50%),
                  linear-gradient(135deg,#7f95ab 50%,transparent 50%);
 background-position:calc(100% - 17px) 17px,calc(100% - 12px) 17px;
 background-size:5px 5px,5px 5px;background-repeat:no-repeat}
input[type=range]{-webkit-appearance:none;height:4px;border-radius:99px;
 background:linear-gradient(90deg,var(--acc),#0ea5e9);outline:none;margin:6px 0}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;
 border-radius:50%;background:#fff;border:3px solid var(--acc);cursor:pointer}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:8px}
button{border:1px solid var(--line);background:var(--card2);color:var(--txt);border-radius:10px;
 padding:10px 8px;font-size:12.5px;cursor:pointer;transition:.15s;font-family:inherit}
button:hover{border-color:var(--acc);color:var(--acc)}
button:active{transform:scale(.97)}
button.on{background:#22d3ee1a;border-color:var(--acc);color:var(--acc)}
button:disabled{opacity:.4;cursor:not-allowed}
button.primary{background:linear-gradient(135deg,#22d3ee,#0ea5e9);color:#04121a;
 border:none;font-weight:600;grid-column:1/-1}
button.rec{background:linear-gradient(135deg,#f87171,#dc2626);color:#fff;border:none;
 font-weight:600;grid-column:1/-1}
button.rec.stop{background:var(--card2);color:#ff9d9d;border:1px solid #f8717155}
.toast{position:fixed;left:50%;bottom:26px;transform:translate(-50%,20px);opacity:0;
 background:#1b2733;border:1px solid var(--line);border-radius:10px;padding:10px 18px;
 font-size:13px;pointer-events:none;transition:.25s;z-index:99;
 box-shadow:0 12px 30px -10px #000;max-width:88vw}
.toast.show{opacity:1;transform:translate(-50%,0)}
.pane{display:none}
.pane.on{display:block}
.fm{background:var(--card);border:1px solid var(--line);border-radius:14px;overflow:hidden}
.fmbar{display:flex;align-items:center;gap:8px;padding:11px 13px;border-bottom:1px solid var(--line);
 flex-wrap:wrap}
.crumb{font-size:13px;flex:1;min-width:120px;word-break:break-all}
.crumb a{color:var(--acc);cursor:pointer;text-decoration:none}
.crumb i{color:var(--dim);font-style:normal;margin:0 4px}
.fmbar button{padding:7px 12px;font-size:12px}
.list{max-height:60vh;overflow-y:auto}
.frow{display:flex;align-items:center;gap:10px;padding:10px 13px;
 border-bottom:1px solid #1a232d;font-size:13px}
.frow:hover{background:#17222c}
.frow.dir{cursor:pointer}
.fic{width:26px;height:26px;border-radius:7px;flex:none;display:grid;place-items:center;
 font-size:9px;font-weight:700;background:#1c2a36;color:var(--dim)}
.fic.d{background:#22d3ee1a;color:var(--acc)}
.fic.v{background:#f8717120;color:#ff9d9d}
.fname{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.fsize{color:var(--dim);font-size:11.5px;font-variant-numeric:tabular-nums;flex:none}
.fops{display:flex;gap:5px;flex:none}
.fops button{padding:5px 9px;font-size:11px;border-radius:7px}
.empty{padding:36px 16px;text-align:center;color:var(--dim);font-size:13px}
.fmfoot{padding:9px 13px;font-size:11.5px;color:var(--dim);border-top:1px solid var(--line);
 display:flex;justify-content:space-between;gap:10px;flex-wrap:wrap}
.up{position:fixed;left:0;right:0;top:0;height:3px;background:#22d3ee;width:0;z-index:100;
 transition:width .2s;display:none}
.up.on{display:block}
</style>
</head>
<body>
<div class="up" id="upbar"></div>
<div class="wrap">
  <header>
    <div class="logo">CAM</div>
    <div>
      <h1>ESP32-CAM 图传</h1>
      <div class="sub" id="sub">正在连接设备…</div>
    </div>
  </header>

  <div class="tabs">
    <button class="tab on" data-pane="live">实时画面</button>
    <button class="tab" data-pane="files">文件管理</button>
  </div>

  <div class="pane on" id="pane-live">
    <div class="grid">
      <div class="viewer">
        <img id="video" alt="">
        <div class="tag"><i class="dot" id="dot"></i><span id="tagtext">未开始</span></div>
        <div class="recbadge" id="recbadge"><i class="recdot"></i><span id="rectext">录制中</span></div>
        <div class="halt" id="halt"><div><b>画面未连接</b>请确认手机/电脑已连上设备的 WiFi</div></div>
      </div>

      <div class="panel">
        <div class="card">
          <h2>设备状态</h2>
          <div class="rows">
            <div class="row"><span>当前地址</span><span id="s-ip">-</span></div>
            <div class="row"><span>路由器 IP</span><span id="s-staip">-</span></div>
            <div class="row"><span>信号强度</span><span id="s-rssi">-</span></div>
            <div class="row"><span>实时帧率</span><span id="s-fps">-</span></div>
            <div class="row"><span>可用内存</span><span id="s-heap">-</span></div>
            <div class="row"><span>SD 卡</span><span id="s-sd">-</span></div>
          </div>
        </div>

        <div class="card">
          <h2>画面设置</h2>
          <div class="field">
            <label>分辨率</label>
            <select id="fs">
              <option value="VGA">640 × 480</option>
              <option value="SVGA" selected>800 × 600</option>
              <option value="XGA">1024 × 768</option>
              <option value="HD">1280 × 720</option>
              <option value="UXGA">1600 × 1200</option>
            </select>
          </div>
          <div class="field"><label>画质 <b id="qv">12</b></label>
            <input type="range" id="q" min="6" max="40" value="12"></div>
          <div class="field"><label>亮度 <b id="bv">0</b></label>
            <input type="range" id="b" min="-2" max="2" value="0"></div>
          <div class="field"><label>对比度 <b id="cv">0</b></label>
            <input type="range" id="c" min="-2" max="2" value="0"></div>
        </div>

        <div class="card">
          <h2>录制</h2>
          <div class="field">
            <label>录制帧率</label>
            <select id="rfps">
              <option value="5">5 fps（省空间）</option>
              <option value="10" selected>10 fps（推荐）</option>
              <option value="15">15 fps（流畅）</option>
            </select>
          </div>
          <div class="btns">
            <button class="rec" id="btnRec">开始录制</button>
          </div>
        </div>

        <div class="card">
          <h2>快捷操作</h2>
          <div class="btns">
            <button id="btnH">水平镜像</button>
            <button id="btnV">垂直翻转</button>
            <button id="btnL">补光灯</button>
            <button id="btnR">重连画面</button>
            <button class="primary" id="btnCap">抓拍并下载</button>
            <button id="btnSave" style="grid-column:1/-1">保存一张到 SD 卡</button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="pane" id="pane-files">
    <div class="fm">
      <div class="fmbar">
        <div class="crumb" id="crumb">SD 卡</div>
        <button id="btnRefresh">刷新</button>
        <button id="btnMkdir">新建文件夹</button>
        <button id="btnUpload">上传文件</button>
        <button id="btnWipe">清空</button>
        <input type="file" id="fileInput" hidden>
      </div>
      <div class="list" id="list"><div class="empty">正在读取…</div></div>
      <div class="fmfoot">
        <span id="fmstat">-</span>
        <span id="fmcount"></span>
      </div>
    </div>
  </div>
</div>
<div class="toast" id="toast"></div>

<script>
const HOST = location.hostname;
const STREAM_URL = 'http://' + HOST + ':81/stream';
const img = document.getElementById('video');
const halt = document.getElementById('halt');
const dot = document.getElementById('dot');
const tagtext = document.getElementById('tagtext');
const recbadge = document.getElementById('recbadge');
const rectext = document.getElementById('rectext');
const btnRec = document.getElementById('btnRec');
let streaming = false, lastFrames = 0, lastT = 0, mirror = 0, vflip = 0;
let curPath = '/', recOn = false, devFs = 'SVGA';

function toast(msg){
  const t = document.getElementById('toast');
  t.textContent = msg; t.classList.add('show');
  clearTimeout(t._id); t._id = setTimeout(()=>t.classList.remove('show'), 2000);
}
function esc(s){ return String(s).replace(/[&<>"']/g, c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
function fsz(b){
  if(b < 1024) return b + ' B';
  if(b < 1048576) return (b/1024).toFixed(1) + ' KB';
  if(b < 1073741824) return (b/1048576).toFixed(1) + ' MB';
  return (b/1073741824).toFixed(2) + ' GB';
}
function mmss(s){
  const m = Math.floor(s/60), r = Math.floor(s%60);
  return (m<10?'0':'')+m+':'+(r<10?'0':'')+r;
}

document.querySelectorAll('.tab').forEach(t=>{
  t.onclick = ()=>{
    document.querySelectorAll('.tab').forEach(x=>x.classList.remove('on'));
    document.querySelectorAll('.pane').forEach(x=>x.classList.remove('on'));
    t.classList.add('on');
    document.getElementById('pane-' + t.dataset.pane).classList.add('on');
    if(t.dataset.pane === 'files') loadDir(curPath);
  };
});

function setLive(on, text){
  dot.classList.toggle('on', on);
  tagtext.textContent = text;
  halt.style.display = on ? 'none' : 'grid';
}
function startStream(){
  setLive(true, '连接中…');
  img.src = STREAM_URL + '?t=' + Date.now();
  streaming = true;
}
function stopStream(){
  img.removeAttribute('src');
  streaming = false;
  setLive(false, '已停止');
}
img.onload = ()=>{ if(streaming) setLive(true, '实时'); };
img.onerror = ()=>{ if(streaming) setLive(false, '画面中断'); };

async function apply(q){
  const was = streaming;
  if(was) stopStream();
  try { await fetch('/control?' + q, {cache:'no-store'}); } catch(e){}
  if(was) setTimeout(startStream, 260);
}

document.getElementById('fs').onchange = e => {
  if(recOn){ toast('录制中不能改分辨率'); e.target.value = devFs; return; }
  apply('fs=' + e.target.value);
};
const q = document.getElementById('q'), qv = document.getElementById('qv');
q.oninput = ()=>{ qv.textContent = q.value; };
q.onchange = ()=> apply('quality=' + q.value);
const b = document.getElementById('b'), bv = document.getElementById('bv');
b.oninput = ()=>{ bv.textContent = b.value; };
b.onchange = ()=> fetch('/control?brightness=' + b.value);
const c = document.getElementById('c'), cv = document.getElementById('cv');
c.oninput = ()=>{ cv.textContent = c.value; };
c.onchange = ()=> fetch('/control?contrast=' + c.value);

const btnH = document.getElementById('btnH'), btnV = document.getElementById('btnV');
btnH.onclick = ()=>{ mirror = mirror ? 0 : 1; btnH.classList.toggle('on', !!mirror);
  fetch('/control?hmirror=' + mirror); };
btnV.onclick = ()=>{ vflip = vflip ? 0 : 1; btnV.classList.toggle('on', !!vflip);
  fetch('/control?vflip=' + vflip); };

const btnL = document.getElementById('btnL');
btnL.onclick = ()=>{
  const on = !btnL.classList.contains('on');
  btnL.classList.toggle('on', on);
  fetch('/control?flash=' + (on ? 1 : 0));
  toast(on ? '补光灯已打开' : '补光灯已关闭');
};

document.getElementById('btnR').onclick = ()=>{ startStream(); toast('正在重新连接画面'); };

document.getElementById('btnCap').onclick = async ()=>{
  toast('正在抓拍…');
  try{
    const r = await fetch('/capture?t=' + Date.now(), {cache:'no-store'});
    if(!r.ok) throw 0;
    const blob = await r.blob();
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'ESP32CAM_' + Date.now() + '.jpg';
    a.click();
    setTimeout(()=>URL.revokeObjectURL(a.href), 4000);
    toast('已保存到下载目录 (' + Math.round(blob.size/1024) + ' KB)');
  }catch(e){ toast('抓拍失败，请重试'); }
};

document.getElementById('btnSave').onclick = async ()=>{
  toast('正在写入 SD 卡…');
  try{
    const r = await fetch('/save', {cache:'no-store'});
    const j = await r.json();
    toast(j.ok ? ('已保存 ' + j.name) : ('失败: ' + j.msg));
  }catch(e){ toast('失败: 设备无响应'); }
};

btnRec.onclick = async ()=>{
  if(!recOn){
    const fps = document.getElementById('rfps').value;
    toast('正在开始录制…');
    try{
      const r = await fetch('/api/rec/start?fps=' + fps, {cache:'no-store'});
      const j = await r.json();
      if(!j.ok){ toast('录制失败: ' + j.msg); return; }
      recOn = true;
      btnRec.textContent = '停止录制';
      btnRec.classList.add('stop');
      recbadge.classList.add('on');
      toast('开始录制 → ' + j.name);
    }catch(e){ toast('录制失败: 设备无响应'); }
  } else {
    await stopRec();
  }
};

async function stopRec(){
  try{
    const r = await fetch('/api/rec/stop', {cache:'no-store'});
    const j = await r.json();
    toast(j.ok ? ('已保存 ' + j.name + '  ' + fsz(j.size)) : ('停止失败: ' + j.msg));
    if(j.ok && document.getElementById('pane-files').classList.contains('on')) loadDir(curPath);
  }catch(e){ toast('停止失败'); }
  recOn = false;
  btnRec.textContent = '开始录制';
  btnRec.classList.remove('stop');
  recbadge.classList.remove('on');
}

/* ---------------- 文件管理 ---------------- */
function crumbs(p){
  const parts = p.split('/').filter(x=>x);
  let h = '<a data-p="/">SD 卡</a>', acc = '';
  parts.forEach(x=>{ acc += '/' + x; h += '<i>/</i><a data-p="' + acc + '">' + esc(x) + '</a>'; });
  return h;
}
function extIcon(n){
  const e = n.split('.').pop().toLowerCase();
  if(['avi','mp4','mov','mjpg'].includes(e)) return {c:'v', t:'VID'};
  return {c:'', t:'JPG'};
}
function renderList(j){
  devFs = j.fs || devFs;
  document.getElementById('crumb').innerHTML = crumbs(j.path);
  document.getElementById('crumb').querySelectorAll('a').forEach(a=>{
    a.onclick = ()=> loadDir(a.dataset.p);
  });
  const box = document.getElementById('list');
  let h = '';
  if(j.path !== '/'){
    const up = j.path.replace(/\/[^/]+$/, '') || '/';
    h += '<div class="frow dir" data-nav="' + up + '"><span class="fic d">..</span>' +
         '<span class="fname">返回上级</span><span class="fsize"></span><span class="fops"></span></div>';
  }
  if(!j.items.length && j.path === '/'){
    h += '<div class="empty">SD 卡是空的</div>';
  }
  j.items.forEach(it=>{
    const ic = it.d ? {c:'d', t:'DIR'} : extIcon(it.n);
    h += '<div class="frow' + (it.d ? ' dir' : '') + '" data-n="' + esc(it.n) + '" data-d="' + (it.d?1:0) + '">' +
         '<span class="fic ' + ic.c + '">' + ic.t + '</span>' +
         '<span class="fname">' + esc(it.n) + '</span>' +
         '<span class="fsize">' + (it.d ? '' : fsz(it.s)) + '</span>' +
         '<span class="fops">' +
           (it.d ? '' : '<button data-act="dl">下载</button>') +
           '<button data-act="mv">改名</button>' +
           '<button data-act="rm">删除</button>' +
         '</span></div>';
  });
  box.innerHTML = h;
  document.getElementById('fmstat').textContent =
    '已用 ' + fsz(j.used * 1048576) + ' / ' + fsz(j.total * 1048576);
  document.getElementById('fmcount').textContent = j.items.length + ' 个项目';

  box.querySelectorAll('.frow').forEach(row=>{
    const name = row.dataset.n;
    const isDir = row.dataset.d === '1';
    const full = (j.path === '/' ? '' : j.path) + '/' + name;
    if(row.dataset.nav){ row.onclick = ()=> loadDir(row.dataset.nav); return; }
    if(isDir) row.onclick = e => { if(!e.target.closest('button')) loadDir(full); };
    row.querySelectorAll('button').forEach(btn=>{
      btn.onclick = async ev=>{
        ev.stopPropagation();
        const act = btn.dataset.act;
        if(act === 'dl'){
          const a = document.createElement('a');
          a.href = '/download?path=' + encodeURIComponent(full);
          a.download = name; a.click();
        } else if(act === 'rm'){
          if(!confirm('确定删除 ' + name + ' ?')) return;
          const r = await fetch('/api/delete?path=' + encodeURIComponent(full));
          const x = await r.json();
          toast(x.ok ? '已删除' : ('删除失败: ' + x.msg));
          loadDir(curPath);
        } else if(act === 'mv'){
          const nn = prompt('新名称', name);
          if(!nn || nn === name) return;
          const r = await fetch('/api/rename?path=' + encodeURIComponent(full) +
                                '&name=' + encodeURIComponent(nn));
          const x = await r.json();
          toast(x.ok ? '已重命名' : ('失败: ' + x.msg));
          loadDir(curPath);
        }
      };
    });
  });
}
async function loadDir(p){
  curPath = p || '/';
  try{
    const r = await fetch('/api/list?path=' + encodeURIComponent(curPath), {cache:'no-store'});
    const j = await r.json();
    if(!j.ok){ toast(j.msg || '读取失败'); return; }
    renderList(j);
  }catch(e){ toast('读取失败: 设备无响应'); }
}
document.getElementById('btnRefresh').onclick = ()=> loadDir(curPath);
document.getElementById('btnMkdir').onclick = async ()=>{
  const n = prompt('新文件夹名称');
  if(!n) return;
  const p = (curPath === '/' ? '' : curPath) + '/' + n;
  const r = await fetch('/api/mkdir?path=' + encodeURIComponent(p));
  const j = await r.json();
  toast(j.ok ? '已创建' : ('失败: ' + j.msg));
  loadDir(curPath);
};
document.getElementById('btnWipe').onclick = async ()=>{
  if(!confirm('确定清空 SD 卡全部内容？此操作不可恢复。')) return;
  if(!confirm('再次确认：真的删除所有文件？')) return;
  toast('正在清空…');
  const r = await fetch('/api/wipe?confirm=YES');
  const j = await r.json();
  toast(j.ok ? '已清空' : ('失败: ' + j.msg));
  loadDir('/');
};
const fileInput = document.getElementById('fileInput');
document.getElementById('btnUpload').onclick = ()=> fileInput.click();
fileInput.onchange = ()=>{
  const f = fileInput.files[0];
  if(!f) return;
  const bar = document.getElementById('upbar');
  bar.classList.add('on'); bar.style.width = '0%';
  const fd = new FormData();
  fd.append('file', f, f.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload?dir=' + encodeURIComponent(curPath));
  xhr.upload.onprogress = e=>{
    if(e.lengthComputable) bar.style.width = (e.loaded / e.total * 100) + '%';
  };
  xhr.onload = ()=>{
    bar.classList.remove('on'); bar.style.width = '0%';
    let ok = false;
    try { ok = JSON.parse(xhr.responseText).ok; } catch(e){}
    toast(ok ? ('已上传 ' + f.name) : '上传失败');
    loadDir(curPath);
  };
  xhr.onerror = ()=>{ bar.classList.remove('on'); toast('上传失败'); };
  xhr.send(fd);
  fileInput.value = '';
};

/* ---------------- 状态轮询 ---------------- */
async function poll(){
  try{
    const r = await fetch('/status', {cache:'no-store'});
    const j = await r.json();
    document.getElementById('s-ip').textContent = j.apip;
    document.getElementById('s-staip').textContent = j.staip || '未连接';
    document.getElementById('s-rssi').textContent = j.staip ? (j.rssi + ' dBm') : '-';
    document.getElementById('s-heap').textContent = Math.round(j.heap/1024) + ' KB';
    document.getElementById('s-sd').textContent = j.sd ? ('就绪 ' + j.sdfree + 'MB 可用') : '未检测到';
    document.getElementById('sub').textContent =
      'AP ' + j.ap + ' · STA ' + (j.staid || '未连接') + ' · 运行 ' + j.up + 's';
    devFs = j.fs;

    /* ---- 控件回填：另一端（小程序 / 另一台电脑）改了也要同步过来 ----
       正在拖动的控件不覆盖，避免手放开前被拉回去 ---- */
    const selFs = document.getElementById('fs');
    if(!recOn && devFs && selFs.value !== devFs && document.activeElement !== selFs) selFs.value = devFs;
    if(j.quality !== undefined && document.activeElement !== q && Number(q.value) !== Number(j.quality)){
      q.value = j.quality; qv.textContent = j.quality;
    }
    if(j.bri !== undefined && document.activeElement !== b && Number(b.value) !== Number(j.bri)){
      b.value = j.bri; bv.textContent = j.bri;
    }
    if(j.con !== undefined && document.activeElement !== c && Number(c.value) !== Number(j.con)){
      c.value = j.con; cv.textContent = j.con;
    }
    if(j.hm !== undefined){
      mirror = Number(j.hm) ? 1 : 0;
      btnH.classList.toggle('on', !!mirror);
    }
    if(j.vf !== undefined){
      vflip = Number(j.vf) ? 1 : 0;
      btnV.classList.toggle('on', !!vflip);
    }
    if(j.led !== undefined) btnL.classList.toggle('on', !!Number(j.led));

    const now = performance.now();
    if(lastT && j.frames >= lastFrames){
      const dt = (now - lastT) / 1000;
      document.getElementById('s-fps').textContent = ((j.frames - lastFrames)/dt).toFixed(1) + ' fps';
    }
    lastFrames = j.frames; lastT = now;

    if(j.rec){
      recOn = true;
      btnRec.textContent = '停止录制';
      btnRec.classList.add('stop');
      recbadge.classList.add('on');
      rectext.textContent = '录制中 ' + mmss(j.recsec) + ' · ' + j.recf + ' 帧';
      document.getElementById('fs').disabled = true;
    } else {
      if(recOn){ recOn = false; btnRec.textContent = '开始录制';
        btnRec.classList.remove('stop'); recbadge.classList.remove('on'); }
      document.getElementById('fs').disabled = false;
    }
  }catch(e){
    document.getElementById('sub').textContent = '设备无响应，请检查连接';
  }
}

startStream();
poll();
setInterval(poll, 1000);
</script>
</body>
</html>)rawliteral";

