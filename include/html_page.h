#pragma once
#include <pgmspace.h>

// =====================================================================
// HTML_PAGE.H — Embedded web controller UI (stored in flash/PROGMEM)
// =====================================================================

static const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>BipedBot Controller</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#080810;--surf:#111120;--surf2:#1a1a2e;
  --acc:#7c6dfa;--acc2:#a78bfa;--acc3:#60a5fa;
  --stop:#ff4d6d;--go:#22d472;--warn:#f59e0b;
  --txt:#e2e8f0;--muted:#64748b;--border:rgba(255,255,255,0.07);
  --glow:rgba(124,109,250,0.35);
}
body{
  font-family:system-ui,-apple-system,sans-serif;
  background:var(--bg);color:var(--txt);
  min-height:100dvh;display:flex;flex-direction:column;
  align-items:center;justify-content:flex-start;
  padding:1rem;overflow-x:hidden;overflow-y:auto;
}
body::before{
  content:'';position:fixed;inset:0;pointer-events:none;
  background:
    linear-gradient(rgba(124,109,250,0.025) 1px,transparent 1px),
    linear-gradient(90deg,rgba(124,109,250,0.025) 1px,transparent 1px);
  background-size:36px 36px;
}
body::after{
  content:'';position:fixed;top:-15%;left:50%;transform:translateX(-50%);
  width:700px;height:280px;pointer-events:none;
  background:radial-gradient(ellipse,rgba(124,109,250,0.12) 0%,transparent 70%);
}
.wrap{width:100%;max-width:420px;display:flex;flex-direction:column;gap:.9rem;position:relative;z-index:1;padding-bottom:1.5rem}

