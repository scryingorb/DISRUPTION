"""Prepare Phase 8 manual QA and raw audition artifacts; never tune the bank or touch dist."""
from pathlib import Path
import array
import csv
import hashlib
import html
import math
import shutil
import struct
import xml.etree.ElementTree as ET
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Docs/Phase8'
DRAFTS = ROOT / 'Docs/ReleaseDrafts'
PARAMS = ['mode', 'drive', 'damage', 'tone', 'body', 'crush', 'mix', 'output', 'hq', 'bypass']
JOBS = {
 'Vocal Heat':'Gentle vocal saturation', 'Warm Blood':'Warm harmonic weight',
 'Bass Furnace':'Bass saturation and density', 'Drum Ember':'Drum bus warmth',
 'Melted Synth':'Saturated synth texture', 'Full Burn':'Strong driven saturation',
 'Redline Vocal':'Clipped vocal urgency', 'Snare Breaker':'Snare attack distortion',
 'Flatline 808':'Clipped 808 weight', 'Concrete Drums':'Dense clipped drums',
 'Hardwired':'Hard-edged driven synth texture', 'Total Collapse':'Severe clipped destruction',
 'Torn Voice':'Asymmetric vocal distortion', 'Blown Cone':'Damaged speaker guitar texture',
 'Crooked Bass':'Asymmetric bass grit', 'Razor Lead':'Sharp distorted synth lead',
 'Factory Floor':'Industrial drum distortion', 'Shredded':'Extreme asymmetric destruction',
 'Digital Dust':'Light digital wear', 'Broken Codec':'Degraded vocal texture',
 'Bit Wound':'Digital synth damage', 'Dead Circuit':'Crushed drum texture',
 'Low Resolution':'Reduced-resolution synth texture', 'System Failure':'Extreme digital destruction',
 'Fracture':'Complex guitar distortion', 'Voice Damage':'Nonlinear vocal damage',
 'Folded Mass':'Folded bass density', 'Reactor':'Intense synth folding',
 'Signal Collapse':'Complex nonlinear collapse', 'MELTDOWN':'Maximum destruction character',
}

def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8')

def wav(path):
    """JUCE float32 RIFF WAVE, including extensible format; read without external packages."""
    raw = path.read_bytes()
    assert raw[:4] == b'RIFF' and raw[8:12] == b'WAVE'
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(raw):
        tag, count = struct.unpack_from('<4sI', raw, pos)
        chunk = raw[pos+8:pos+8+count]
        if tag == b'fmt ': fmt = chunk
        if tag == b'data': data = chunk
        pos += 8 + count + count % 2
    code, channels, rate = struct.unpack_from('<HHI', fmt)
    bits = struct.unpack_from('<H', fmt, 14)[0]
    if code == 65534: code = struct.unpack_from('<H', fmt, 24)[0]
    assert code == 3 and bits == 32 and channels == 2 and rate == 48000
    values = array.array('f'); values.frombytes(data)
    return values, channels, rate

def metrics(path):
    values, ch, rate = wav(path)
    peak, energy, low, bright, overs = 0., 0., 0., 0., 0
    lp_low, lp_high = [0.]*ch, [0.]*ch
    al, ah = math.exp(-2*math.pi*350/rate), math.exp(-2*math.pi*3000/rate)
    for n, v in enumerate(values):
        c=n%ch
        peak=max(peak,abs(v)); energy+=v*v; overs+=abs(v)>=1
        lp_low[c]=(1-al)*v+al*lp_low[c]; low+=lp_low[c]**2
        lp_high[c]=(1-ah)*v+ah*lp_high[c]; bright+=(v-lp_high[c])**2
    rms=math.sqrt(energy/len(values))
    return dict(peak=peak, rms=rms, crest_db=20*math.log10(peak/rms),
                overs_samples=overs, low_fraction=low/energy, bright_fraction=bright/energy)

