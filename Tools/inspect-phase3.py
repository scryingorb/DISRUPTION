"""Render an offline Phase 3 DSP inspector from measured CSVs, stdlib only."""
from pathlib import Path
import csv
import json

root = Path(__file__).resolve().parent.parent
folder = root / 'Docs/Phase3'
curves = {}
with (folder / 'chaos-transfer-curves.csv').open(newline='') as stream:
    for row in csv.DictReader(stream):
        curves.setdefault(row['damage'], []).append([float(row['input']), float(row['output'])])
curves = {key: points[::8] for key, points in curves.items()}
holds = {}
with (folder / 'crush-sample-hold.csv').open(newline='') as stream:
    for row in csv.DictReader(stream):
        if int(row['frame']) < 512:
            holds.setdefault(row['damage'], []).append([int(row['frame']), float(row['input']), float(row['output']), int(row['captured'])])
page = r'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>DISRUPTION / Phase 3 DSP inspection</title>
<style>body{margin:30px auto;padding:0 20px;max-width:1200px;background:#171a1e;color:#d8dbe1;font:15px system-ui}h1{font-size:26px}p{color:#aeb6c1;line-height:1.6}section{padding:20px;margin:24px 0;background:#23272d;border:1px solid #414854;border-radius:6px}svg{width:100%;height:auto}svg text{fill:#b8bfca;font-size:13px}select,input{accent-color:#ff662d}select{background:#343c46;color:white;padding:6px}label{margin-right:25px;display:inline-flex;gap:10px;align-items:center}a{color:#ff9d79}.key{display:flex;gap:20px;flex-wrap:wrap;margin:12px 0}.small{font:13px monospace}</style>
<h1>DISRUPTION / Phase 3 measured DSP</h1>
<p>Developer-only inspection. CHAOS is plotted before its DC blocker. CRUSH shows the shared capture clock, held values and quantization for a 437 Hz input at 48 kHz. No graph code is part of the plugin.</p>
<section><h2>CHAOS wavefolder and asymmetric saturation</h2><label>DAMAGE <select id="damage"><option value="all">All curves</option><option>0</option><option>25</option><option>50</option><option>75</option><option>100</option></select></label><label>Input view +/-<input id="range" type="range" min="0.5" max="4" step="0.5" value="2"><span id="rangeValue">2</span></label><div class="key" id="legend"></div><svg id="chaos" viewBox="0 0 1000 360" role="img" aria-label="Measured CHAOS transfer curves"></svg><p class="small">Input is amplitude after DRIVE. The dashed line is unity transfer. Folding reverses slope; it is not threshold clipping.</p></section>
<section><h2>CRUSH sample-and-hold / quantization</h2><label>DAMAGE <select id="crushDamage"><option>0</option><option>50</option><option selected>100</option></select></label><div class="key"><span style="color:#70b9d3">Input</span><span style="color:#ff662d">Processed (including hold and quantization)</span><span style="color:#eecb75">Capture instant</span></div><svg id="crush" viewBox="0 0 1000 360" role="img" aria-label="CRUSH sample-and-hold waveform"></svg><p id="clock"></p></section>
<p>Full-resolution data: <a href="chaos-transfer-curves.csv">CHAOS curves</a>, <a href="crush-sample-hold.csv">CRUSH waveform</a>, <a href="crush-clock-mapping.csv">physical capture-rate mapping</a>. <a href="aliasing-evidence.csv">Measured aliasing</a> is intentional for CRUSH and unresolved in CHAOS; HQ remains inactive.</p>
<script>
const curves=__CURVES__, holds=__HOLDS__;
const ns='http://www.w3.org/2000/svg',colors=['#a5b7cd','#79bfb2','#f0cf75','#ff9c58','#ff513b'], values=[0,25,50,75,100];
function element(name,attrs={}){const e=document.createElementNS(ns,name);for(const [key,value] of Object.entries(attrs))e.setAttribute(key,value);return e;}
function text(svg,x,y,value,anchor='middle'){const e=element('text',{x,y,'text-anchor':anchor});e.textContent=value;svg.append(e);}
function axes(svg,min,max,limit,label){svg.replaceChildren();const x=v=>60+(v-min)/(max-min)*900,y=v=>310-(v+limit)/(2*limit)*280;
for(const v of [min,(min+max)/2,max]){svg.append(element('line',{x1:x(v),x2:x(v),y1:30,y2:310,stroke:'#414b59'}));text(svg,x(v),330,v.toFixed(1));}
for(const v of [-limit,0,limit]){svg.append(element('line',{x1:60,x2:960,y1:y(v),y2:y(v),stroke:'#414b59'}));text(svg,50,y(v)+4,v.toFixed(1),'end');}text(svg,510,353,label);return {x,y};}
function path(svg,points,x,y,color){svg.append(element('path',{d:points.map((p,i)=>`${i?'L':'M'}${x(p[0]).toFixed(2)},${y(p[1]).toFixed(2)}`).join(' '),fill:'none',stroke:color,'stroke-width':2}));}
function drawChaos(){const range=Number(document.querySelector('#range').value),selection=document.querySelector('#damage').value,svg=document.querySelector('#chaos'),{x,y}=axes(svg,-range,range,1.5,'Driven input amplitude');document.querySelector('#rangeValue').textContent=range;const linear=Math.min(range,1.5);svg.append(element('line',{x1:x(-linear),x2:x(linear),y1:y(-linear),y2:y(linear),stroke:'#687687','stroke-dasharray':'5 6'}));
for(let i=0;i<values.length;i++)if(selection==='all'||Number(selection)===values[i])path(svg,curves[values[i]].filter(p=>Math.abs(p[0])<=range),x,y,colors[i]);}
function drawCrush(){const d=document.querySelector('#crushDamage').value,data=holds[d],svg=document.querySelector('#crush'),{x,y}=axes(svg,0,511,1,'Host sample frame (48 kHz)');path(svg,data.map(p=>[p[0],p[1]]),x,y,'#70b9d3');
// Use step drawing rather than connecting digital holds with sloping lines.
const steps=[];for(let i=0;i<data.length;i++){if(i)steps.push([data[i][0],data[i-1][2]]);steps.push([data[i][0],data[i][2]]);}path(svg,steps,x,y,'#ff662d');for(const p of data)if(p[3])svg.append(element('circle',{cx:x(p[0]),cy:y(p[2]),r:2,fill:'#eecb75'}));
document.querySelector('#clock').textContent=d==='100'?'DAMAGE 100: nominal 3-bit grid, 600 Hz effective capture rate, 80 host frames per hold.':d==='50'?'DAMAGE 50: approximately 8.79 effective bits, 14.03 kHz capture rate, alternating 3/4-frame holds.':'DAMAGE 0: nominal 16-bit grid, native 48 kHz capture rate.';}
document.querySelector('#legend').innerHTML=values.map((v,i)=>`<span style="color:${colors[i]}">DAMAGE ${v}%</span>`).join('');document.querySelector('#damage').onchange=drawChaos;document.querySelector('#range').oninput=drawChaos;document.querySelector('#crushDamage').onchange=drawCrush;drawChaos();drawCrush();
</script></html>'''
page = page.replace('__CURVES__', json.dumps(curves, separators=(',', ':'))).replace('__HOLDS__', json.dumps(holds, separators=(',', ':')))
(folder / 'DSP-Inspector.html').write_text(page, encoding='utf-8')
print('Created Docs/Phase3/DSP-Inspector.html')
