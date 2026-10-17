"""Create an offline developer viewer from measured C++ transfer curves (stdlib only)."""
import csv
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
curves = {}
with (root / 'Docs/Phase2/transfer-curves.csv').open(newline='') as source:
    for row in csv.DictReader(source):
        key = row['mode'] + ':' + row['damage']
        points = curves.setdefault(key, [])
        points.append([float(row['input']), float(row['output'])])
# Keep full-resolution CSV intact; plotting downsamples the measured data by eight.
curves = {key: values[::8] for key, values in curves.items()}
page = r'''<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>DISRUPTION Phase 2 — measured transfer curves</title>
<style>
body{font:15px system-ui;background:#16191d;color:#d6d9df;margin:32px auto;max-width:1250px;padding:0 20px}h1{font-size:26px}p{color:#aab1bd;line-height:1.6}.controls{display:flex;gap:30px;flex-wrap:wrap;margin:24px 0}label{display:flex;gap:12px;align-items:center}select,input{accent-color:#ff6029}select{background:#30353c;color:white;padding:8px}.charts{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:20px}.card{background:#20242a;border:1px solid #39404a;border-radius:6px;padding:12px}.card h2{margin:8px;font-size:20px}svg{width:100%;height:auto}svg text{fill:#abb3c0;font-size:12px}.legend{display:flex;gap:15px;flex-wrap:wrap;margin:20px 0}a{color:#ff9a78}.value{min-height:24px;font-family:monospace}
</style>
<h1>DISRUPTION / Phase 2 transfer curves</h1>
<p>Developer inspection utility. These are measurements from the actual C++ engine transfer functions, before DC filtering, mixing and output gain. RIP's audible steady-state waveform loses its mean through its DC blocker. Input is the amplitude <em>after DRIVE</em>.</p>
<div class="controls"><label>DAMAGE <select id="damage"><option value="all">Overlay all</option><option>0</option><option>25</option><option>50</option><option>75</option><option>100</option></select></label><label>Input view ±<input id="range" type="range" min="0.5" max="4" step="0.5" value="2"><span id="rangeLabel">2</span></label></div>
<div class="legend" id="legend"></div><div class="charts" id="charts"></div>
<p>Hover inside a chart to inspect the nearest plotted input/output sample. Full-resolution data: <a href="transfer-curves.csv">transfer-curves.csv</a>. Aliasing is unresolved: <a href="aliasing-evidence.csv">measured fold-back evidence</a>. Listening renders are generated separately and do not alter plugin gain behavior.</p>
<script>
const curves=__DATA__;
const modes=['BURN','BREAK','RIP'], damageValues=[0,25,50,75,100], colors=['#a4b4c9','#77bbb1','#f0cf76','#ff9b58','#ff5038'];
const ns='http://www.w3.org/2000/svg'; const el=(tag,attrs={})=>{const node=document.createElementNS(ns,tag);for(const [k,v] of Object.entries(attrs))node.setAttribute(k,v);return node;};
document.querySelector('#legend').innerHTML=damageValues.map((d,i)=>`<span style="color:${colors[i]}">DAMAGE ${d}%</span>`).join('');
for(const mode of modes){const card=document.createElement('div');card.className='card';card.innerHTML=`<h2>${mode}</h2><svg id="${mode}" viewBox="0 0 400 320" role="img" aria-label="${mode} transfer curves"></svg><div class="value" id="${mode}-value">Hover to inspect</div>`;document.querySelector('#charts').append(card);}
function draw(){const range=Number(document.querySelector('#range').value), selection=document.querySelector('#damage').value;document.querySelector('#rangeLabel').textContent=range;
for(const mode of modes){const svg=document.getElementById(mode);svg.replaceChildren();const sx=x=>48+(x+range)/(2*range)*330,sy=y=>272-(y+1.8)/3.6*250;
for(const x of [-range,0,range]){svg.append(el('line',{x1:sx(x),x2:sx(x),y1:22,y2:272,stroke:'#434b57'}));const label=el('text',{x:sx(x),y:292,'text-anchor':'middle'});label.textContent=x;svg.append(label);}
for(const y of [-1.5,0,1.5]){svg.append(el('line',{x1:48,x2:378,y1:sy(y),y2:sy(y),stroke:'#434b57'}));const label=el('text',{x:40,y:sy(y)+4,'text-anchor':'end'});label.textContent=y;svg.append(label);}
const linear=Math.min(1.8,range);svg.append(el('line',{x1:sx(-linear),x2:sx(linear),y1:sy(-linear),y2:sy(linear),stroke:'#77808c','stroke-dasharray':'4 5'}));
for(let i=0;i<damageValues.length;i++){const damage=damageValues[i];if(selection!=='all'&&Number(selection)!==damage)continue;const pts=curves[`${mode}:${damage}`].filter(p=>Math.abs(p[0])<=range);svg.append(el('path',{d:pts.map((p,j)=>`${j?'L':'M'}${sx(p[0]).toFixed(2)},${sy(p[1]).toFixed(2)}`).join(' '),stroke:colors[i],'stroke-width':2,fill:'none'}));}
const label=el('text',{x:210,y:314,'text-anchor':'middle'});label.textContent='Driven input amplitude';svg.append(label);
svg.onpointermove=e=>{const rect=svg.getBoundingClientRect(),coordinate=(e.clientX-rect.left)*400/rect.width,x=(coordinate-48)/330*2*range-range;const selected=selection==='all'?100:Number(selection);const pts=curves[`${mode}:${selected}`];const nearest=pts.reduce((a,b)=>Math.abs(a[0]-x)<Math.abs(b[0]-x)?a:b);document.getElementById(mode+'-value').textContent=`D=${selected}%  x=${nearest[0].toFixed(3)} -> y=${nearest[1].toFixed(3)}`;};
}}
document.querySelector('#damage').onchange=draw;document.querySelector('#range').oninput=draw;draw();
</script></html>'''
(root / 'Docs/Phase2/Transfer-Curves.html').write_text(page.replace('__DATA__', json.dumps(curves, separators=(',', ':'))), encoding='utf-8')
print('Created Docs/Phase2/Transfer-Curves.html')