/* Header */
.hdr{text-align:center;padding:.5rem 0}
.hdr h1{font-size:2rem;font-weight:800;letter-spacing:-.04em;background:linear-gradient(135deg,#a78bfa,#7c6dfa,#60a5fa);-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
.hdr p{color:var(--muted);font-size:.75rem;margin-top:.2rem;letter-spacing:.04em}

/* Status bar */
.status{display:flex;align-items:center;justify-content:space-between;background:var(--surf);border:1px solid var(--border);border-radius:12px;padding:.65rem 1rem}
.dot{width:8px;height:8px;border-radius:50%;background:var(--stop);box-shadow:0 0 8px var(--stop);transition:all .4s;flex-shrink:0}
.dot.on{background:var(--go);box-shadow:0 0 10px var(--go);animation:blink 2.5s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.35}}
.sinfo{display:flex;align-items:center;gap:.5rem}
.stxt{font-size:.78rem;color:var(--muted)}
.badge{font-size:.68rem;font-weight:700;padding:.18rem .6rem;border-radius:20px;background:rgba(124,109,250,.13);color:var(--acc2);border:1px solid rgba(124,109,250,.25);text-transform:uppercase;letter-spacing:.06em;transition:all .3s}
.badge.moving{background:rgba(34,212,114,.12);color:#22d472;border-color:rgba(34,212,114,.3)}
.badge.homing{background:rgba(245,158,11,.12);color:var(--warn);border-color:rgba(245,158,11,.3)}

/* D-Pad */
.dpad-card{background:var(--surf);border:1px solid var(--border);border-radius:20px;padding:1.2rem 1.5rem;display:flex;flex-direction:column;align-items:center;gap:.7rem}
.clabel{font-size:.68rem;text-transform:uppercase;letter-spacing:.1em;color:var(--muted);font-weight:600}
.dpad{display:grid;grid-template-columns:repeat(3,78px);grid-template-rows:repeat(3,78px);gap:5px}
.db{background:var(--surf2);border:1px solid var(--border);border-radius:14px;color:var(--txt);font-size:1.4rem;cursor:pointer;display:flex;align-items:center;justify-content:center;user-select:none;-webkit-user-select:none;touch-action:none;transition:transform .1s,background .12s,border-color .12s,box-shadow .12s;-webkit-tap-highlight-color:transparent}
.db:active,.db.hi{background:rgba(124,109,250,.2);border-color:var(--acc);box-shadow:0 0 22px var(--glow),inset 0 0 12px rgba(124,109,250,.1);transform:scale(.93)}
.up{grid-column:2;grid-row:1}.lft{grid-column:1;grid-row:2}
.ctr{grid-column:2;grid-row:2;background:var(--surf2);border:1px solid var(--border);border-radius:14px;display:flex;align-items:center;justify-content:center;opacity:.25;cursor:default}
.ctr::before{content:'';width:7px;height:7px;border-radius:50%;background:var(--muted)}
.rgt{grid-column:3;grid-row:2}.dn{grid-column:2;grid-row:3}

/* Action buttons */
.actions{display:grid;grid-template-columns:1fr 1fr 1fr;gap:.75rem}
.abtn{padding:.9rem;border-radius:16px;border:1px solid var(--border);cursor:pointer;font-size:.82rem;font-weight:700;letter-spacing:.04em;display:flex;align-items:center;justify-content:center;gap:.45rem;user-select:none;-webkit-user-select:none;transition:transform .12s,background .15s,box-shadow .15s;-webkit-tap-highlight-color:transparent}
.abtn:active{transform:scale(.95)}
.abtn-stop{background:rgba(255,77,109,.1);border-color:rgba(255,77,109,.25);color:var(--stop)}
.abtn-stop:active{background:rgba(255,77,109,.22);box-shadow:0 0 22px rgba(255,77,109,.3)}
.abtn-home{background:rgba(34,212,114,.1);border-color:rgba(34,212,114,.25);color:var(--go)}
.abtn-home:active{background:rgba(34,212,114,.22);box-shadow:0 0 22px rgba(34,212,114,.3)}
.abtn-sit{background:rgba(167,139,250,.1);border-color:rgba(167,139,250,.25);color:var(--acc2)}
.abtn-sit:active{background:rgba(167,139,250,.22);box-shadow:0 0 22px rgba(124,109,250,.35)}

/* Speed */
.speed-card{background:var(--surf);border:1px solid var(--border);border-radius:16px;padding:.9rem 1.1rem;display:flex;align-items:center;gap:.85rem}
.sp-label{font-size:.68rem;text-transform:uppercase;letter-spacing:.1em;color:var(--muted);font-weight:600;flex-shrink:0}
.sp-ctrl{display:flex;align-items:center;gap:.65rem;flex:1}
.sp-btn{width:34px;height:34px;border-radius:10px;border:1px solid var(--border);background:var(--surf2);color:var(--txt);font-size:1.1rem;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center;flex-shrink:0;user-select:none;-webkit-user-select:none;transition:all .12s;-webkit-tap-highlight-color:transparent}
.sp-btn:active{background:rgba(124,109,250,.2);border-color:var(--acc);transform:scale(.88)}
.sp-track{flex:1;height:4px;background:var(--surf2);border-radius:2px}
.sp-fill{height:100%;background:linear-gradient(90deg,var(--acc),var(--acc2));border-radius:2px;transition:width .35s cubic-bezier(.4,0,.2,1)}
.sp-val{font-size:.78rem;font-weight:700;color:var(--acc2);min-width:32px;text-align:right;flex-shrink:0}

/* Keyboard hint */
.hint{text-align:center;color:var(--muted);font-size:.68rem;opacity:.5;padding:.1rem 0}

/* ---- Servo gauges ---- */
.gauges-card{background:var(--surf);border:1px solid var(--border);border-radius:20px;padding:1rem 1.2rem}
.gauges-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:.5rem;margin-top:.75rem}
.gauge-wrap{display:flex;flex-direction:column;align-items:center;gap:.2rem}
.gauge-name{font-size:.6rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em;font-weight:600}
.gauge-val{font-size:.65rem;color:var(--acc2);font-weight:700}

/* ---- IMU panel ---- */
.imu-card{background:var(--surf);border:1px solid var(--border);border-radius:20px;padding:1rem 1.2rem;display:flex;flex-direction:column;gap:.75rem}
.imu-body{display:flex;gap:1rem;align-items:center}
.bubble-wrap{flex-shrink:0;display:flex;flex-direction:column;align-items:center;gap:.35rem}
.bubble-label{font-size:.6rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em;font-weight:600}
.imu-vals{flex:1;display:flex;flex-direction:column;gap:.4rem}
.imu-row{display:flex;justify-content:space-between;gap:.5rem}
.imu-cell{flex:1;background:var(--surf2);border:1px solid var(--border);border-radius:8px;padding:.35rem .5rem;display:flex;flex-direction:column;gap:.1rem}
.imu-axis{font-size:.55rem;color:var(--muted);text-transform:uppercase;letter-spacing:.07em}
.imu-num{font-size:.72rem;font-weight:700;color:var(--txt);font-variant-numeric:tabular-nums}
.imu-offline{font-size:.75rem;color:var(--muted);text-align:center;padding:.5rem;opacity:.6}
.pr-row{display:flex;gap:.6rem}
.pr-cell{flex:1;background:var(--surf2);border:1px solid var(--border);border-radius:10px;padding:.5rem .7rem;display:flex;justify-content:space-between;align-items:baseline}
.pr-lbl{font-size:.6rem;color:var(--muted);text-transform:uppercase;letter-spacing:.07em}
.pr-num{font-size:.9rem;font-weight:800;color:var(--acc2);font-variant-numeric:tabular-nums}
</style>
</head>
<body>
<div class="wrap">
  <div class="hdr">
    <h1>&#x1F916; BipedBot</h1>
    <p>6-SERVO WALKING ROBOT &mdash; WIRELESS CONTROLLER</p>
  </div>

  <div class="status">
    <div class="sinfo">
      <div class="dot" id="dot"></div>
      <span class="stxt" id="stxt">Connecting&hellip;</span>
    </div>
    <div class="badge" id="badge">IDLE</div>
  </div>

  <div class="dpad-card">
    <div class="clabel">Movement Control</div>
    <div class="dpad">
      <div class="db up"  id="bF" ontouchstart="hold('f',this)" ontouchend="rel(this)" onmousedown="hold('f',this)" onmouseup="rel(this)" onmouseleave="rel(this)">&#9650;</div>
      <div class="db lft" id="bL" ontouchstart="hold('l',this)" ontouchend="rel(this)" onmousedown="hold('l',this)" onmouseup="rel(this)" onmouseleave="rel(this)">&#9664;</div>
      <div class="ctr"></div>
      <div class="db rgt" id="bR" ontouchstart="hold('r',this)" ontouchend="rel(this)" onmousedown="hold('r',this)" onmouseup="rel(this)" onmouseleave="rel(this)">&#9654;</div>
      <div class="db dn"  id="bB" ontouchstart="hold('b',this)" ontouchend="rel(this)" onmousedown="hold('b',this)" onmouseup="rel(this)" onmouseleave="rel(this)">&#9660;</div>
    </div>
  </div>

  <div class="actions">
    <div class="abtn abtn-stop" id="bStop" onmousedown="cmd('s')" ontouchstart="cmd('s')">&#9632; STOP</div>
    <div class="abtn abtn-home" id="bHome" onmousedown="cmd('h')" ontouchstart="cmd('h')">&#8962; HOME</div>
    <div class="abtn abtn-sit"  id="bSit"  onmousedown="cmd('i')" ontouchstart="cmd('i')">&#129490; SIT</div>
  </div>

  <div class="speed-card">
    <div class="sp-label">Speed</div>
    <div class="sp-ctrl">
      <div class="sp-btn" onmousedown="cmd('-')" ontouchstart="cmd('-')">&#8722;</div>
      <div class="sp-track"><div class="sp-fill" id="spFill" style="width:50%"></div></div>
      <div class="sp-btn" onmousedown="cmd('+')" ontouchstart="cmd('+')">+</div>
    </div>
    <div class="sp-val" id="spVal">&mdash;</div>
  </div>

  <div class="hint">WASD / Arrows &bull; Space = stop &bull; H = home &bull; I = sit</div>

  <!-- ===== Servo Position Gauges ===== -->
  <div class="gauges-card">
    <div class="clabel">Live Servo Positions</div>
    <div class="gauges-grid" id="gaugesGrid">
      <!-- injected by JS -->
    </div>
  </div>

  <!-- ===== IMU Panel ===== -->
  <div class="imu-card">
    <div class="clabel">IMU &mdash; MPU6050 (D21/D22)</div>
    <div id="imuBody">
      <div class="imu-offline">Waiting for data&hellip;</div>
    </div>
  </div>

</div>

<script>
// ─── Constants ───────────────────────────────────────────────────────
const MIN_IV=2, MAX_IV=30;
// Servo limits matching config.h
const S_MIN=[1000,500,500,1170,1450,1700];
const S_MAX=[2500,2500,2500,2500,2500,2500];
const S_NAME=['S1','S2','S3','S4','S5\u2009slide','S6\u2009rot'];
// Distinct colours per servo
const S_COL=['#7c6dfa','#a78bfa','#60a5fa','#34d399','#f59e0b','#f472b6'];
const CIRC=2*Math.PI*26; // circumference for r=26

// ─── Build gauge SVGs ─────────────────────────────────────────────────
function buildGauges(){
  const g=document.getElementById('gaugesGrid');
  g.innerHTML='';
  for(let i=0;i<6;i++){
    const w=document.createElement('div');
    w.className='gauge-wrap';
    w.innerHTML=
      '<svg viewBox="0 0 72 72" width="72" height="72" style="overflow:visible">'+
        '<circle cx="36" cy="36" r="26" fill="none" stroke="#1a1a2e" stroke-width="7"/>'+
        '<circle id="ga'+i+'" cx="36" cy="36" r="26" fill="none" stroke="'+S_COL[i]+'" stroke-width="7"'+
          ' stroke-dasharray="'+CIRC+'" stroke-dashoffset="'+CIRC+'"'+
          ' stroke-linecap="round" transform="rotate(-90 36 36)"'+
          ' style="transition:stroke-dashoffset .18s ease,filter .18s"'+
          ' filter="drop-shadow(0 0 4px '+S_COL[i]+'66)"/>'+
        '<text x="36" y="33" text-anchor="middle" fill="#94a3b8" font-size="9" font-weight="600" font-family="system-ui">'+S_NAME[i]+'</text>'+
        '<text id="gv'+i+'" x="36" y="46" text-anchor="middle" fill="'+S_COL[i]+'" font-size="9" font-weight="700" font-family="system-ui">&mdash;</text>'+
      '</svg>';
    g.appendChild(w);
  }
}

function setGauge(i,pos){
  const pct=Math.max(0,Math.min(1,(pos-S_MIN[i])/(S_MAX[i]-S_MIN[i])));
  const offset=CIRC*(1-pct);
  document.getElementById('ga'+i).style.strokeDashoffset=offset;
  document.getElementById('gv'+i).textContent=Math.round(pct*100)+'%';
}

// ─── Build IMU HTML ───────────────────────────────────────────────────
function buildIMU(){
  document.getElementById('imuBody').innerHTML=
    '<div class="pr-row">'+
      '<div class="pr-cell"><span class="pr-lbl">Pitch</span><span class="pr-num" id="iPitch">&mdash;</span></div>'+
      '<div class="pr-cell"><span class="pr-lbl">Roll</span><span class="pr-num" id="iRoll">&mdash;</span></div>'+
    '</div>'+
    '<div class="imu-body">'+
      '<div class="bubble-wrap">'+
        '<svg id="bubble" viewBox="0 0 110 110" width="110" height="110">'+
          '<circle cx="55" cy="55" r="50" fill="none" stroke="#1a1a2e" stroke-width="2"/>'+
          '<circle cx="55" cy="55" r="25" fill="none" stroke="#1a1a2e" stroke-width="1" stroke-dasharray="3,3"/>'+
          '<line x1="5" y1="55" x2="105" y2="55" stroke="#1a1a2e" stroke-width="1"/>'+
          '<line x1="55" y1="5" x2="55" y2="105" stroke="#1a1a2e" stroke-width="1"/>'+
          '<circle id="ball" cx="55" cy="55" r="11" fill="rgba(124,109,250,0.75)" stroke="#a78bfa" stroke-width="2"/>'+
        '</svg>'+
        '<div class="bubble-label">Tilt Level</div>'+
      '</div>'+
      '<div class="imu-vals">'+
        '<div class="imu-row">'+mkCell('ax','Ax')+mkCell('ay','Ay')+mkCell('az','Az')+'</div>'+
        '<div class="imu-row">'+mkCell('gx','Gx')+mkCell('gy','Gy')+mkCell('gz','Gz')+'</div>'+
        '<div class="imu-row" style="grid-template-columns:1fr 1fr">'+mkCell('tmp','Temp','&mdash;&deg;C')+'</div>'+
      '</div>'+
    '</div>';
}
function mkCell(id,label,init){
  return '<div class="imu-cell">'+
    '<div class="imu-axis">'+label+'</div>'+
    '<div class="imu-num" id="'+id+'">'+(init||'&mdash;')+'</div>'+
  '</div>';
}

let imuBuilt=false;
function updateIMU(d){
  if(!d.imu){
    document.getElementById('imuBody').innerHTML='<div class="imu-offline">&#x26A0; MPU6050 not detected on D21/D22</div>';
    return;
  }
  if(!imuBuilt){buildIMU();imuBuilt=true;}
  document.getElementById('iPitch').textContent=d.pitch.toFixed(1)+'°';
  document.getElementById('iRoll').textContent=d.roll.toFixed(1)+'°';
  document.getElementById('ax').textContent=d.ax.toFixed(2)+' m/s²';
  document.getElementById('ay').textContent=d.ay.toFixed(2)+' m/s²';
  document.getElementById('az').textContent=d.az.toFixed(2)+' m/s²';
  document.getElementById('gx').textContent=d.gx.toFixed(2)+' r/s';
  document.getElementById('gy').textContent=d.gy.toFixed(2)+' r/s';
  document.getElementById('gz').textContent=d.gz.toFixed(2)+' r/s';
  document.getElementById('tmp').textContent=d.temp.toFixed(1)+'°C';
  // Bubble: map ±45° → ±38px
  const bx=Math.max(-38,Math.min(38,-d.roll/45*38));
  const by=Math.max(-38,Math.min(38,d.pitch/45*38));
  document.getElementById('ball').setAttribute('cx',55+bx);
  document.getElementById('ball').setAttribute('cy',55+by);
}

// ─── WebSocket ────────────────────────────────────────────────────────
const MIN_IVv=2,MAX_IVv=30;
let ws,rt,activeEl=null;

function connect(){
  ws=new WebSocket('ws://'+location.hostname+'/ws');
  ws.onopen=()=>{
    document.getElementById('dot').classList.add('on');
    document.getElementById('stxt').textContent='Connected \u2014 192.168.4.1';
    clearTimeout(rt);
  };
  ws.onclose=()=>{
    document.getElementById('dot').classList.remove('on');
    document.getElementById('stxt').textContent='Disconnected';
    rt=setTimeout(connect,2000);
  };
  ws.onerror=()=>ws.close();
  ws.onmessage=e=>{
    try{
      const d=JSON.parse(e.data);
      // State badge
      if(d.state!=null){
        const b=document.getElementById('badge');
        b.textContent=d.state;
        b.className='badge'+(d.state==='IDLE'?'':['HOME','SIT'].includes(d.state)?' homing':' moving');
      }
      // Speed bar
      if(d.speed!=null){
        const pct=((MAX_IVv-d.speed)/(MAX_IVv-MIN_IVv)*100).toFixed(1);
        document.getElementById('spFill').style.width=pct+'%';
        document.getElementById('spVal').textContent=d.speed+'ms';
      }
      // Servo gauges
      if(d.pos&&d.pos.length===6){d.pos.forEach((p,i)=>setGauge(i,p));}
      // IMU
      updateIMU(d);
    }catch(ex){}
  };
}

function cmd(c){if(ws&&ws.readyState===1)ws.send(c);}
function hold(c,el){if(activeEl&&activeEl!==el)rel(activeEl);activeEl=el;el.classList.add('hi');cmd(c);}
function rel(el){if(!el)return;el.classList.remove('hi');if(activeEl===el){activeEl=null;cmd('s');}}

// Keyboard
const KM={ArrowUp:'f',ArrowDown:'b',ArrowLeft:'l',ArrowRight:'r',
          'w':'f','W':'f','a':'l','A':'l','d':'r','D':'r','x':'b','X':'b',
          ' ':'s','Escape':'s','h':'h','H':'h','i':'i','I':'i'};
const MOTION=['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','w','W','a','A','d','D','x','X'];
const held=new Set();
document.addEventListener('keydown',e=>{if(held.has(e.key))return;held.add(e.key);if(KM[e.key]){cmd(KM[e.key]);e.preventDefault();}});
document.addEventListener('keyup',e=>{held.delete(e.key);if(MOTION.includes(e.key))cmd('s');});

buildGauges();
connect();
</script>
</body>
</html>
)rawhtml";
