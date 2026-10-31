#pragma once
#include <pgmspace.h>

// ======================================================================
//  Pagina principale di configurazione
// ======================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="it"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#05060f">
<title>SmallTV Ultra</title>
<style>
:root{--bg:#05060f;--card:#0e1230;--line:#242a5c;--tx:#f4f6ff;--dim:#8b93c4;--a1:#00e5ff;--a2:#ff2e93}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html{background:var(--bg)}
body{margin:0;min-height:100vh;color:var(--tx);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
background:radial-gradient(900px 480px at 100% -5%,color-mix(in srgb,var(--a2) 22%,transparent),transparent),
radial-gradient(800px 480px at -10% 0,color-mix(in srgb,var(--a1) 18%,transparent),transparent),var(--bg)}
.wrap{max-width:560px;margin:0 auto;padding:20px 16px 120px}
header{display:flex;align-items:center;justify-content:space-between;gap:12px}
h1{margin:0;font-size:26px;letter-spacing:.3px;background:linear-gradient(90deg,var(--a1),var(--a2));-webkit-background-clip:text;background-clip:text;color:transparent}
.chip{display:inline-flex;align-items:center;gap:8px;font-size:13px;color:var(--dim);border:1px solid var(--line);border-radius:99px;padding:5px 11px;background:#ffffff08}
.dot{width:8px;height:8px;border-radius:50%;background:#ff4d4d}.dot.on{background:#3ddc84;box-shadow:0 0 8px #3ddc84}
.card{background:linear-gradient(160deg,color-mix(in srgb,var(--card) 90%,#fff 6%),var(--card));border:1px solid var(--line);border-radius:22px;padding:18px;margin:14px 0}
h2{margin:0 0 12px;font-size:12px;letter-spacing:1.6px;text-transform:uppercase;color:var(--dim);font-weight:700}
.live{text-align:center;padding:22px 16px}
#tm{font-size:76px;font-weight:800;line-height:1;font-variant-numeric:tabular-nums;letter-spacing:1px;background:linear-gradient(90deg,var(--tx),var(--a1));-webkit-background-clip:text;background-clip:text;color:transparent}
#sec{font-size:22px;color:var(--a2);font-weight:700;margin-left:6px;font-variant-numeric:tabular-nums}
#ap{font-size:18px;color:var(--a1);font-weight:700;margin-left:6px}
.on-badge{display:none;margin-left:8px;font-size:11px;letter-spacing:.5px;color:#05060f;background:var(--a1);border-radius:99px;padding:2px 9px;vertical-align:middle}
.on-badge.show{display:inline-block}
#dt{color:var(--dim);text-transform:uppercase;letter-spacing:2px;font-size:13px;margin:6px 0 14px}
.wxrow{display:flex;justify-content:center;align-items:baseline;gap:14px;flex-wrap:wrap}
#wt{font-size:40px;font-weight:700}#wd{color:var(--a1);font-size:17px}
.sub{color:var(--dim);font-size:13px;margin-top:4px}
label{display:block;font-size:14px;color:var(--dim);margin:12px 0 6px}
input[type=text],input[type=number],input[type=password],select{width:100%;background:#00000055;border:1px solid var(--line);color:var(--tx);border-radius:14px;padding:12px 14px;font:inherit;outline:none}
input:focus,select:focus{border-color:var(--a1);box-shadow:0 0 0 3px color-mix(in srgb,var(--a1) 25%,transparent)}
.row{display:flex;gap:10px}.row>*{flex:1}.row>.fix{flex:0 0 auto}
.btn{appearance:none;border:0;border-radius:14px;padding:12px 16px;font:inherit;font-weight:700;color:#05060f;background:linear-gradient(90deg,var(--a1),var(--a2));cursor:pointer}
.btn.ghost{background:#ffffff0d;color:var(--tx);border:1px solid var(--line)}
.btn.warn{background:#ff4d4d22;color:#ff8a8a;border:1px solid #ff4d4d55}
.btn:active{transform:scale(.98)}
.res button{display:block;width:100%;text-align:left;margin:6px 0 0;background:#ffffff0a;border:1px solid var(--line);border-radius:12px;padding:10px 12px;color:var(--tx);font:inherit;cursor:pointer}
.res small{color:var(--dim)}
.themes{display:grid;grid-template-columns:repeat(auto-fit,minmax(96px,1fr));gap:10px}
.th{display:flex;align-items:center;gap:6px;background:#ffffff0a;border:2px solid var(--line);border-radius:14px;padding:10px;color:var(--tx);font:inherit;font-size:14px;cursor:pointer}
.th.on{border-color:var(--a1);background:#ffffff14}
.th i{width:14px;height:14px;border-radius:50%;display:inline-block}
.sw{display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid #ffffff10}.sw:last-child{border:0}
.sw span{font-size:15px}.sw small{display:block;color:var(--dim);font-size:12px}
input[type=checkbox]{width:46px;height:26px;appearance:none;background:#ffffff18;border-radius:99px;position:relative;cursor:pointer;transition:.2s;flex:none}
input[type=checkbox]::after{content:"";position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff;transition:.2s}
input[type=checkbox]:checked{background:var(--a1)}input[type=checkbox]:checked::after{left:23px}
input[type=range]{width:100%;accent-color:var(--a1)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.kv{font-size:13px;color:var(--dim)}.kv b{display:block;color:var(--tx);font-size:15px;font-weight:600}
.bar{position:fixed;left:0;right:0;bottom:0;padding:12px 16px calc(12px + env(safe-area-inset-bottom));background:linear-gradient(0deg,var(--bg) 60%,transparent);display:flex;justify-content:center}
.bar .btn{width:100%;max-width:528px;padding:15px;font-size:17px}
#toast{position:fixed;left:50%;bottom:90px;transform:translateX(-50%) translateY(20px);background:#000c;border:1px solid var(--line);padding:10px 18px;border-radius:99px;opacity:0;transition:.3s;pointer-events:none}
#toast.show{opacity:1;transform:translateX(-50%)}
a.btn{display:block;text-align:center;text-decoration:none}
</style></head><body>
<div class="wrap">
<header><div><h1>SmallTV Ultra</h1><div class="sub" id="ver">V.1.0.1</div></div><span class="chip"><i class="dot" id="dot"></i><span id="ip">...</span></span></header>

<section class="card live">
  <div><span id="tm">--:--</span><span id="sec"></span><span id="ap"></span></div>
  <div id="dt">&nbsp;</div>
  <div class="wxrow"><span id="wt">--&deg;</span><span id="wd">In attesa del meteo...</span></div>
  <div class="sub" id="wx2"></div>
</section>

<section class="card">
  <h2>Posizione meteo</h2>
  <div class="sub">Citt&agrave; attuale</div>
  <div style="font-size:22px;font-weight:700" id="cur">ROMA</div>
  <label for="q">Cerca un&rsquo;altra citt&agrave;</label>
  <div class="row"><input id="q" type="text" placeholder="es. Milano" autocomplete="off"><button class="btn fix" type="button" id="bs">Cerca</button></div>
  <div class="res" id="res"></div>
  <details style="margin-top:12px"><summary class="sub">Coordinate manuali</summary>
    <div class="row"><div><label>Latitudine</label><input id="lat" type="number" step="0.0001"></div><div><label>Longitudine</label><input id="lon" type="number" step="0.0001"></div></div>
  </details>
  <label for="wxm">Aggiorna il meteo ogni</label>
  <select id="wxm"><option value="10">10 minuti</option><option value="15">15 minuti</option><option value="30">30 minuti</option><option value="60">1 ora</option></select>
</section>

<section class="card">
  <h2>Aspetto</h2>
  <div class="themes" id="themes"></div>
  <label>Luminosit&agrave; <b id="brv" style="color:var(--tx)"></b></label>
  <input id="br" type="range" min="5" max="100" step="5">
  <label for="fmt">Formato dell&rsquo;ora</label>
  <select id="fmt"><option value="24">24 ore (es. 19:31)</option><option value="12">12 ore con AM/PM (es. 07:31 PM)</option></select>
</section>

<section class="card">
  <h2>Modalit&agrave; notturna <span class="on-badge" id="nmb">attiva ora</span></h2>
  <div class="sw"><span>Abbassa la luminosit&agrave; di notte<small>nella fascia oraria scelta qui sotto</small></span><input id="nm" type="checkbox"></div>
  <div class="row">
    <div><label for="ns">Dalle</label><input id="ns" type="time" value="22:00"></div>
    <div><label for="ne">Alle</label><input id="ne" type="time" value="07:00"></div>
  </div>
  <label>Luminosit&agrave; notturna <b id="nbv" style="color:var(--tx)"></b></label>
  <input id="nb" type="range" min="0" max="100" step="1">
  <div class="sub">0% = schermo spento durante la notte</div>
</section>

<section class="card">
  <h2>Schermo</h2>
  <div class="sw"><span>Ruota di 180&deg;<small>se il dispositivo &egrave; montato al contrario</small></span><input id="flip" type="checkbox"></div>
  <div class="sw"><span>Inverti colori<small>se i colori sembrano in negativo</small></span><input id="inv" type="checkbox"></div>
  <div class="sw"><span>Scambia rosso/blu<small>se rosso e blu sono invertiti</small></span><input id="swp" type="checkbox"></div>
</section>

<section class="card">
  <h2>Sistema</h2>
  <div class="grid" style="margin-bottom:14px">
    <div class="kv">Rete<b id="i_ssid">-</b></div><div class="kv">Segnale<b id="i_rssi">-</b></div>
    <div class="kv">Firmware<b id="i_fw">-</b></div><div class="kv">RAM libera<b id="i_heap">-</b></div>
  </div>
  <div class="grid">
    <button class="btn ghost" type="button" id="bref">Aggiorna meteo ora</button>
    <a class="btn ghost" href="/wifi">Cambia Wi-Fi</a>
    <a class="btn ghost" href="/update">Aggiorna firmware</a>
    <button class="btn ghost" type="button" id="breb">Riavvia</button>
  </div>
  <button class="btn warn" type="button" id="bforget" style="width:100%;margin-top:10px">Dimentica rete Wi-Fi</button>
</section>
</div>

<div class="bar"><button class="btn" type="button" id="bsave">Salva impostazioni</button></div>
<div id="toast"></div>

<script>
const $=s=>document.querySelector(s);
const TH=[
 {n:"Neon",a:"#00e5ff",b:"#ff2e93",bg:"#05060f",c:"#0e1230"},
 {n:"Aurora",a:"#2dffb2",b:"#7c5cff",bg:"#030f0e",c:"#0a2422"},
 {n:"Tramonto",a:"#ff9f1c",b:"#ff3d81",bg:"#120610",c:"#2a0f26"},
 {n:"Ghiaccio",a:"#7fd1ff",b:"#a78bfa",bg:"#06101c",c:"#0f2338"},
 {n:"Ambra",a:"#ffb000",b:"#ff6a00",bg:"#000000",c:"#14100a"}];
let C={city:"ROMA",lat:41.8919,lon:12.5113,theme:0,bright:80,wx:15,flip:false,invert:true,swap:false,h12:false,night:false,ns:"22:00",ne:"07:00",nb:10};
const strip=s=>s.normalize('NFD').replace(/[̀-ͯ]/g,'').replace(/[^\x20-\x7e]/g,'');
function toast(t){const e=$('#toast');e.textContent=t;e.className='show';clearTimeout(toast.t);toast.t=setTimeout(()=>e.className='',2200)}
function wxText(c){
 if(c===0)return'Sereno';if(c===1)return'Quasi sereno';if(c===2)return'Parzialmente nuvoloso';if(c===3)return'Coperto';
 if(c===45||c===48)return'Nebbia';if(c>=51&&c<=57)return'Pioviggine';if(c>=61&&c<=67)return'Pioggia';
 if(c>=71&&c<=77)return'Neve';if(c>=80&&c<=82)return'Rovesci';if(c===85||c===86)return'Rovesci di neve';
 if(c>=95)return'Temporale';return'--'}
function applyTheme(i){const t=TH[i]||TH[0],r=document.documentElement.style;
 r.setProperty('--a1',t.a);r.setProperty('--a2',t.b);r.setProperty('--bg',t.bg);r.setProperty('--card',t.c);
 document.querySelector('meta[name=theme-color]').content=t.bg}
function renderThemes(){const box=$('#themes');box.innerHTML='';
 TH.forEach((t,i)=>{const b=document.createElement('button');b.type='button';b.className='th'+(C.theme===i?' on':'');
  b.innerHTML='<i style="background:'+t.a+'"></i><i style="background:'+t.b+'"></i><span>'+t.n+'</span>';
  b.onclick=()=>{C.theme=i;applyTheme(i);renderThemes()};box.appendChild(b)})}
function render(){
 $('#cur').textContent=C.city;$('#lat').value=C.lat;$('#lon').value=C.lon;$('#wxm').value=String(C.wx);
 $('#br').value=C.bright;$('#brv').textContent=C.bright+'%';
 $('#flip').checked=!!C.flip;$('#inv').checked=!!C.invert;$('#swp').checked=!!C.swap;
 $('#fmt').value=C.h12?'12':'24';
 $('#nm').checked=!!C.night;$('#ns').value=C.ns;$('#ne').value=C.ne;$('#nb').value=C.nb;$('#nbv').textContent=C.nb+'%';
 applyTheme(C.theme);renderThemes()}
async function load(){try{const r=await fetch('/api/config',{cache:'no-store'});C=Object.assign(C,await r.json());render()}catch(e){toast('Impossibile leggere le impostazioni')}}
async function save(){
 C.bright=+$('#br').value;C.wx=+$('#wxm').value;C.flip=$('#flip').checked;C.invert=$('#inv').checked;C.swap=$('#swp').checked;
 C.h12=$('#fmt').value==='12';C.night=$('#nm').checked;C.ns=$('#ns').value||'22:00';C.ne=$('#ne').value||'07:00';C.nb=+$('#nb').value;
 C.lat=parseFloat($('#lat').value)||C.lat;C.lon=parseFloat($('#lon').value)||C.lon;
 try{const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(C)});
  toast(r.ok?'Salvato ✓':'Errore nel salvataggio');render()}catch(e){toast('Dispositivo non raggiungibile')}}
async function post(u){try{await fetch(u,{method:'POST'});return true}catch(e){return false}}
async function search(){
 const q=$('#q').value.trim();if(!q)return;const res=$('#res');res.textContent='Cerco...';
 try{const r=await fetch('https://geocoding-api.open-meteo.com/v1/search?name='+encodeURIComponent(q)+'&count=6&language=it&format=json');
  const j=await r.json();res.innerHTML='';
  if(!j.results||!j.results.length){res.textContent='Nessun risultato';return}
  j.results.forEach(o=>{const b=document.createElement('button');b.type='button';
   const n=document.createElement('span');n.textContent=o.name;const s=document.createElement('small');
   s.textContent='  '+[o.admin1,o.country].filter(Boolean).join(', ');b.append(n,s);
   b.onclick=()=>{C.city=strip(o.name).toUpperCase().slice(0,20);C.lat=o.latitude;C.lon=o.longitude;res.innerHTML='';$('#q').value='';render();toast('Città scelta: ricordati di salvare')};
   res.appendChild(b)})
 }catch(e){res.textContent='Ricerca non disponibile: serve internet su questo telefono/PC. Usa le coordinate manuali.'}}
async function poll(){
 try{const s=await(await fetch('/api/status',{cache:'no-store'})).json();
  $('#dot').className='dot on';$('#ip').textContent=s.ip;
  let hm=s.time.slice(0,5),ap='';
  if(s.synced&&C.h12){let h=+s.time.slice(0,2);ap=h>=12?'PM':'AM';h=h%12||12;hm=(h<10?'0':'')+h+s.time.slice(2,5)}
  $('#tm').textContent=s.synced?hm:'--:--';$('#sec').textContent=s.synced?s.time.slice(6):'';$('#ap').textContent=ap;
  $('#nmb').className='on-badge'+(s.nm?' show':'');
  if(s.synced){const p=s.date.split('-').map(Number);
   $('#dt').textContent=new Date(p[0],p[1]-1,p[2]).toLocaleDateString('it-IT',{weekday:'long',day:'numeric',month:'long'})}
  if(s.wx&&s.wx.ok){$('#wt').textContent=Math.round(s.wx.t)+'°';$('#wd').textContent=wxText(s.wx.code);
   $('#wx2').textContent='Min '+Math.round(s.wx.min)+'°  Max '+Math.round(s.wx.max)+'°  Umidità '+s.wx.hum+'%'}
  else{$('#wt').textContent='--°';$('#wd').textContent='In attesa del meteo...';$('#wx2').textContent=''}
  $('#i_ssid').textContent=s.ssid||'-';$('#i_rssi').textContent=s.rssi+' dBm';$('#i_fw').textContent='V.'+s.fw;$('#ver').textContent='V.'+s.fw;$('#i_heap').textContent=Math.round(s.heap/1024)+' KB'
 }catch(e){$('#dot').className='dot'}
 setTimeout(poll,2000)}
$('#bs').onclick=search;$('#q').onkeydown=e=>{if(e.key==='Enter')search()};
$('#bsave').onclick=save;$('#br').oninput=()=>$('#brv').textContent=$('#br').value+'%';$('#br').onchange=save;
$('#nb').oninput=()=>$('#nbv').textContent=$('#nb').value+'%';
$('#fmt').onchange=()=>{C.h12=$('#fmt').value==='12'};
$('#bref').onclick=async()=>{await post('/api/refresh');toast('Aggiornamento richiesto')};
$('#breb').onclick=async()=>{if(confirm('Riavviare SmallTV?')){await post('/api/reboot');toast('Riavvio...')}};
$('#bforget').onclick=async()=>{if(confirm('Dimenticare la rete Wi-Fi? Lo schermo mostrerà di nuovo la procedura di configurazione.')){await post('/api/forget');toast('Fatto, riavvio...')}};
load();poll();
</script></body></html>)rawliteral";

// ======================================================================
//  Pagina configurazione Wi-Fi (captive portal)
// ======================================================================
const char WIFI_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="it"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#05060f">
<title>SmallTV - Wi-Fi</title>
<style>
:root{--bg:#05060f;--card:#0e1230;--line:#242a5c;--tx:#f4f6ff;--dim:#8b93c4;--a1:#00e5ff;--a2:#ff2e93}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;min-height:100vh;color:var(--tx);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
background:radial-gradient(900px 480px at 100% -5%,#ff2e9338,transparent),radial-gradient(800px 480px at -10% 0,#00e5ff2e,transparent),var(--bg)}
.wrap{max-width:520px;margin:0 auto;padding:26px 16px 40px}
h1{margin:0 0 4px;font-size:28px;background:linear-gradient(90deg,var(--a1),var(--a2));-webkit-background-clip:text;background-clip:text;color:transparent}
p{color:var(--dim);margin:0 0 18px}
.card{background:var(--card);border:1px solid var(--line);border-radius:22px;padding:18px;margin:14px 0}
h2{margin:0 0 10px;font-size:12px;letter-spacing:1.6px;text-transform:uppercase;color:var(--dim)}
.net{display:flex;justify-content:space-between;align-items:center;width:100%;margin:6px 0 0;background:#ffffff0a;border:1px solid var(--line);border-radius:14px;padding:12px 14px;color:var(--tx);font:inherit;cursor:pointer;text-align:left}
.net.on{border-color:var(--a1);background:#ffffff16}
.net small{color:var(--dim)}
label{display:block;font-size:14px;color:var(--dim);margin:14px 0 6px}
input{width:100%;background:#00000055;border:1px solid var(--line);color:var(--tx);border-radius:14px;padding:13px 14px;font:inherit;outline:none}
input:focus{border-color:var(--a1);box-shadow:0 0 0 3px #00e5ff40}
.btn{appearance:none;width:100%;border:0;border-radius:14px;padding:15px;font:inherit;font-size:17px;font-weight:700;color:#05060f;background:linear-gradient(90deg,var(--a1),var(--a2));cursor:pointer;margin-top:18px}
.btn.ghost{background:#ffffff0d;color:var(--tx);border:1px solid var(--line);margin-top:10px;font-size:15px;padding:11px}
.btn:disabled{opacity:.5}
#msg{margin-top:14px;color:var(--a1);min-height:22px}
.show{font-size:13px;color:var(--dim);margin-top:8px;display:flex;gap:8px;align-items:center}.show input{width:auto}
</style></head><body><div class="wrap">
<h1>SmallTV Ultra</h1>
<p>Scegli la tua rete Wi-Fi: il display torner&agrave; da solo all&rsquo;orologio.</p>
<div class="card"><h2>Reti trovate</h2><div id="list">Cerco reti...</div><button class="btn ghost" type="button" id="rs">Cerca di nuovo</button></div>
<div class="card"><h2>Collega</h2>
<label for="ssid">Nome rete (SSID)</label><input id="ssid" type="text" autocapitalize="off" autocorrect="off" spellcheck="false">
<label for="pw">Password</label><input id="pw" type="password" autocomplete="off">
<label class="show"><input type="checkbox" id="sp"> Mostra password</label>
<button class="btn" type="button" id="go">Connetti e salva</button><div id="msg"></div></div>
</div>
<script>
const $=s=>document.querySelector(s);
function draw(n){const l=$('#list');l.innerHTML='';
 if(!n.length){l.textContent='Nessuna rete trovata: scrivi il nome a mano.';return}
 n.forEach(x=>{const b=document.createElement('button');b.type='button';b.className='net';
  const a=document.createElement('span');a.textContent=x.ssid;const s=document.createElement('small');
  s.textContent=(x.enc?'🔒 ':'')+x.rssi+' dBm';b.append(a,s);
  b.onclick=()=>{$('#ssid').value=x.ssid;document.querySelectorAll('.net').forEach(e=>e.classList.remove('on'));b.classList.add('on');$('#pw').focus()};
  l.appendChild(b)})}
async function scan(){$('#list').textContent='Cerco reti...';
 for(let i=0;i<14;i++){try{const j=await(await fetch('/api/scan',{cache:'no-store'})).json();if(j.networks){draw(j.networks);return}}catch(e){}
  await new Promise(r=>setTimeout(r,1200))}
 $('#list').textContent='Nessuna rete trovata: scrivi il nome a mano.'}
$('#rs').onclick=scan;
$('#sp').onchange=()=>$('#pw').type=$('#sp').checked?'text':'password';
$('#go').onclick=async()=>{const ssid=$('#ssid').value.trim();if(!ssid){$('#msg').textContent='Inserisci il nome della rete';return}
 $('#go').disabled=true;$('#msg').textContent='Salvo...';
 try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,pass:$('#pw').value})});
  if(!r.ok)throw 0;
  $('#msg').innerHTML='Salvato! SmallTV si riavvia e si collega a <b></b>.<br>Se la password &egrave; giusta vedrai l&rsquo;ora sul display. Poi riconnetti il telefono al tuo Wi-Fi e apri <b>http://smalltv.local</b> (o l&rsquo;IP mostrato sullo schermo) per personalizzarlo.';
  $('#msg b').textContent=ssid}
 catch(e){$('#msg').textContent='Errore: riprova.';$('#go').disabled=false}};
scan();
</script></body></html>)rawliteral";
