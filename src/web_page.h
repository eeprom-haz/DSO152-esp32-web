// Browser viewer served by the ESP32. Draws the scope's own 300-point trace
// (frame v2 from main.cpp), so it matches the scope screen.
#pragma once
#include <pgmspace.h>

const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Web DSO-152</title>
<link rel="icon" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAABdCAYAAABafGNLAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAABJQSURBVHhe7Z1njFzVGYb5FyVKokBEMXIgRZFQ8isQ8gNFSiISEkWKAEUKJkQhFPc1nt2dXS8u2NjYphiMy64LxmDT7XWh9957LzbNpncwHQx80XPIOzpzuG3u1N3sK32a2bl37pxz3nO+87V7dwcbQlOxQ/jBEBqLIQKajCECmowhApqMAUHA9u3b7e2337ZnnnnG7r//frvlllvsqquusksvvdQuueQSu+yyy+zqq6+2W2+91R588EF77rnn3PlffPFFeKmWQ0sS8N5779lDDz1k69evt9NPP90mT55s48ePd3LMMcfYxIkTrVAofEP4nOOc19bWZlOnTrVFixbZxRdfbI888oi9//774U81HS1DwOuvv27XXnutnXbaaaWBZFAnTJjgBnTs2LE2ZsyYkvB3KOFxEcZ1eO3o6HCE3HTTTfbWW2+FTWgKmkoAqgWVwqAwWBpwBm/06NFuIMNBZGbz+bhx49y5fMYrf/M9jmuFaDVwPtfjuK7FOcuWLbOHH37Yvvrqq7BpDUNTCPj444/tuuuus+nTp5dmuwaJgdRnvD/++ONt+fLldvnll9tdd91lmzdvtpdeesnN4Hfffde2bdvmXt9880178cUX7amnnrI777zTqZ0lS5a432DguSbCNX0ykBNOOMHtH59//nnY1LqjoQSwKd544402bdo0NwuZneFgHHvssbZq1Sq755573KBWC2b3a6+9ZnfccYetWLHCuru7S78l0lk1tAeyOa+RaBgBjz/+uM2dO7c08KNGjXKzUWqHWY46+PTTT8Ov1hQfffSRU3u9vb2l35fKExHsQ88++2z41bqg7gTQ4fPPP98NMqIZT0e7urqcpfPGG2+EX2sIXn75Zbvwwgutvb29jAjtHbSt3mqprgRs2rTJZsyY4Tqo5S5dvGHDBmduVoPPPvvMrrzySlswf4HNnzffNm7YmMvURNVBhDZwGQC0+8QTT7QXXngh/ErNUDcC2GTpELMedSNr5owzznA6uVow+CeddJJN6pxkR7cdZUeOP8K6O7tt+rTpuU1MBnrx4sWlzZp202b+rtfeUBcCzj333LIlTQd6enrs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">
<style>
:root{--bg:#12151c;--panel:#1a1e28;--plot:#0b0e14;--text:#e2e8f0;--muted:#64748b}
*{box-sizing:border-box}
html,body{overflow-x:hidden}
body{margin:0;background:var(--bg);color:var(--text);font:14px system-ui,sans-serif;display:flex;flex-direction:column;height:100vh;height:100dvh}
header{display:flex;align-items:center;gap:10px;padding:8px 12px;background:var(--panel)}
#logo{width:36px;height:36px;border-radius:50%;background:#fff}
h1{font-size:17px;margin:0}
#set{font:bold 13px Consolas,monospace;color:#facc15}
#stat{font:12px Consolas,monospace;color:var(--muted);margin-left:auto}
#about,#setb{padding:6px 10px}
#wrap{flex:1;min-height:200px;margin:8px 8px 0}
canvas{width:100%;height:100%;display:block;background:var(--plot);border-radius:6px;touch-action:none}
#meas{padding:6px 12px 0;font:bold 13px Consolas,monospace;color:#38bdf8;display:flex;flex-wrap:wrap;gap:4px 10px}
.chip{white-space:nowrap}
#curs{padding:2px 12px 0;font:13px Consolas,monospace;color:#f0abfc;min-height:1em}
.bar{display:flex;flex-wrap:wrap;gap:6px;align-items:center;padding:8px}
.grp{display:flex;gap:2px}
.adj{display:contents}
#toolshdr{display:none}
button,select{background:#1e3a5f;color:#fff;border:0;border-radius:5px;padding:9px 12px;font:inherit;cursor:pointer;touch-action:manipulation;-webkit-tap-highlight-color:transparent}
button.t{background:#5f4b1e} button.g{background:#334155}
button.r{background:#7f1d1d} button.r.stopped{background:#166534}
button.on{outline:2px solid #facc15} button:disabled{opacity:.4;cursor:default}
button:active{filter:brightness(1.3)}
.lbl{font-size:12px;color:var(--muted);margin:0 2px 0 6px}
#tools{display:none;background:var(--panel);margin:0 8px 8px;border-radius:6px;padding:4px}
#tools.open{display:block}
#tools .bar{padding:4px 6px}
#aboutbox,#setbox,#probebox{display:none;position:fixed;inset:0;z-index:10;background:rgba(0,0,0,.6);align-items:center;justify-content:center;padding:16px}
#aboutbox.open,#setbox.open,#probebox.open{display:flex}
#aboutcard,#setcard,#probecard{background:var(--panel);border-radius:10px;padding:10px;max-width:360px;width:100%;max-height:90vh;overflow:auto;text-align:center;box-shadow:0 8px 30px rgba(0,0,0,.5)}
#setcard,#probecard{text-align:left;max-width:420px}
#aboutbox h2,#setbox h2,#probebox h2{font-size:15px;margin:6px 8px}
#aboutbox p,#setbox p,#probebox p{margin:6px 8px;color:var(--muted)}
#setform,#wifiform,#probeform{display:grid;grid-template-columns:auto 1fr;gap:5px 8px;align-items:center;font-size:12px;margin:4px 8px}
#setform label,#wifiform label,#probeform label{color:var(--muted);text-align:right}
#setform input,#setform select,#wifiform input,#wifiform select,#probeform input{width:100%;min-width:0;background:var(--plot);color:var(--text);border:1px solid #334155;border-radius:4px;padding:4px 6px;font:12px Consolas,monospace}
#setform input[type=checkbox],#wifiform input[type=checkbox],#probeform input[type=checkbox]{width:auto;justify-self:start}
#setform .btns,#wifiform .btns,#probeform .btns{grid-column:1/-1;display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
#setstatus{font:11px Consolas,monospace;color:#cbd5e1;margin:6px 8px;min-height:2.4em;white-space:pre-wrap}
#qr{display:block;margin:6px auto;background:#fff;border-radius:8px;padding:6px}
#bmc{display:inline-block;margin:6px 8px;background:#facc15;color:#12151c;border-radius:5px;padding:9px 12px;font:inherit;text-decoration:none;cursor:pointer}
#recinfo{font:12px Consolas,monospace;color:#cbd5e1;min-width:140px}
#scrub{flex:1;min-width:140px}
#more{margin-left:auto}
@media (max-width:600px){
header{flex-wrap:wrap;gap:6px;padding:6px 8px}
h1{display:none}
#set{order:9;width:100%;font-size:12px;line-height:1.3;overflow-wrap:anywhere}
#stat{order:1;font-size:11px;margin-left:auto}
#setb,#about{order:2;min-width:40px;min-height:40px;padding:0;display:inline-flex;align-items:center;justify-content:center;font-size:16px}
.w{display:none}
#about::before{content:"\24D8"}
#wrap{margin:6px 6px 0;min-height:120px}
#meas{display:grid;grid-template-columns:repeat(3,1fr);gap:4px 6px;padding:6px 8px 0;font-size:11px}
.chip{overflow:hidden;text-overflow:ellipsis}
#curs{padding:2px 8px 0;font-size:12px}
.bar{padding:6px;gap:8px}
.bar button{min-height:44px}
.bar.ctrl{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.adj{display:flex;flex-direction:column;gap:4px;min-width:0}
.lbl{margin:0;font-size:11px}
.adj .grp{display:grid;grid-template-columns:1fr 1fr;gap:4px}
.adj .grp button{min-height:44px;width:100%;padding:6px 4px}
#mode,#run,#more{min-height:44px;width:100%;padding:6px 4px}
#more{margin-left:0}
}
@media (orientation:landscape) and (max-height:500px){
body{display:grid;grid-template-columns:minmax(0,1fr) 220px;grid-template-rows:auto auto auto minmax(0,1fr);height:100vh;height:100dvh;overflow:hidden}
header{grid-column:2;grid-row:1;flex-wrap:wrap;gap:4px;padding:4px 6px}
#logo{width:26px;height:26px}
h1{display:none}
#set{order:9;width:100%;font-size:10px;line-height:1.25;overflow-wrap:anywhere}
#stat{order:1;font-size:10px;margin-left:auto}
#setb,#about{order:2;min-width:32px;min-height:32px;padding:0 4px;font-size:13px}
.w{display:none}
#about::before{content:"\24D8"}
#wrap{grid-column:1;grid-row:1/span 4;margin:4px;min-height:0;height:auto}
#meas{grid-column:2;grid-row:2;display:grid;grid-template-columns:1fr 1fr;gap:2px 6px;padding:4px 6px 0;font-size:10px}
#curs{grid-column:2;grid-row:3;padding:2px 6px 0;font-size:10px}
.bar.ctrl{grid-column:2;grid-row:4;display:grid;grid-template-columns:repeat(3,1fr);gap:4px;padding:4px;align-content:start;overflow:auto;min-height:0}
.adj{display:flex;flex-direction:column;gap:2px;min-width:0}
.lbl{margin:0;font-size:9px}
.adj .grp{display:grid;grid-template-columns:1fr 1fr;gap:2px}
.adj .grp button{min-height:36px;width:100%;padding:2px}
#mode,#run,#more{min-height:36px;width:100%;padding:4px 2px;font-size:11px}
#more{margin-left:0}
button,select{padding:4px 6px}
.chip{overflow:hidden;text-overflow:ellipsis}
}
@media (max-width:600px),(orientation:landscape) and (max-height:500px){
#tools{position:fixed;left:0;right:0;bottom:0;z-index:5;max-height:60vh;overflow:auto;border-radius:12px 12px 0 0;box-shadow:0 -8px 30px rgba(0,0,0,.6);margin:0;padding:0 0 8px}
#toolshdr{display:flex;position:sticky;top:0;z-index:1;align-items:center;gap:8px;padding:8px 12px;background:var(--panel);border-radius:12px 12px 0 0;border-bottom:1px solid #334155}
#toolshdr b{font-size:14px}
#toolsclose{margin-left:auto;min-width:40px;min-height:40px;padding:0;display:inline-flex;align-items:center;justify-content:center;font-size:16px}
#tools .bar{gap:8px;padding:6px 8px}
}
</style></head><body>
<header><img id="logo" alt="" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAABdCAYAAABafGNLAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAABJQSURBVHhe7Z1njFzVGYb5FyVKokBEMXIgRZFQ8isQ8gNFSiISEkWKAEUKJkQhFPc1nt2dXS8u2NjYphiMy64LxmDT7XWh9957LzbNpncwHQx80XPIOzpzuG3u1N3sK32a2bl37pxz3nO+87V7dwcbQlOxQ/jBEBqLIQKajCECmowhApqMAUHA9u3b7e2337ZnnnnG7r//frvlllvsqquusksvvdQuueQSu+yyy+zqq6+2W2+91R588EF77rnn3PlffPFFeKmWQ0sS8N5779lDDz1k69evt9NPP90mT55s48ePd3LMMcfYxIkTrVAofEP4nOOc19bWZlOnTrVFixbZxRdfbI888oi9//774U81HS1DwOuvv27XXnutnXbaaaWBZFAnTJjgBnTs2LE2ZsyYkvB3KOFxEcZ1eO3o6HCE3HTTTfbWW2+FTWgKmkoAqgWVwqAwWBpwBm/06NFuIMNBZGbz+bhx49y5fMYrf/M9jmuFaDVwPtfjuK7FOcuWLbOHH37Yvvrqq7BpDUNTCPj444/tuuuus+nTp5dmuwaJgdRnvD/++ONt+fLldvnll9tdd91lmzdvtpdeesnN4Hfffde2bdvmXt9880178cUX7amnnrI777zTqZ0lS5a432DguSbCNX0ykBNOOMHtH59//nnY1LqjoQSwKd544402bdo0NwuZneFgHHvssbZq1Sq755573KBWC2b3a6+9ZnfccYetWLHCuru7S78l0lk1tAeyOa+RaBgBjz/+uM2dO7c08KNGjXKzUWqHWY46+PTTT8Ov1hQfffSRU3u9vb2l35fKExHsQ88++2z41bqg7gTQ4fPPP98NMqIZT0e7urqcpfPGG2+EX2sIXn75Zbvwwgutvb29jAjtHbSt3mqprgRs2rTJZsyY4Tqo5S5dvGHDBmduVoPPPvvMrrzySlswf4HNnzffNm7YmMvURNVBhDZwGQC0+8QTT7QXXngh/ErNUDcC2GTpELMedSNr5owzznA6uVow+CeddJJN6pxkR7cdZUeOP8K6O7tt+rTpuU1MBnrx4sWlzZp202b+rtfeUBcCzj333LIlTQd6enrs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"><h1>Web DSO-152</h1><span id="set">connecting...</span><span id="stat"></span><button class="g" id="setb">&#9881;<span class="w"> Settings</span></button><button class="g" id="about"><span class="w">About</span></button></header>
<div id="wrap"><canvas id="c"></canvas></div>
<div id="meas"></div>
<div id="curs"></div>
<div class="bar ctrl">
<div class="adj"><span class="lbl">V/div</span><span class="grp"><button data-adj="2 3">&#9660;</button><button data-adj="2 2">&#9650;</button></span></div>
<div class="adj"><span class="lbl">Time</span><span class="grp"><button data-adj="4 3">&#9660;</button><button data-adj="4 2">&#9650;</button></span></div>
<div class="adj"><span class="lbl">Trig</span><span class="grp"><button class="t" data-adj="6 3">&#9660;</button><button class="t" data-adj="6 2">&#9650;</button></span></div>
<button class="t" id="mode">Mode</button>
<button class="r" id="run">STOP</button>
<button class="g" id="more">&#8943; Tools</button>
</div>
<div id="tools">
<div id="toolshdr"><b>Tools</b><button class="g" id="toolsclose">&#10005;</button></div>
<div class="bar">
<span class="lbl">0 V</span><span class="grp"><button class="g" id="zd">&#9660;</button><button class="g" id="zu">&#9650;</button></span>
<span class="lbl">Probe</span><select id="probe"></select><button class="g" id="probeedit">Edit</button>
<button class="g" id="ct">Time cursors</button><button class="g" id="cv">Volt cursors</button>
<button class="g" id="pause">Freeze view</button>
<span class="lbl">fps</span><select id="fps"><option>5</option><option>10</option><option selected>15</option><option>20</option></select>
<button class="g" id="png">PNG</button><button class="g" id="csv">CSV</button>
</div>
<div class="bar">
<button class="r" id="rec">&#9679; Record</button><span id="recinfo">0 captures</span>
<button class="g" id="play" disabled>&#9654;</button>
<input type="range" id="scrub" min="0" max="0" value="0" disabled>
<button class="g" id="live" disabled>Live</button>
<button class="g" id="rcsv" disabled>Export</button><button class="g" id="rclr" disabled>Clear</button>
</div>
</div>
<div id="aboutbox">
<div id="aboutcard">
<h2>Web DSO-152</h2>
<p>Wi-Fi viewer for the FNIRSI DSO152. An ESP32 reads the scope over SWD; the scope keeps its stock firmware.</p>
<p>Open http://dso152.local or the IP shown on the serial log. Wi-Fi setup: join the DSO152-Setup network.</p>
<p>Scan to buy me a coffee:</p>
<img id="qr" alt="Buy me a coffee QR code" width="160" height="160">
<a id="bmc" target="_blank" rel="noopener">&#9749; Buy me a coffee</a>
<button class="g" id="aboutclose">Close</button>
</div>
</div>
<div id="setbox">
<div id="setcard">
<h2>Settings</h2>
<form id="wifiform">
<div id="wifinow" style="grid-column:1/-1">Loading...</div>
<div style="grid-column:1/-1;color:var(--muted)">Saved networks</div>
<div id="wifilist" style="grid-column:1/-1"></div>
<label for="w_ssid">Network</label>
<div style="display:flex;gap:5px;min-width:0">
<select id="w_ssid"><option value="">Other...</option></select><button type="button" class="g" id="w_scan">Scan</button>
</div>
<input id="w_other" type="text" autocomplete="off" autocapitalize="none" placeholder="network name (or Scan)" style="grid-column:1/-1">
<label for="w_pass">Password</label><input id="w_pass" type="password" autocomplete="new-password">
<div class="btns">
<button type="submit" class="g">Add network</button>
<button type="button" class="g" id="w_connect" hidden>Connect now</button>
</div>
<div style="grid-column:1/-1;color:var(--muted);font-size:11px;line-height:1.3">Joins the strongest saved network in range. If none is found it makes its own hotspot DSO152 (password dso152scope), page at 192.168.4.1.</div>
</form>
<form id="setform">
<label for="s_log_on">Log to FTP</label><input type="checkbox" id="s_log_on">
<label for="s_log_int">Log every</label><select id="s_log_int"><option value="10">10 s</option><option value="30" selected>30 s</option><option value="60">60 s</option><option value="300">5 min</option></select>
<label for="s_ftp_host">FTP host</label><input id="s_ftp_host" type="text" autocomplete="off" autocapitalize="none">
<label for="s_ftp_port">FTP port</label><input id="s_ftp_port" type="number" min="1" max="65535">
<label for="s_ftp_user">FTP user</label><input id="s_ftp_user" type="text" autocomplete="off" autocapitalize="none">
<label for="s_ftp_pass">FTP password</label><input id="s_ftp_pass" type="password" autocomplete="new-password">
<label for="s_ftp_dir">Folder</label><input id="s_ftp_dir" type="text" autocomplete="off" autocapitalize="none">
<label for="s_ftp_every">Upload every</label><select id="s_ftp_every"><option value="15">15 min</option><option value="30">30 min</option><option value="60" selected>1 h</option><option value="120">2 h</option><option value="360">6 h</option></select>
<label for="s_mqtt_on">Enable MQTT</label><input type="checkbox" id="s_mqtt_on">
<label for="s_mqtt_host">Broker host</label><input id="s_mqtt_host" type="text" autocomplete="off" autocapitalize="none">
<label for="s_mqtt_port">Broker port</label><input id="s_mqtt_port" type="number" min="1" max="65535">
<label for="s_mqtt_user">Broker user</label><input id="s_mqtt_user" type="text" autocomplete="off" autocapitalize="none">
<label for="s_mqtt_pass">Broker password</label><input id="s_mqtt_pass" type="password" autocomplete="new-password">
<label for="s_mqtt_topic">Topic</label><input id="s_mqtt_topic" type="text" autocomplete="off" autocapitalize="none">
<label for="s_mqtt_int">Send every</label><select id="s_mqtt_int"><option value="1">1 s</option><option value="2">2 s</option><option value="5" selected>5 s</option><option value="10">10 s</option></select>
<div class="btns">
<button type="submit" class="g">Save</button>
<button type="button" class="g" id="setupload">Upload now</button>
<button type="button" class="g" id="setdownload">Download log</button>
<button type="button" class="g" id="setclear">Clear log</button>
<button type="button" class="g" id="setclose">Close</button>
</div>
</form>
<div id="setstatus">...</div>
</div>
</div>
<div id="probebox">
<div id="probecard">
<h2>Probe</h2>
<form id="probeform">
<label for="p_name">Name</label><input id="p_name" type="text" autocomplete="off">
<label for="p_unit">Unit</label><input id="p_unit" type="text" autocomplete="off">
<label for="p_mv">mV per unit</label><input id="p_mv" type="number" step="any" min="0.001">
<label for="p_z">0 at (V)</label><input id="p_z" type="number" step="any">
<label for="p_inv">Invert</label><input type="checkbox" id="p_inv">
<div class="btns">
<button type="submit" class="g">Save</button>
<button type="button" class="g" id="probedel">Delete</button>
<button type="button" class="g" id="probeclose">Close</button>
</div>
</form>
</div>
</div>
<script>
// Frame v2 from the ESP32: the scope's own 300-point on-screen trace (pixels
// from zero, 25 px/div, 12 x 8 divisions) plus settings and measurements.
const VOLTS=["10mV","20mV","50mV","100mV","200mV","500mV","1V","2V","5V","10V","20V"];
const VPD=[0.01,0.02,0.05,0.1,0.2,0.5,1,2,5,10,20];
const DIV=[1600,800,400,200,100,40,20,8,4,2,1];
const TIMES=["10us","20us","50us","100us","200us","500us","1ms","2ms","5ms","10ms","20ms","50ms",
"100ms","200ms","500ms","1s","2s","5s","10s","20s","50s","100s","200s","500s"];
const SPD=[1e-5,2e-5,5e-5,1e-4,2e-4,5e-4,1e-3,2e-3,5e-3,.01,.02,.05,.1,.2,.5,1,2,5,10,20,50,100,200,500];
const MODES=["AUTO","NORMAL","SINGLE"], P=300, PX_DIV=25, HD=P/PX_DIV, VD=8, MAX_REC=9000;
const BMC_URL="https://buymeacoffee.com/eeprom_haz"; // Buy Me a Coffee link
const $=id=>document.getElementById(id), cv=$("c"), cx=cv.getContext("2d");
let ws, f=null, held=null, frozen=false, zeroDiv=3, frames=0, tf=performance.now(), view=null, zx=1, zv=1, ox=0, oy=0, badge=null;
const cur={t:false,v:false,t1:.3,t2:.7,v1:.35,v2:.65}; let drag=null;
const ptrs=new Map(); let pan=null, lastTap=null;
const rec={on:false,frames:[],t0:0,idx:-1,timer:null,last:""};

function decode(buf){
  const d=new DataView(buf); if(buf.byteLength<716||d.getUint8(2)!=2||!(d.getUint8(3)&1)) return null;
  const st=new Uint8Array(buf,8,32);
  const probe=st[3]==1?10:1, cpl=Math.min(st[4],1), vi=Math.min(st[16],10), ti=Math.min(st[17],23);
  const k=20/DIV[vi]/100*probe, cpp=vi==7?16/5:4;     // volts per count, counts per pixel
  const meas=[]; for(let i=0;i<6;i++) meas.push(d.getInt32(84+4*i,true));
  const toV=x=>x/DIV[vi]/100*probe, volts=new Float32Array(P), px=new Int16Array(P);
  for(let i=0;i<P;i++){px[i]=d.getInt16(116+2*i,true); volts[i]=px[i]*cpp*k;}
  return {probe,cpl,vi,ti,vdiv:VPD[vi]*probe,tdiv:SPD[ti],dt:SPD[ti]/PX_DIV,volts,px,
    vave:toV(meas[0]),vmax:toV(meas[1]),vmin:toV(meas[2]),vpp:toV(meas[3]),vrms:toV(meas[5]),
    trig:d.getInt32(16,true)*4*k,mode:st[19],running:st[2]==1};
}
function analyse(v,dt){
  let lo=Infinity,hi=-Infinity; for(const x of v){if(x<lo)lo=x;if(x>hi)hi=x;}
  const r={}; if(hi-lo<1e-9) return r;
  const mid=(lo+hi)/2,hy=(hi-lo)*.1,up=[],dn=[]; let s=v[0]>mid?1:0;
  for(let i=1;i<v.length;i++){ if(s==0&&v[i]>mid+hy){up.push(i);s=1;} else if(s==1&&v[i]<mid-hy){dn.push(i);s=0;} }
  if(up.length>=2){r.period=(up[up.length-1]-up[0])*dt/(up.length-1); r.freq=1/r.period;
    const u=up[0], q=dn.find(x=>x>u); if(q!==undefined) r.duty=(q-u)*dt/r.period*100;}
  return r;
}
const ft=t=>{const a=Math.abs(t);return a>=1?t.toPrecision(3)+"s":a>=1e-3?(t*1e3).toPrecision(3)+"ms":(t*1e6).toPrecision(3)+"us";};
const ff=x=>x>=1e3?(x/1e3).toFixed(3)+"kHz":x.toFixed(2)+"Hz";
const fv=x=>x.toFixed(2)+"V";
const BUILTIN=[
  {name:"Voltage",unit:"V",mv:1000,z:0,inv:false},
  {name:"Clamp 100mV/A",unit:"A",mv:100,z:0,inv:false},
  {name:"Clamp 10mV/A",unit:"A",mv:10,z:0,inv:false},
  {name:"Clamp 1mV/A",unit:"A",mv:1,z:0,inv:false},
  {name:"Pressure 0-100psi 0.5-4.5V",unit:"psi",mv:40,z:0.5,inv:false}
];
let customProbes=[], probes=[], selProbe=null;
function normProbe(p){return {name:String(p.name),unit:String(p.unit||"V"),mv:Math.abs(+p.mv)||1000,z:+p.z||0,inv:!!p.inv,custom:true};}
function rebuildProbes(){probes=BUILTIN.map(p=>({...p}));customProbes.forEach(c=>{const i=probes.findIndex(p=>p.name==c.name);if(i>=0)probes[i]=c;else probes.push(c);});}
function fillProbeSelect(){const s=$("probe");s.innerHTML="";probes.forEach(p=>{const o=document.createElement("option");o.value=o.textContent=p.name;s.appendChild(o);});s.value=selProbe.name;}
function loadProbes(){let n="Voltage";try{const a=JSON.parse(localStorage.getItem("dso152_probes")||"[]");customProbes=Array.isArray(a)?a.filter(p=>p&&p.name).map(normProbe):[];n=localStorage.getItem("dso152_probe")||"Voltage";}catch(e){customProbes=[];}rebuildProbes();selProbe=probes.find(p=>p.name==n)||probes[0];fillProbeSelect();}
function saveProbeStore(){try{localStorage.setItem("dso152_probes",JSON.stringify(customProbes.map(({name,unit,mv,z,inv})=>({name,unit,mv,z,inv}))));localStorage.setItem("dso152_probe",selProbe.name);}catch(e){}}
const pv=v=>(selProbe.inv?-1:1)*(v-selProbe.z)/(selProbe.mv/1000);
const pdiff=v=>(selProbe.inv?-1:1)*v/(selProbe.mv/1000);
const pdiv=vdiv=>vdiv/(selProbe.mv/1000);
function fu(x){
  const a=Math.abs(x); let div=1,pre="";
  if(a>=1e9){div=1e9;pre="G";}else if(a>=1e6){div=1e6;pre="M";}else if(a>=1e3){div=1e3;pre="k";}
  else if(a&&a<1e-6){div=1e-9;pre="n";}else if(a&&a<1e-3){div=1e-6;pre="u";}else if(a&&a<1){div=1e-3;pre="m";}
  return String(+(x/div).toPrecision(3))+pre+selProbe.unit;   // 3 significant digits, no trailing zeros
}

function draw(){
  const r=cv.getBoundingClientRect(), dpr=devicePixelRatio||1;
  if(cv.width!=Math.round(r.width*dpr)||cv.height!=Math.round(r.height*dpr)){cv.width=Math.round(r.width*dpr);cv.height=Math.round(r.height*dpr);}
  const w=cv.width,h=cv.height; cx.fillStyle="#0b0e14"; cx.fillRect(0,0,w,h);
  const s=f; if(!s){badge=null;return;}
  const t=held||s;                                   // trace source (playback/freeze)
  const ppd=h/VD, zy=h-zeroDiv*ppd, ppv=ppd/s.vdiv, xs=i=>w*i/(P-1);
  view={zy,ppv,w,h,dt:t.dt,seg:t.volts,zx,zv,ox,oy};
  cx.lineWidth=dpr; cx.font=(11*dpr)+"px Consolas,monospace";
  cx.setLineDash([2*dpr,4*dpr]); cx.strokeStyle="#1e293b";
  for(let i=1;i<HD;i++){const x=w*i/HD;cx.beginPath();cx.moveTo(x,0);cx.lineTo(x,h);cx.stroke();}
  cx.fillStyle="#64748b";
  for(let i=1;i<VD;i++){const y=h*i/VD;cx.beginPath();cx.moveTo(0,y);cx.lineTo(w,y);cx.stroke();
    cx.fillText(fu(pv((zy-(y/zv+oy*h))/ppv)),4*dpr,y-4*dpr);}
  cx.setLineDash([]); cx.strokeStyle="#475569";
  const zy2=(zy-oy*h)*zv; if(zy2>=0&&zy2<=h){cx.beginPath();cx.moveTo(0,zy2);cx.lineTo(w,zy2);cx.stroke();}

  cx.strokeStyle="#22c55e"; cx.lineWidth=2*dpr; cx.beginPath();
  for(let i=0;i<P;i++){const x=(xs(i)-ox*w)*zx, yy=(zy-t.volts[i]*(ppd/t.vdiv)-oy*h)*zv, y=(zx==1&&zv==1)?Math.max(-2,Math.min(h+2,yy)):yy; i?cx.lineTo(x,y):cx.moveTo(x,y);}
  cx.stroke();

  const ty=(zy-s.trig*ppv-oy*h)*zv;                  // trigger line is always live
  const tTxt="T "+fu(pv(s.trig));
  if(ty>=0&&ty<=h){cx.setLineDash([6*dpr,4*dpr]);cx.strokeStyle="#facc15";cx.lineWidth=dpr;
    cx.beginPath();cx.moveTo(0,ty);cx.lineTo(w,ty);cx.stroke();cx.setLineDash([]);
    cx.fillStyle="#facc15";cx.textAlign="right";cx.fillText(Math.abs(ty-zy2)<2*dpr?"T":tTxt,w-4*dpr,ty-4*dpr);cx.textAlign="left";}

  if(zx>1||zv>1){
    const txt=`Zoom t×${+zx.toFixed(2)} V×${+zv.toFixed(2)}  ${ft(s.tdiv/zx)}/div  ${fu(pdiff(s.vdiv/zv))}/div`;
    const tw=cx.measureText(txt).width, bw=tw+10*dpr, bh=16*dpr, by=4*dpr;
    const bx=(ty>=0&&ty<=h&&ty<by+bh+16*dpr)?4*dpr:w-bw-4*dpr;
    cx.fillStyle="rgba(15,23,42,.88)"; cx.fillRect(bx,by,bw,bh);
    cx.strokeStyle="#475569"; cx.lineWidth=dpr; cx.strokeRect(bx+.5,by+.5,bw-1,bh-1);
    cx.fillStyle="#e2e8f0"; cx.textAlign="left"; cx.textBaseline="middle"; cx.fillText(txt,bx+5*dpr,by+bh/2); cx.textBaseline="alphabetic";
    badge={x:bx/dpr,y:by/dpr,w:bw/dpr,h:bh/dpr};
  }else badge=null;

  drawCursors(dpr);
  const a=analyse(t.volts,t.dt);
  const tag=rec.idx>=0?`  PLAYBACK ${rec.idx+1}/${rec.frames.length}`:frozen?"  FROZEN":"";
  $("run").textContent=s.running?"STOP":"RUN"; $("run").classList.toggle("stopped",!s.running);
  $("mode").textContent=MODES[s.mode]||"Mode";
  const perDiv=selProbe.name=="Voltage"?VOLTS[s.vi]+"/div"+(s.probe==10?" x10":""):fu(pdiv(s.vdiv))+"/div";
  $("set").textContent=`${s.running?"RUN":"STOP"}  ${perDiv}  ${TIMES[s.ti]}/div  ${MODES[s.mode]||s.mode} ${fu(pv(s.trig))}${tag}`;
  const ml=selProbe.unit=="V"?["Vpp","Vmax","Vmin","Vavg","Vrms"]:selProbe.unit=="A"?["Ipp","Imax","Imin","Iavg","Irms"]:["Pp","Max","Min","Avg","Rms"];
  const ms=$("meas"); ms.textContent="";
  [[ml[0],fu(pdiff(t.vpp))],[ml[1],fu(pv(t.vmax))],[ml[2],fu(pv(t.vmin))],[ml[3],fu(pv(t.vave))],[ml[4],fu(pv(t.vrms))],
   ["Freq",a.freq?ff(a.freq):"--"],["Duty",a.duty!=null?a.duty.toFixed(1)+"%":"--"]].forEach(([k,v],i)=>{
    if(i)ms.appendChild(document.createTextNode(" "));
    const c=document.createElement("span"); c.className="chip";
    const b=document.createElement("b"); b.textContent=k; c.appendChild(b);
    c.appendChild(document.createTextNode(" "+v)); ms.appendChild(c);
  });
}
function drawCursors(dpr){
  const v=view, out=[]; if(!v) return;
  const tAt=fx=>(v.ox+fx/v.zx)*(P-1)*v.dt, vAt=fy=>(v.zy-(fy*v.h/v.zv+v.oy*v.h))/v.ppv;
  const at=fx=>v.seg[Math.max(0,Math.min(P-1,Math.round((v.ox+fx/v.zx)*(P-1))))];
  cx.lineWidth=dpr; cx.font=(11*dpr)+"px Consolas,monospace";
  if(cur.t){cx.strokeStyle=cx.fillStyle="#f472b6";
    const t1=tAt(cur.t1),t2=tAt(cur.t2);
    [["1",cur.t1],["2",cur.t2]].forEach(([n,fx])=>{const x=fx*v.w;cx.beginPath();cx.moveTo(x,0);cx.lineTo(x,v.h);cx.stroke();cx.fillText("t"+n+" "+ft(tAt(fx)),x+3*dpr,v.h-6*dpr);});
    out.push(`t1 ${ft(t1)}  t2 ${ft(t2)}  Δt ${ft(t2-t1)}  V(t1) ${fu(pv(at(cur.t1)))}  V(t2) ${fu(pv(at(cur.t2)))}`);}
  if(cur.v){cx.strokeStyle=cx.fillStyle="#a78bfa";
    [["1",cur.v1],["2",cur.v2]].forEach(([n,fy])=>{const y=fy*v.h;cx.beginPath();cx.moveTo(0,y);cx.lineTo(v.w,y);cx.stroke();cx.fillText("V"+n+" "+fu(pv(vAt(fy))),v.w/2,y-4*dpr);});
    const a=vAt(cur.v1),b=vAt(cur.v2); out.push(`V1 ${fu(pv(a))}  V2 ${fu(pv(b))}  ΔV ${fu(pdiff(a-b))}`);}
  $("curs").textContent=out.join("     ");
}
function clampView(){zx=Math.max(1,Math.min(16,zx));zv=Math.max(1,Math.min(16,zv));ox=Math.max(0,Math.min(1-1/zx,ox));oy=Math.max(0,Math.min(1-1/zv,oy));}
function zoomAt(ax,k,fx,fy){if(ax=="t"){const nz=Math.max(1,Math.min(16,zx*k));ox+=fx*(1/zx-1/nz);zx=nz;}else{const nz=Math.max(1,Math.min(16,zv*k));oy+=fy*(1/zv-1/nz);zv=nz;}clampView();draw();}
function resetView(){zx=1;zv=1;ox=0;oy=0;draw();}
cv.addEventListener("pointerdown",e=>{
  const r=cv.getBoundingClientRect(), px=e.clientX-r.left, py=e.clientY-r.top, fx=px/r.width, fy=py/r.height, c=[];
  ptrs.set(e.pointerId,{x:px,y:py}); cv.setPointerCapture(e.pointerId);
  if(ptrs.size>=2){drag=null;pan=null;lastTap=null;return;}
  if(badge&&px>=badge.x&&px<=badge.x+badge.w&&py>=badge.y&&py<=badge.y+badge.h){lastTap=null;resetView();return;}
  const now=e.timeStamp||performance.now();
  if(e.isPrimary&&lastTap&&now-lastTap.t<300&&Math.hypot(px-lastTap.x,py-lastTap.y)<30){lastTap=null;drag=null;pan=null;resetView();return;}
  if(e.isPrimary)lastTap={t:now,x:px,y:py};
  if(cur.t) c.push(["t1",Math.abs(fx-cur.t1)],["t2",Math.abs(fx-cur.t2)]);
  if(cur.v) c.push(["v1",Math.abs(fy-cur.v1)],["v2",Math.abs(fy-cur.v2)]);
  c.sort((a,b)=>a[1]-b[1]);
  if(c.length&&c[0][1]<.04)drag=c[0][0];
  else if(zx>1||zv>1)pan={id:e.pointerId,x:px,y:py};
});
cv.addEventListener("pointermove",e=>{
  const p=ptrs.get(e.pointerId); if(!p)return;
  const r=cv.getBoundingClientRect(), nx=e.clientX-r.left, ny=e.clientY-r.top;
  if(ptrs.size>=2){
    const ids=[...ptrs.keys()], a=ptrs.get(ids[0]), b=ptrs.get(ids[1]), ax=a.x, ay=a.y, bx0=b.x, by0=b.y;
    p.x=nx; p.y=ny;
    const sx0=Math.abs(bx0-ax), sy0=Math.abs(by0-ay), mx=(a.x+b.x)/2, my=(a.y+b.y)/2, sx=Math.abs(b.x-a.x), sy=Math.abs(b.y-a.y);
    if(sx>=sy){if(sx0>0)zoomAt("t",sx/sx0,mx/r.width,my/r.height);}else{if(sy0>0)zoomAt("v",sy/sy0,mx/r.width,my/r.height);}
    return;
  }
  if(drag){cur[drag]=Math.max(0,Math.min(1,drag[0]=="t"?nx/r.width:ny/r.height));p.x=nx;p.y=ny;draw();return;}
  if(pan&&pan.id==e.pointerId){ox-=(nx-pan.x)/(r.width*zx);oy-=(ny-pan.y)/(r.height*zv);pan.x=nx;pan.y=ny;clampView();draw();}
  p.x=nx; p.y=ny;
});
function endPointer(e){ptrs.delete(e.pointerId);if(pan&&pan.id==e.pointerId)pan=null;if(!ptrs.size)drag=null;}
cv.addEventListener("pointerup",endPointer);
cv.addEventListener("pointercancel",endPointer);
cv.addEventListener("wheel",e=>{const r=cv.getBoundingClientRect(),fx=(e.clientX-r.left)/r.width,fy=(e.clientY-r.top)/r.height;const d=e.deltaY||e.deltaX;if(!d)return;e.preventDefault();zoomAt(e.shiftKey?"v":"t",d<0?1.25:1/1.25,fx,fy);},{passive:false});

function onFrame(buf){
  const nf=decode(buf);
  if(!nf){$("set").textContent="scope not connected (check SWD wires / scope power)";return;}
  frames++;
  if(rec.on){               // record each new capture once (the trace only changes on a capture)
    const key=nf.px.join(",");
    if(key!=rec.last){rec.last=key; if(rec.frames.length<MAX_REC) rec.frames.push({t:performance.now()-rec.t0,buf}); else toggleRec(); updRec();}
  }
  f=nf; draw();
}
function connect(){
  ws=new WebSocket(`ws://${location.hostname}:81/`); ws.binaryType="arraybuffer";
  ws.onopen=()=>{$("stat").textContent="connected"; ws.send("fps "+$("fps").value);};
  ws.onclose=()=>{$("stat").textContent="reconnecting..."; setTimeout(connect,1500);};
  ws.onmessage=e=>onFrame(e.data);
}
setInterval(()=>{const n=performance.now(),fps=Math.round(frames*1000/(n-tf));frames=0;tf=n;
  if(ws&&ws.readyState==1)$("stat").textContent=fps+" fps";},1000);

// Recording / playback
function updRec(){
  const n=rec.frames.length, dur=n?rec.frames[n-1].t/1000:0;
  $("recinfo").textContent=`${n} captures, ${dur.toFixed(1)} s`+(rec.on?" ●":"");
  ["play","scrub","rcsv","rclr"].forEach(id=>$(id).disabled=!n||rec.on); $("scrub").max=Math.max(0,n-1);
}
function toggleRec(){rec.on=!rec.on; if(rec.on){rec.frames=[];rec.last="";rec.t0=performance.now();showLive();}
  $("rec").textContent=rec.on?"■ Stop":"● Record"; $("rec").classList.toggle("on",rec.on); updRec();}
function showFrame(i){rec.idx=i;$("scrub").value=i;held=decode(rec.frames[i].buf);$("live").disabled=false;draw();}
function stopPlay(){clearInterval(rec.timer);rec.timer=null;$("play").innerHTML="&#9654;";}
function showLive(){rec.idx=-1;stopPlay();$("live").disabled=true;held=null;frozen=false;$("pause").classList.remove("on");}
$("rec").onclick=toggleRec;
$("scrub").oninput=()=>{stopPlay();showFrame(+$("scrub").value);};
$("play").onclick=()=>{ if(rec.timer){stopPlay();return;}
  let i=rec.idx<0||rec.idx>=rec.frames.length-1?0:rec.idx; showFrame(i); $("play").innerHTML="&#10074;&#10074;";
  const step=Math.max(30,rec.frames[rec.frames.length-1].t/Math.max(1,rec.frames.length-1));
  rec.timer=setInterval(()=>{if(++i>=rec.frames.length){stopPlay();return;} showFrame(i);},step);};
$("live").onclick=()=>{showLive();draw();};
$("rclr").onclick=()=>{showLive();rec.frames=[];updRec();};
function download(name,blob){const a=document.createElement("a");a.href=URL.createObjectURL(blob);a.download=name;a.click();}
$("rcsv").onclick=()=>{
  const rows=["capture,time_s,vdiv,tdiv,point_dt,unit"+Array.from({length:P},(_,i)=>",v"+i).join("")+"\n"];
  rec.frames.forEach((r,n)=>{const d=decode(r.buf); if(d) rows.push(`${n},${(r.t/1000).toFixed(3)},${VOLTS[d.vi]},${TIMES[d.ti]},${d.dt},${selProbe.unit},`+Array.from(d.volts,x=>pv(x).toFixed(4)).join(",")+"\n");});
  download("dso152_recording.csv",new Blob(rows,{type:"text/csv"}));};

// Controls
document.querySelectorAll("[data-adj]").forEach(b=>b.onclick=()=>ws.send("adj "+b.dataset.adj));
$("mode").onclick=()=>ws.send("mode");
$("run").onclick=()=>ws.send("run");
$("more").onclick=()=>{$("tools").classList.toggle("open");$("more").classList.toggle("on");setTimeout(draw,0);};
$("toolsclose").onclick=()=>$("more").click();
function phoneLayout(){return matchMedia("(max-width:600px), (orientation:landscape) and (max-height:500px)").matches;}
document.addEventListener("pointerdown",e=>{
  const t=$("tools"); if(!phoneLayout()||!t.classList.contains("open"))return;
  if(t.contains(e.target)||$("more").contains(e.target))return;
  $("more").click();
},true);
$("about").onclick=()=>{ if(!$("qr").src)$("qr").src="/qr.png"; $("aboutbox").classList.toggle("open");$("about").classList.toggle("on");};
$("aboutclose").onclick=()=>{$("aboutbox").classList.remove("open");$("about").classList.remove("on");};
$("setb").onclick=()=>{ $("setbox").classList.toggle("open");$("setb").classList.toggle("on");
  if($("setbox").classList.contains("open")){loadSettings();loadWifi();startStatus();}else stopStatus();};
$("setclose").onclick=()=>{$("setbox").classList.remove("open");$("setb").classList.remove("on");stopStatus();};
$("aboutbox").onclick=e=>{if(e.target.id=="aboutbox")$("aboutclose").onclick();};
$("setbox").onclick=e=>{if(e.target.id=="setbox")$("setclose").onclick();};
function openProbeEditor(){const p=selProbe;$("p_name").value=p.name;$("p_unit").value=p.unit;$("p_mv").value=p.mv;$("p_z").value=p.z;$("p_inv").checked=p.inv;$("probedel").disabled=!p.custom;$("probebox").classList.add("open");$("probeedit").classList.add("on");}
function closeProbeEditor(){$("probebox").classList.remove("open");$("probeedit").classList.remove("on");}
$("probeedit").onclick=openProbeEditor;
$("probeclose").onclick=closeProbeEditor;
$("probebox").onclick=e=>{if(e.target.id=="probebox")closeProbeEditor();};
$("probe").onchange=()=>{selProbe=probes.find(p=>p.name==$("probe").value)||probes[0];saveProbeStore();draw();};
$("probeform").onsubmit=e=>{e.preventDefault();const name=$("p_name").value.trim()||"Custom";const np={name,unit:$("p_unit").value.trim()||"V",mv:Math.abs(parseFloat($("p_mv").value))||1000,z:parseFloat($("p_z").value)||0,inv:$("p_inv").checked,custom:true};const i=customProbes.findIndex(p=>p.name==name);if(i>=0)customProbes[i]=np;else customProbes.push(np);rebuildProbes();selProbe=probes.find(p=>p.name==name)||probes[0];saveProbeStore();fillProbeSelect();draw();closeProbeEditor();};
$("probedel").onclick=()=>{if(!selProbe.custom)return;const n=selProbe.name;customProbes=customProbes.filter(p=>p.name!=n);rebuildProbes();selProbe=probes.find(p=>p.name==n)||probes[0];saveProbeStore();fillProbeSelect();draw();closeProbeEditor();};
document.addEventListener("keydown",e=>{if(e.key=="Escape"){$("aboutclose").onclick();$("setclose").onclick();$("probeclose").onclick();}});
$("bmc").href=BMC_URL;
$("zu").onclick=()=>{zeroDiv=Math.min(VD,zeroDiv+1);draw();};
$("zd").onclick=()=>{zeroDiv=Math.max(0,zeroDiv-1);draw();};
$("ct").onclick=()=>{cur.t=!cur.t;$("ct").classList.toggle("on",cur.t);if(!cur.t&&!cur.v)$("curs").textContent="";draw();};
$("cv").onclick=()=>{cur.v=!cur.v;$("cv").classList.toggle("on",cur.v);if(!cur.t&&!cur.v)$("curs").textContent="";draw();};
$("pause").onclick=()=>{ if(rec.idx>=0) return; frozen=!frozen; held=frozen?f:null; $("pause").classList.toggle("on",frozen); draw();};
$("fps").onchange=()=>ws.send("fps "+$("fps").value);
addEventListener("resize",draw);
$("png").onclick=()=>{
  const dpr=devicePixelRatio||1, pad=40*dpr, out=document.createElement("canvas");
  out.width=cv.width; out.height=cv.height+pad*2; const o=out.getContext("2d");
  o.fillStyle="#12151c";o.fillRect(0,0,out.width,out.height);o.drawImage(cv,0,pad);
  o.font=(13*dpr)+"px Consolas,monospace";
  o.fillStyle="#facc15";o.fillText($("set").textContent,8*dpr,26*dpr);
  o.fillStyle="#38bdf8";o.fillText($("meas").textContent,8*dpr,cv.height+pad+22*dpr);
  o.fillStyle="#f0abfc";o.fillText($("curs").textContent,8*dpr,cv.height+pad+38*dpr);
  out.toBlob(b=>download("dso152.png",b));};
$("csv").onclick=()=>{const t=held||f; if(!t)return;
  let s=`# ${selProbe.name=="Voltage"?VOLTS[t.vi]+"/div":fu(pdiv(t.vdiv))+"/div"}, ${TIMES[t.ti]}/div\npoint,time_s,unit,volts\n`;
  for(let i=0;i<P;i++) s+=`${i},${(i*t.dt).toPrecision(6)},${selProbe.unit},${pv(t.volts[i]).toFixed(4)}\n`;
  download("dso152.csv",new Blob([s],{type:"text/csv"}));};
// Settings popup
let statusTimer=null;
function setSel(id,v){const e=$(id);e.value=String(v);if(!e.value)e.selectedIndex=0;}
async function loadSettings(){
  try{
    const r=await fetch("/settings"); if(!r.ok)throw new Error("HTTP "+r.status);
    const s=await r.json();
    $("s_log_on").checked=!!s.log_on; setSel("s_log_int",s.log_int);
    $("s_ftp_host").value=s.ftp_host||""; $("s_ftp_port").value=s.ftp_port||21;
    $("s_ftp_user").value=s.ftp_user||"";
    $("s_ftp_pass").value=""; $("s_ftp_pass").placeholder=s.ftp_pass_set?"unchanged":"";
    $("s_ftp_dir").value=s.ftp_dir||"/"; setSel("s_ftp_every",s.ftp_every);
    $("s_mqtt_on").checked=!!s.mqtt_on;
    $("s_mqtt_host").value=s.mqtt_host||""; $("s_mqtt_port").value=s.mqtt_port||1883;
    $("s_mqtt_user").value=s.mqtt_user||"";
    $("s_mqtt_pass").value=""; $("s_mqtt_pass").placeholder=s.mqtt_pass_set?"unchanged":"";
    $("s_mqtt_topic").value=s.mqtt_topic||"dso152"; setSel("s_mqtt_int",s.mqtt_int);
  }catch(e){$("setstatus").textContent="error: "+e.message;}
}
async function loadWifi(){
  try{
    const r=await fetch("/wifi"); if(!r.ok)throw new Error("HTTP "+r.status);
    const w=await r.json(), ap=w.mode=="ap";
    $("wifinow").textContent=ap?`Hotspot ${w.ssid||"DSO152"}, ${w.ip||"192.168.4.1"} (no known network found)`:`On ${w.ssid}, ${w.ip}, ${w.rssi} dBm`;
    $("w_connect").hidden=!ap;
    const list=$("wifilist"); list.textContent="";
    (w.saved||[]).forEach(ssid=>{
      const row=document.createElement("div");
      row.style.cssText="display:flex;align-items:center;justify-content:space-between;gap:6px";
      const name=document.createElement("span"); name.textContent=ssid;
      const del=document.createElement("button"); del.type="button"; del.className="g"; del.textContent="x";
      del.style.cssText="padding:1px 8px";
      del.onclick=async()=>{
        if(!confirm(`Remove ${ssid}?`))return;
        try{
          const b=new URLSearchParams(); b.set("ssid",ssid);
          const rr=await fetch("/wifi/del",{method:"POST",body:b});
          const t=(await rr.text()).trim();
          if(rr.ok&&t=="ok")loadWifi(); else $("setstatus").textContent="error: "+t;
        }catch(e){$("setstatus").textContent="error: "+e.message;}
      };
      row.appendChild(name); row.appendChild(del); list.appendChild(row);
    });
  }catch(e){$("setstatus").textContent="error: "+e.message;}
}
function wifiOther(){
  const other=!$("w_ssid").value;
  $("w_other").hidden=!other;
  if(!other)$("w_other").value="";
}
async function scanWifi(){
  $("setstatus").textContent="scanning...";
  for(let i=0;i<15;i++){
    try{
      const r=await fetch("/wifi/scan"); if(!r.ok)throw new Error("HTTP "+r.status);
      const s=await r.json();
      if(s.busy){await new Promise(done=>setTimeout(done,700));continue;}
      $("w_ssid").innerHTML="";
      (s.nets||[]).forEach(n=>{
        const o=document.createElement("option");
        o.value=n.ssid; o.textContent=`${n.ssid} (${n.rssi} dBm)${n.open?" open":""}`;
        $("w_ssid").appendChild(o);
      });
      const o=document.createElement("option"); o.value=""; o.textContent="Other..."; $("w_ssid").appendChild(o);
      $("w_ssid").selectedIndex=0; wifiOther();
      $("setstatus").textContent="";
      return;
    }catch(e){$("setstatus").textContent="error: "+e.message;return;}
  }
  $("setstatus").textContent="scan timed out";
}
async function addWifi(){
  const b=new URLSearchParams();
  b.set("ssid",$("w_ssid").value||$("w_other").value);
  b.set("pass",$("w_pass").value);
  try{
    const r=await fetch("/wifi/add",{method:"POST",body:b});
    const t=(await r.text()).trim();
    if(r.ok&&t=="ok"){$("setstatus").textContent="network saved";$("w_pass").value="";loadWifi();}
    else $("setstatus").textContent="error: "+t;
  }catch(e){$("setstatus").textContent="error: "+e.message;}
}
async function connectWifi(){
  try{
    const r=await fetch("/wifi/connect",{method:"POST"});
    const t=(await r.text()).trim();
    if(r.ok&&t=="trying")$("setstatus").textContent="trying saved networks, the hotspot will drop if one works. Reconnect your phone to that network and open http://dso152.local/";
    else $("setstatus").textContent="error: "+t;
  }catch(e){$("setstatus").textContent="error: "+e.message;}
}
async function saveSettings(){
  const b=new URLSearchParams();
  if($("s_log_on").checked)b.set("log_on","1");
  b.set("log_int",$("s_log_int").value);
  b.set("ftp_host",$("s_ftp_host").value.trim());
  b.set("ftp_port",$("s_ftp_port").value);
  b.set("ftp_user",$("s_ftp_user").value);
  b.set("ftp_pass",$("s_ftp_pass").value);
  b.set("ftp_dir",$("s_ftp_dir").value);
  b.set("ftp_every",$("s_ftp_every").value);
  if($("s_mqtt_on").checked)b.set("mqtt_on","1");
  b.set("mqtt_host",$("s_mqtt_host").value.trim());
  b.set("mqtt_port",$("s_mqtt_port").value);
  b.set("mqtt_user",$("s_mqtt_user").value);
  b.set("mqtt_pass",$("s_mqtt_pass").value);
  b.set("mqtt_topic",$("s_mqtt_topic").value.trim());
  b.set("mqtt_int",$("s_mqtt_int").value);
  b.set("tz_min",String(-new Date().getTimezoneOffset()));
  try{
    const r=await fetch("/settings",{method:"POST",body:b});
    const t=(await r.text()).trim();
    if(r.ok&&t=="ok"){$("setstatus").textContent="saved";$("s_ftp_pass").value="";$("s_mqtt_pass").value="";loadSettings();}
    else $("setstatus").textContent="error: "+t;
  }catch(e){$("setstatus").textContent="error: "+e.message;}
}
async function pollStatus(){
  try{
    const r=await fetch("/status"); if(!r.ok)throw new Error("HTTP "+r.status);
    const s=await r.json();
    $("setstatus").textContent=`Time ${s.time||"--"}, ${s.pending_bytes} bytes waiting, FTP: ${s.ftp_status}, MQTT: ${s.mqtt}`;
  }catch(e){$("setstatus").textContent="status error: "+e.message;}
}
function startStatus(){pollStatus();stopStatus();statusTimer=setInterval(pollStatus,5000);}
function stopStatus(){if(statusTimer){clearInterval(statusTimer);statusTimer=null;}}
$("setform").onsubmit=e=>{e.preventDefault();saveSettings();};
$("wifiform").onsubmit=e=>{e.preventDefault();addWifi();};
$("w_scan").onclick=scanWifi;
$("w_ssid").onchange=wifiOther;
$("w_connect").onclick=connectWifi;
$("setupload").onclick=async()=>{try{const r=await fetch("/ftp/upload",{method:"POST"});$("setstatus").textContent=(await r.text())||"upload done";}catch(e){$("setstatus").textContent="error: "+e.message;}};
$("setdownload").onclick=async()=>{try{const r=await fetch("/log.csv");if(!r.ok)throw new Error("HTTP "+r.status);download("dso152_log.csv",await r.blob());}catch(e){$("setstatus").textContent="error: "+e.message;}};
$("setclear").onclick=async()=>{try{await fetch("/log/clear",{method:"POST"});pollStatus();}catch(e){$("setstatus").textContent="error: "+e.message;}};
loadProbes();
connect();
</script></body></html>
)HTML";