def audition():
    bank = list(ET.parse(ROOT/'Resources/FactoryPresets.xml').getroot())[1:]
    baseline = {r['name']:r for r in csv.DictReader((ROOT/'Docs/Phase6/preset-levels.csv').open(encoding='utf-8'))}
    rows, checklist = [], ['# Factory audition — 1.0.0 RC1 manual gate', '',
      'Status: **NOT RUN — human evaluation required**. All 30 factory values are frozen. Character descriptions below are listening prompts inferred from the preset name, engine and source; confirm intended use during audition. These synthetic sources are aids, not a substitute for real music.', '',
      'Play dry then raw at a comfortable fixed monitoring level. Keep the same player/monitor gain between presets. Raw audio preserves factory OUTPUT; no loudness normalization or limiter was added. Existing 10 ms file-edge fades are retained. Also audition real vocals, drums, bass/808, synths and mixes in the DAW. Record changes as proposals; do not edit the bank during this phase.', '',
      'Evaluator: ____  Date: ____  Monitoring: ____  Build/hash: ____', '']
    page=['<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>DISRUPTION RC1 raw audition</title><style>body{font:16px system-ui;max-width:1100px;margin:auto;padding:24px;background:#151719;color:#eee}a,h1,h2{color:#ff793c}article{border:1px solid #555;padding:16px;margin:12px 0}audio{width:310px;max-width:100%}p{line-height:1.5}small{color:#ccc}</style>',
      '<h1>DISRUPTION 1.0.0 RC1 — human audition</h1><p>30 raw factory renders, grouped by engine. Factory OUTPUT is preserved; these are not loudness matched. Start at a comfortable monitoring level and keep player/monitor gain fixed. Sources are four-second synthetic role approximations, with retained 10 ms file-edge fades. Also test real music in FL Studio. No human approval has been recorded.</p><p><a href="Preset-Audition-Checklist.md">Blank scoring checklist</a> · <a href="preset-loudness.csv">Measured levels</a> · <a href="preset-similarity.csv">Common-source similarity</a></p>']
    category=None
    for preset in bank:
        name, family, source=[preset.get(k) for k in ['name','category','source']]
        values={p.get('id'):p.get('value') for p in preset}
        file=family+'-'+name.replace(' ','-')+'-Raw.wav'
        target=OUT/'Audition'/family/file
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(ROOT/'Docs/Phase6/Listening'/file,target)
        dry=OUT/'Audition/Sources'/('Source-'+source+'.wav');dry.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(ROOT/'Docs/Phase6/Listening'/dry.name,dry)
        measured=metrics(target)
        gain=float(baseline[name]['gain_db'])
        flags=[]
        if gain>6: flags.append('LIKELY_LOUD')
        if gain<-6: flags.append('LIKELY_QUIET')
        if measured['low_fraction']>.85: flags.append('BASS_HEAVY_REVIEW')
        if measured['bright_fraction']>.25: flags.append('BRIGHT_REVIEW')
        if measured['overs_samples']: flags.append('POTENTIAL_CLIPPING')
        rows.append(dict(category=family,name=name,source=source,**measured,gain_db=gain,flags=';'.join(flags)))
        setting=' / '.join(k.upper()+' '+values[k] for k in PARAMS)
        checklist += ['## '+name, '',f'Engine: {family}. Source: {source}. Character/job to verify: {JOBS[name]}.', '',setting, '',
          f'[Raw render](Audition/{family}/{quote(file)}) · [Dry source](Audition/Sources/{dry.name})', '',
          '| Evaluation | Blank result |','|---|---|','| Sound quality 1–5 | |','| Useful Yes/No | |',
          '| Too loud Yes/No | |','| Too harsh Yes/No | |','| Too bass-heavy Yes/No | |',
          '| Distinct from nearby presets Yes/No; comparison name | |','| Keep / Adjust / Remove | |','| Notes / real material used | |','']
        if family!=category:page.append('<h2>'+family+'</h2>');category=family
        page.append('<article><h3>'+html.escape(name)+'</h3><p>'+html.escape(family+' / '+source+' / '+JOBS[name])+ '</p><small>'+html.escape(setting)+ '</small><p>Dry<br><audio controls preload="none" src="'+quote(str(dry.relative_to(OUT)).replace('\\','/'))+'"></audio><br>Factory raw<br><audio controls preload="none" src="'+quote(str(target.relative_to(OUT)).replace('\\','/'))+'"></audio></p><p>'+html.escape(f"Peak {measured['peak']:.4f}; RMS {measured['rms']:.4f}; crest {measured['crest_db']:.2f} dB; gain {gain:+.2f} dB; overs {measured['overs_samples']}; flags: {'; '.join(flags) or 'none'}")+'</p></article>')
    page.append('</html>')
    write(OUT/'Preset-Audition-Checklist.md','\n'.join(checklist))
    write(OUT/'Factory-Listening.html','\n'.join(page))
    with (OUT/'preset-loudness.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    write(OUT/'Loudness-Method.md', '''# Factory loudness review

NOT a human audition approval. `preset-loudness.csv` measures the copied raw float32 stereo 48 kHz files, including their existing 10 ms edge fades. Samples preserve factory OUTPUT. Peak is linear sample peak, RMS is whole-file RMS, crest is 20 log10(peak/RMS), overs counts individual channel samples with abs(value) >= 1. This is not LUFS or intersample true-peak analysis. No automatic retuning, limiting or loudness matching occurred.

Gain dB comes from Phase 6's regenerated pre-fade output/input RMS comparison. Low proxy is one-pole 350 Hz low-pass energy / full energy; bright proxy is the residual of a one-pole 3 kHz low-pass / full energy. These overlapping proxies are not perceptual loudness or formal spectral bands.

Review thresholds: RMS gain > +6 dB likely loud; < -6 dB likely quiet; low fraction > 0.85 bass-heavy; bright residual > 0.25 bright; sample overs potential clipping. Intentional bass presets can trigger bass flags. Thresholds only nominate human checks. Compare absolute levels and context across source types cautiously; sources differ.

Similarity compares all 435 pairs on one identical mono 48 kHz one-second composite of bass, vocal-like tones and periodic high-frequency transient bursts, aligned by the fixed 60-sample latency. Only analysis vectors are divided by their own RMS. Flag normalized difference < 0.10 (approximately correlation > 0.995). Different output gains alone are intentionally removed from this character analysis; the audition files retain those gains. One source and this metric can miss musical distinctions. Confirm candidates on real material and their intended source before suggesting changes.
''')

def manual():
    write(OUT/'FL-Studio-Manual-QA.md', r'''# FL Studio manual QA — 1.0.0 RC1

**NOT RUN.** Prepared plan; no installation or FL Studio validation was performed automatically. Record PASS/FAIL, evidence and notes for every row. Use the candidate bundle, not stale `dist`.

Tester/date: ____  FL Studio version/build/bitness: ____  Windows version: ____
CPU/interface/driver: ____  candidate DLL SHA256: ____  sample rate/block: ____

1. Close FL Studio. For this manual test, copy the **whole** `build/DISRUPTION_artefacts/Release/VST3/DISRUPTION.vst3` directory to `C:\Program Files\Common Files\VST3\DISRUPTION.vst3`, retaining `Contents`. Do not rename its DLL or use the Phase 2 dist bundle. Record any old copy and remove duplicate discovery paths safely before scanning.
2. Open FL Studio: Options > File settings > Manage plugins. Enable Verify plugins; use Find installed plugins. For a replaced build enable Rescan previously verified plugins. Confirm the entry is DISRUPTION, vendor VoxxLab, effect, VST3 x64; host numeric metadata can show 1.0.0 while the editor shows **1.0.0 RC1**. Save scan evidence.
3. Find the verified effect in Plugin database > Installed > Effects, insert it on an audio mixer track and open its editor. Play a known reference recording at comfortable level.
4. Run the tests below at 44.1/48/88.2/96 kHz where the driver permits, and buffers 32/64/128/256/512/1024 where available. Mark unsupported configurations N/A with driver reason. Change buffer and rate with playback stopped, then resume; also test supported live buffer changes. Record actual driver values and xruns.
5. For each automation control, tweak it, then Tools > Last tweaked > Create automation clip. If necessary, use wrapper Browse parameters and create a clip from the named parameter. Use smooth ramps, stepped jumps and rapid alternating extremes. For Mode, HQ and Bypass verify discrete steps rather than unintended intermediate states. Play and render; compare rendered and live behavior.

| Test | PASS/FAIL/N/A | Evidence / notes |
|---|---|---|
| Discovery / identity / candidate header / editor opens | | |
| Real vocal, drum, bass/808, synth and mix playback | | |
| All five modes at Damage 0/50/100, moving every control | | |
| Drive 0..36 dB automation | | |
| Damage 0..100 automation | | |
| Tone -100..100 automation | | |
| Body 0..100 automation | | |
| Global Crush 0..100 automation | | |
| Mix 0..100 automation / dry-wet timing | | |
| Output -24..+12 dB automation (allow intentional overs) | | |
| Mode BURN/BREAK/RIP/CRUSH/CHAOS automation; names/order | | |
| HQ automation and repeated switching during playback | | |
| Bypass automation and repeated switching during playback | | |
| FL wrapper bypass versus internal Bypass; host compensation | | |
| 60 samples compensation: 1.360544/1.25/0.680272/0.625 ms | | |
| CRUSH character remains intentional with HQ on/off | | |
| All permitted sample rates / blocks and host buffer changes | | |
| Export/render, stop/start, seek, loop and undo automation | | |
| Editor close/reopen during playback; audio unaffected | | |
| Shift-drag fine adjustment, double-click defaults, tooltips | | |
| Factory INIT + 30 browse/previous/next and modified marker | | |
| Custom state save, exit FL entirely, reopen exact 10 values | | |
| Factory preset save session, restart, exact values/name retained | | |
| Save user preset, load another, reload user, restart FL persists | | |
| User project recall independent of user preset file lookup | | |
| Protected factory deletion / user overwrite confirmation | | |
| 1 instance: CPU, audio, state and UI stability | | |
| 4 instances: CPU, audio, state and UI stability | | |
| 8 instances: CPU, audio, state and UI stability | | |
| 16 instances if practical: CPU, audio, state and UI stability | | |
| Multi-instance project restart; every instance exact state | | |
| Extended real-time session, renders, no crash or runaway | | |

State procedure: set custom values Mode CHAOS, Drive 23.4, Damage 87, Tone -37, Body 41, Crush 62, Mix 73, Output -4.5, HQ on, Bypass off. Screenshot values; save `.flp`, close FL Studio fully, reopen and compare all 10. Repeat with a factory preset and with multiple differently configured instances.

File-independent user recall: save a named user preset, use it with one edited value and save the project. Close FL. Back up the corresponding UUID `.disruptionpreset` from `%APPDATA%\VoxxLab\DISRUPTION\Presets`, temporarily move that one file outside the preset folder, reopen the project and confirm embedded parameters/name/modified context survive. Restore the file afterward. Do not move unrelated files. Then verify saved user presets remain browsable after restart.

Multi-instance procedure: configure 1, 4, 8, 16 instances across mixer tracks with different modes/HQ/states, record idle and playback CPU/xruns with editor closed/open, automate several independently, save project, fully restart FL and check each instance. Record duration and resource behavior. If 16 is impractical, document the reason and maximum stable count.

Official procedure references: [Image-Line external plugin installation/verified scan](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm), [Image-Line automation clips](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/playlist_automationclip.htm). Steps describe current documentation; record differences in the installed FL build.
''')
    checks=['Full window visible','Text readable','No clipped labels','Preset browser readable','Five mode buttons aligned','DAMAGE centered','Knob labels correct','Value displays readable','HQ visible','Bypass visible','Artwork sharp enough','No overlap','No unexpected blank areas','Mouse hitboxes align','Dropdown selection / scrolling','Fine drag / default reset','Move between mixed-DPI monitors','Close/reopen after monitor move','Maximal Damage / dense waveform','Candidate version label readable']
    doc=['# Windows DPI manual QA', '', '**NOT RUN.** Automated scaled images do not validate Windows per-monitor DPI, FL wrapper scaling or mouse coordinates. Editor size is fixed 900 × 600 logical units; resizing is intentionally unsupported.', '',
      'Tester/date: ____ Windows/build: ____ FL build: ____ monitor layout/native resolution: ____', '',
      'For each scale, set Windows Display > Scale to the stated percentage, follow any OS sign-in/restart requirement, launch/restart FL Studio and open the candidate. Record FL scaling settings separately. Check real mouse interactions and dropdowns. On mixed-DPI monitors, move the live window both ways, reopen, and repeat controls. Save whole-window screenshots with scale and host settings recorded.', '']
    shots=['INIT','DAMAGE-100','CHAOS-preset','preset-dropdown','modified-user-preset','HQ-On','bypass']
    shot_rows=['scale,view,requested_filename,status,notes']
    for scale in [100,125,150,200]:
        doc+=['## '+str(scale)+'%', '', '| Check | PASS/FAIL | Notes |','|---|---|---|']
        doc += ['| '+c+' | | |' for c in checks]
        doc += ['', 'Screenshot slots (manual capture, no screenshots claimed):', '']
        for view in shots:
            file=f'DPI-{scale}-{view}.png'
            doc += ['- [ ] `Screenshots/Manual/'+file+'`']
            shot_rows.append(f'{scale},{view},{file},NOT RUN,')
        doc.append('')
    write(OUT/'Windows-DPI-Checklist.md','\n'.join(doc))
    write(OUT/'Screenshot-Slots.csv','\n'.join(shot_rows)+'\n')
    write(OUT/'Screenshots/Manual/README.md','Manual screenshots belong here. 28 expected slots are listed in ../../Screenshot-Slots.csv. No synthetic placeholders are supplied. Record actual Windows scale, host scaling and monitor resolution.\n')

def drafts():
    write(DRAFTS/'README-DRAFT.md', r'''# VoxxLab DISRUPTION

**Internal 1.0.0 RC1 documentation draft — not a released product.** Windows x64 VST3 audio destruction effect with five engines, a fixed 900 × 600 editor and audio-reactive display.

| Mode | Character |
|---|---|
| BURN | Saturation and harmonic drive |
| BREAK | Hard clipped destruction |
| RIP | Asymmetric distortion |
| CRUSH | Bit depth and sample-rate degradation |
| CHAOS | Complex nonlinear folding |

DRIVE (0..36 dB) boosts distortion input. DAMAGE (0..100%) controls engine intensity. TONE (-100..100%) shifts the tonal balance. BODY (0..100%) adds low-band weight. Global CRUSH (0..100%) adds digital degradation after the engine. MIX (0..100%) blends dry and processed audio. OUTPUT (-24..+12 dB) controls final level. HQ enables 4× oversampling where appropriate; CRUSH deliberately retains native-rate degradation. BYPASS routes the dry signal with compensation. All processing and bypass paths report 60 samples latency in both HQ states. Extreme settings can exceed full scale; lower Output as needed.

Select INIT or 30 embedded factory presets. Browse with the menu or arrows; a modified marker indicates edits. User presets support save/load and explicit overwrite/delete confirmation, use `%APPDATA%VoxxLabDISRUPTIONPresets`, and are independent from embedded project recall. Shift-drag gives finer knob adjustment; double-click resets to default.

Requirements: 64-bit Windows with a VST3-capable x64 host and system audio support. Candidate is built with JUCE 8.0.12 and static MSVC runtime; no separate debug runtime is required. A minimum supported Windows version and real-machine compatibility statement remain subject to owner validation; this build was not tested across OS versions. Mono and stereo are supported; MIDI and surround processing are not offered.

Manual installation uses the complete DISRUPTION.vst3 folder under `C:Program FilesCommon FilesVST3`. See [Installation draft](INSTALLATION-DRAFT.md). Do not use stale dist. Human audition, FL Studio, DPI and licensing acceptance are pending. Support/contact and VoxxLab licensing text must be supplied by the owner before distribution.
''')
    write(DRAFTS/'CHANGELOG-DRAFT.md', '''# Changelog draft

## DISRUPTION 1.0.0 — UNRELEASED, internal candidate RC1

- Five engines: BURN, BREAK, RIP, CRUSH and CHAOS.
- Drive, Damage, Tone, Body, global Crush, Mix and Output controls.
- HQ oversampling, fixed 60-sample latency and compensated bypass.
- INIT + 30 embedded factory presets, user preset management and independent project state recall.
- Fixed industrial 900 × 600 interface with official VoxxLab artwork and audio-reactive visualization.
- Mono/stereo Windows x64 VST3.

Phase 8 changes version/description metadata and adds validation/QA materials. DSP, preset values, parameter IDs, ranges, defaults, permanent identity and state/preset formats are frozen. This entry is a draft, not a public release announcement. No installer, dist refresh or release tag has been created.
''')
    write(DRAFTS/'INSTALLATION-DRAFT.md', r'''# Installation draft — internal RC1

This is a future manual installation guide. No installation was performed in Phase 8, and the candidate is not approved for packaging. Use an x64 Windows VST3 host; supported Windows versions must still be confirmed.

1. Close the DAW and preserve any previous plugin bundle for rollback.
2. Copy the complete `DISRUPTION.vst3` directory, including Contents, to `C:Program FilesCommon FilesVST3DISRUPTION.vst3`. Administrator permission may be needed. Do not copy only its internal DLL. Do not substitute the stale dist copy.
3. In FL Studio open Options > File settings > Manage plugins. Enable Verify plugins and select Find installed plugins. For an updated copy enable Rescan previously verified plugins. Confirm vendor VoxxLab, effect DISRUPTION, x64 VST3.
4. Open the effect on a mixer track and verify the editor shows **1.0.0 RC1**; host numeric version may show 1.0.0. Follow the manual QA checklist before acceptance.

Factory presets/artwork are embedded. User preset data is stored at `%APPDATA%VoxxLabDISRUPTIONPresets`; back it up separately. Updating/removing the VST3 bundle should not remove those files. No installer or uninstall program is provided in this phase.

[Official Image-Line scan/installation documentation](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm). [Full FL manual QA](../Phase8/FL-Studio-Manual-QA.md).
''')
    write(DRAFTS/'EULA-PENDING.md', '''# Owner licensing decision required

Placeholder only. No VoxxLab EULA or company licensing terms were found in this repository. This file grants no license and imposes no invented restrictions.

Before distribution, the owner must provide/approve the VoxxLab EULA or license, confirm rights to distribute the official artwork/product name and third-party components, confirm the applicable JUCE licensing route and required notices, and supply support/contact information and supported system requirements.

Preserve [the existing JUCE notice](../../ThirdPartyLicenses/JUCE-LICENSE.md). It states the framework's AGPLv3/commercial licensing routes. Consult [JUCE's official current license](https://juce.com/legal/juce-8-licence/) and obtain the owner's confirmation; this QA pass does not determine legal compliance or invent company terms.
''')
    write(DRAFTS/'PACKAGE-CONTENTS-DRAFT.md', '''# Future package layout — draft only

No archive or installer has been made. After manual QA and licensing acceptance, packaging can place the approved whole Windows x64 DISRUPTION.vst3 bundle alongside README, CHANGELOG, INSTALLATION, the owner-supplied EULA/license and required ThirdPartyLicenses/JUCE-LICENSE.md notice. Rename/copy draft documents only after approval. Exclude build tests, local paths, screenshots, listening renders, manifests and PDB files from a user package unless deliberately required.

`dist` remains Phase 2 evidence and must not be published as this candidate. This document describes a place for release documentation and license; it is not a completed package.
''')

if __name__ == '__main__':
    OUT.mkdir(parents=True,exist_ok=True)
    audition(); manual(); drafts()
    print('Prepared 30 unchanged raw renders, scoring checklist, listening index, loudness review, FL/DPI plans, 28 screenshot slots and release drafts.')
