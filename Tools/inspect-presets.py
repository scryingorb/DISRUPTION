"""Build a local factory-bank listening index and parameter inventory (stdlib only)."""
from pathlib import Path
import csv
import html
import xml.etree.ElementTree as ET
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Docs/Phase6'
PARAMS = ['mode', 'drive', 'damage', 'tone', 'body', 'crush', 'mix', 'output', 'hq', 'bypass']

def main():
    bank = ET.parse(ROOT / 'Resources/FactoryPresets.xml').getroot()
    levels = {row['name']: row for row in csv.DictReader((OUT / 'preset-levels.csv').open(encoding='utf-8'))}
    with (OUT / 'Factory-Parameters.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.writer(stream)
        writer.writerow(['id', 'category', 'name', 'source', *PARAMS])
        for preset in bank:
            values = {p.get('id'): p.get('value') for p in preset}
            writer.writerow([preset.get(k) for k in ['id', 'category', 'name', 'source']] + [values[k] for k in PARAMS])
    def audio(file):
        return '<audio controls preload="none" src="Listening/' + quote(file) + '"></audio>'
    parts = ['''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>DISRUPTION Factory Listening Bank</title>
<style>body{font:16px system-ui;background:#111316;color:#d6d8da;max-width:1150px;margin:auto;padding:24px}h1,h2{color:#ff541b}article{border:1px solid #383b40;border-radius:8px;padding:16px;margin:12px 0}table{width:100%;border-collapse:collapse}td,th{padding:8px;text-align:left;border-bottom:1px solid #383b40}audio{max-width:100%;width:300px}a{color:#ff925b}small{color:#aaa}summary{cursor:pointer}p{line-height:1.55}</style>
<h1>DISRUPTION — INIT + 30</h1><p>Four-second synthetic vocal-like, drum, bass, guitar-like, synth and effect sources. Raw renders preserve factory OUTPUT; matched renders are adjusted offline to 0.1 RMS for character comparison. This is measured validation and an audition aid; no human listening approval is claimed. Play dry then raw for level changes, or matched for texture. Avoid comparing raw and matched as if they shared gain.</p><p><a href="Factory-Parameters.csv">All parameters</a> · <a href="preset-levels.csv">Level measurements</a> · <a href="distinctness.csv">Common-reference differences</a></p><h2>INIT then all 30, matched</h2>''', audio('INIT-and-30-Matched-Sequence.wav')]
    current = None
    for preset in bank:
        category, name, source = [preset.get(k) for k in ['category', 'name', 'source']]
        if category != current:
            if current is not None:
                parts.append('</details>')
            parts.append('<details open><summary><h2 style="display:inline">' + html.escape(category) + '</h2></summary>')
            current = category
        values = {p.get('id'): p.get('value') for p in preset}
        row = levels[name]
        stem = category + '-' + name.replace(' ', '-')
        parts.append('<article><h3>' + html.escape(name) + '</h3><p>' + html.escape(' / '.join(k.upper() + ' ' + values[k] for k in PARAMS)) + '</p><small>' + html.escape(f"Source: {source}. RMS gain {float(row['gain_db']):+.2f} dB. Raw peak {float(row['peak']):.3f}. Low-band proxy {float(row['low_energy_fraction'])*100:.1f}%.") + '</small><table><tr><th>Dry source</th><th>Factory raw</th><th>Offline RMS matched</th></tr><tr><td>' + audio('Source-' + source + '.wav') + '</td><td>' + audio(stem + '-Raw.wav') + '</td><td>' + audio(stem + '-Matched.wav') + '</td></tr></table></article>')
    parts.append('</details><p>The sequence follows INIT, BURN, BREAK, RIP, CRUSH, CHAOS in inventory order; each segment lasts four seconds. File-edge fades only affect audition exports. No runtime normalization or limiter was added.</p></html>')
    (OUT / 'Factory-Listening.html').write_text('\n'.join(parts), encoding='utf-8')
    print('Wrote Docs/Phase6/Factory-Listening.html and Factory-Parameters.csv')

if __name__ == '__main__':
    main()
