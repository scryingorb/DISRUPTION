"""Generate the local Phase 7 screenshot contact sheet (Python standard library)."""
from pathlib import Path
from html import escape
import struct
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Docs/Phase7'

def main():
    shots = sorted((OUT / 'Screenshots').glob('*.png'))
    assert len(shots) >= 15, 'Run the Phase 7 tests first.'
    html = ['''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>DISRUPTION — Final Interface Previews</title><style>body{background:#101215;color:#d5d7d8;font:16px system-ui;margin:24px}h1,a{color:#ff651f}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(420px,1fr));gap:20px}figure{margin:0;border:1px solid #454c51;padding:12px;background:#171b1e}img{width:100%;height:auto;display:block}figcaption{padding:10px 0;font-size:14px}p{max-width:1000px;line-height:1.6}@media(max-width:500px){main{display:block}figure{margin-bottom:18px}}</style><h1>DISRUPTION / Phase 7</h1><p>Actual JUCE editor captures. Fixed 900 × 600 logical layout. DPI captures render the same component tree at 100%, 125%, 150% and 200%; they do not substitute for testing OS monitor changes in a DAW. Click an image to inspect its original pixels.</p><p><a href="../Phase7-Report.md">Completion report</a> · <a href="Screenshots/DAMAGE-100-2x.png">Maximum DAMAGE / 2×</a></p><main>''']
    for shot in shots:
        width, height = struct.unpack('>II', shot.read_bytes()[16:24])
        caption = escape(shot.stem.replace('-', ' ') + f' / {width} × {height} pixels')
        html.append(f'<figure><a href="Screenshots/{shot.name}"><img loading="lazy" src="Screenshots/{shot.name}" alt="{caption}"></a><figcaption>{caption}</figcaption></figure>')
    html.append('</main></html>')
    (OUT / 'Editor-Previews.html').write_text('\n'.join(html), encoding='utf-8')
    print(f'Wrote screenshot gallery with {len(shots)} real editor captures.')

if __name__ == '__main__':
    main()
