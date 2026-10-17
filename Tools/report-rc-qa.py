"""Audit the built RC and frozen files, then assemble the Phase 8 completion report."""
from pathlib import Path
import csv
import hashlib
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'Docs/Phase8'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):
    raw=path.read_bytes()
    return raw.decode('utf-16') if raw[:2] in [b'\xff\xfe',b'\xfe\xff'] else raw.decode('utf-8-sig')
def rows(name): return list(csv.DictReader((OUT/name).open(encoding='utf-8')))
def write(path,text): path.write_text(text,encoding='utf-8')

def main():
    frozen=json.loads(read(OUT/'Frozen-Hashes.json'))
    dist=json.loads(read(OUT/'Dist-Manifest-Before.json'))
    changed=[p for p,h in frozen.items() if sha(ROOT/p)!=h]
    dist_now={str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'dist').rglob('*') if p.is_file()}
    assert not changed, changed
    current_frozen={str(p.relative_to(ROOT)) for folder in ['Source/DSP','Source/UI','Source/Presets','Resources'] for p in (ROOT/folder).rglob('*') if p.is_file()}
    assert current_frozen==set(frozen), 'frozen file inventory changed'
    assert dist_now==dist, 'dist changed'
    write(OUT/'Dist-Manifest-After.json',json.dumps(dist_now,indent=2)+'\n')
    bundle=ROOT/'build/DISRUPTION_artefacts/Release/VST3/DISRUPTION.vst3'
    dll=bundle/'Contents/x86_64-win/DISRUPTION.vst3'
    module=json.loads(re.sub(r',\s*([}\]])',r'\1',read(bundle/'Contents/Resources/moduleinfo.json')))
    assert module['Name']=='DISRUPTION' and module['Version']=='1.0.0'
    assert module['Factory Info']['Vendor']=='VoxxLab'
    cids=[c['CID'] for c in module['Classes']]
    assert cids==['ABCDEF019182FAEB56784C6244737270','ABCDEF011234ABCD56784C6244737270']
    raw=dll.read_bytes();pe=struct.unpack_from('<I',raw,0x3c)[0]
    assert raw[pe:pe+4]==b'PE\x00\x00' and struct.unpack_from('<H',raw,pe+4)[0]==0x8664
    version=read(OUT/'file-version.txt')
    assert all(re.search(rf'{key}\s+:\s+1\.0\.0\s',version) for key in ['FileVersion','ProductVersion'])
    assert re.search(r'IsDebug\s+:\s+False',version)
    assert 'RC1'.encode('utf-8') in raw
    dependencies=re.findall(r'^\s+([A-Za-z0-9_.-]+\.dll)\s*$',read(OUT/'dependencies.txt'),re.M|re.I)
    assert not any(re.match(r'(msvcp|vcruntime|ucrtbased)',v,re.I) for v in dependencies)
    files=[dict(path=str(p.relative_to(bundle)),bytes=p.stat().st_size,sha256=sha(p)) for p in bundle.rglob('*') if p.is_file()]
    assert len(files)==2 and not any(x['path'].lower().endswith('.pdb') for x in files)
    clean=ROOT/'build/CleanInstallSandbox/DISRUPTION.vst3'
    assert all(sha(clean/x['path'])==x['sha256'] for x in files)
    audit=dict(version='1.0.0 RC1',host_version=module['Version'],cids=cids,manufacturer='VoxxLab',architecture='Windows x64 Release',files=files,bundle_bytes=sum(x['bytes'] for x in files),dll_sha256=sha(dll),dependencies=dependencies,frozen_file_count=len(frozen),frozen_changed=changed,dist_unchanged=True)
    write(OUT/'bundle-audit.json',json.dumps(audit,indent=2)+'\n')

    log=read(ROOT/'phase8-tests.log')
    machine=json.loads(read(OUT/'test-machine.json'))
    total=int(re.search(r'(\d+) checks passed; realtime allocations: 0',log)[1])
    phase8=int(re.search(r'Phase8: (\d+) assertions passed',log)[1])
    phase8wall=float(re.search(r'Phase8: .*wall ([0-9.]+) seconds',log)[1])
    testwall=float(re.search(r'Total Test time \(real\) =\s*([0-9.]+)',log)[1])
    assert '100% tests passed' in log
    extreme=rows('extreme-matrix.csv');rate=rows('sample-rate-buffer.csv');reg=rows('phase7-audio-regression.csv');mem=rows('editor-lifecycle-memory.csv');soak=rows('soak-segments.csv');cpu=rows('cpu.csv');levels=rows('preset-loudness.csv');similar=rows('preset-similarity.csv')
    assert len(extreme)==9600 and len(rate)==5952 and len(reg)==240 and len(cpu)==40 and len(levels)==30 and len(similar)==435 and len(soak)==36
    assert all(float(r['maximum_difference'])==0 and r['latency']=='60' for r in reg)
    assert all(r['latency']=='60' for r in rate+extreme)
    assert all(float(r['peak'])<100 for r in extreme)
    assert sum(1 for p in (OUT/'Audition').rglob('*Raw.wav'))==30
    assert len(rows('Screenshot-Slots.csv'))==28
    flagged=[r for r in levels if r['flags']]
    similar_flags=[r for r in similar if r['flag']]
    min_pair=min(similar,key=lambda r:float(r['normalized_rms_difference']))
    afterwarm=mem[1:]  # First measurement is cycle 20; cycle 50 onward is steady warmed sample range.
    growth=int(afterwarm[-1]['private_bytes'])-int(afterwarm[0]['private_bytes'])
    handle_growth=int(afterwarm[-1]['handles'])-int(afterwarm[0]['handles'])
    gdi_growth=int(afterwarm[-1]['gdi_objects'])-int(afterwarm[0]['gdi_objects'])
    assert growth<64*1024*1024 and handle_growth<=8 and gdi_growth<=4, 'resource growth needs investigation'
    write(OUT/'invariants.json',json.dumps(dict(frozen_files=len(frozen),dist_files=len(dist),checks=total,phase8_checks=phase8,extreme_cases=len(extreme),rate_buffer_rows=len(rate),archived_dll_rows=len(reg),cpu_rows=len(cpu),similarity_pairs=len(similar),private_bytes_growth_after50=growth,handle_growth=handle_growth,gdi_growth=gdi_growth),indent=2)+'\n')
    cpu_table=['| Rate | Mode | HQ | Editor | Median µs | p95 µs | p99 µs | Median % of budget | p95 % | Timer FPS |','|---|---|---|---|---|---|---|---|---|---|']
    modes=['BURN','BREAK','RIP','CRUSH','CHAOS']
    for r in cpu:
        cpu_table.append('| '+ ' | '.join([r['rate'],modes[int(r['mode'])],r['hq'],r['editor'],r['median_us'],r['p95_us'],r['p99_us'],r['median_percent_budget'],r['p95_percent_budget'],r['timer_fps']])+' |')
    differences=[]
    for hz in ['48000','96000']:
        for mode in range(5):
            for hq in ['0','1']:
                pair=[r for r in cpu if r['rate']==hz and r['mode']==str(mode) and r['hq']==hq]
                closed=next(r for r in pair if r['editor']=='closed');opened=next(r for r in pair if r['editor']=='open')
                differences.append((float(opened['median_us'])/float(closed['median_us'])-1)*100)
    levels_table=['| Family | Preset | Source | Peak | RMS | Crest dB | RMS gain dB | Overs | Review flag |','|---|---|---|---|---|---|---|---|---|']
    for r in levels:
        levels_table.append(f"| {r['category']} | {r['name']} | {r['source']} | {float(r['peak']):.4f} | {float(r['rms']):.4f} | {float(r['crest_db']):.2f} | {float(r['gain_db']):+.2f} | {r['overs_samples']} | {r['flags'] or 'none'} |")
    pair_table=['| Pair | RMS-normalized difference | Correlation |','|---|---|---|']
    pair_table += [f"| {r['preset_a']} / {r['preset_b']} | {r['normalized_rms_difference']} | {r['correlation']} |" for r in similar_flags]
    source_audit=read(OUT/'source-audit.txt').strip()
    cleanlog=read(ROOT/'build/CleanInstallSandbox/clean-location.log').strip()
    fontlabel='The version label uses the existing header slot; layout and control geometry are unchanged.'
    report=f'''# Phase 8 — Release Candidate validation and final QA preparation

Internal candidate **DISRUPTION 1.0.0 RC1**. Automated validation and QA preparation are complete. Recommendation: **NOT READY FOR RC PACKAGING** until human audition, FL Studio, real Windows DPI and owner licensing acceptance are recorded. No DSP defect was found; no factory tuning or interface redesign occurred. No installer, dist refresh or Git tag was created.

## 1. Files created or modified

Production changes: `CMakeLists.txt` (numeric version, RC compile definition, description and Windows resource dependency), `Source/PluginEditor.cpp` (header version string only). Harness changes: `Tests/DisruptionTests.cpp` (Phase 8 and clean-location dispatch), new `Tests/Phase8Tests.cpp`. Tools: new `Tools/prepare-rc-qa.py`, `Tools/report-rc-qa.py`. Root `README.md` now identifies this candidate and pending gates.

New `Docs/Phase8/`: frozen/dist manifests, bundle/invariant audit JSON, native version/dependency/PE/source-path audit text, regression/extreme/rate-buffer/soak/CPU/memory/similarity/loudness CSV files, audition checklist/index/method, 30 grouped raw WAVs and six dry sources, FL/DPI/manual screenshot plans. New `Docs/ReleaseDrafts/`: README, CHANGELOG, INSTALLATION, EULA placeholder and future package-layout drafts. This report is new. Build outputs include archived `build/Phase7Baseline/DISRUPTION.vst3`, clean-location bundle/log and build/test logs. Prior suites regenerate their measurements, renders and screenshots (including the header's RC label in Phase 7 captures); historical reports and artifacts are retained.

All **{len(frozen)} frozen source/resource files** have identical SHA256 before/after. [Frozen manifest](Phase8/Frozen-Hashes.json), [invariant checks](Phase8/invariants.json). Only the editor version text changes in production UI. {fontlabel}

## 2. RC versioning approach

CMake project/plugin numeric version **1.0.0**; internal label **RC1** is a cache variable compiled as `DISRUPTION_DISPLAY_VERSION="1.0.0 RC1"`. Editor displays the combined label. VST3 module and Windows PE version metadata use numeric 1.0.0; native numeric metadata alone cannot distinguish this candidate from a future final build, so retain its label, SHA256 and acceptance records.

Permanent identity unchanged: VoxxLab / DISRUPTION, bundle `com.voxxlab.disruption`, manufacturer `VxLb`, plugin `Dsrp`; processor CID `{cids[0]}`, controller CID `{cids[1]}`. All 10 IDs/ranges/defaults/version hints and schema/preset version 1 remain unchanged.

**Metadata defect found and fixed:** JUCE 8.0.12's generated Windows `.rc` build rule did not depend on changed `Info.txt`, retaining old 0.1.0 file-version resources despite new VST3 metadata. Added the missing input dependency in our CMake. Final Windows FileVersion/ProductVersion are 1.0.0; `IsDebug=False`. No DSP/state changes were needed.

## 3. Full regression check count

**{total:,} assertions passed**, one full CTest run including Phases 1–8, **{testwall:.2f} seconds actual wall time**. Phase 8 contributes **{phase8:,}** checks in **{phase8wall:.2f} seconds**. Phase 1–7 checks remain {total-phase8:,}; no older tests were removed or weakened. Build/test evidence: `phase8-build.log`, `phase8-tests.log` and `build/Testing/Temporary/LastTest.log`.

Actual archived Phase 7 DLL comparison: **240 configurations**, all four rates, mono/stereo, five modes, both HQ states, three settings, 4096 frames each, editor open in 80 cases. **Maximum sample difference exactly 0**; old project state recalls exact 10 parameters. [DLL regression](Phase8/phase7-audio-regression.csv). Previous-phase archived compatibility tests also pass.

## 4. Zero-allocation confirmation

**Zero tracked callback `new`/`new[]` allocations** across the full suite and Phase 8 native callbacks. Tracking is enabled only around processing; prepare, editor, source fill, preset/state serialization and buffer setup are outside that scope. Tests check the counter rather than suppressing allocations. This executable's ordinary C++ allocation override does not cover arbitrary malloc/aligned allocation or the separately statically linked hosted DLL CRT; it is not a universal allocator proof. Hosted processing is separately validated for output/state/stability.

## 5. Latency confirmation

Every tested rate/block and both quality states report **60 samples**. This includes compensated bypass, partial Mix, preset restore and mode changes. Existing impulse/time-alignment tests pass; Phase 7 comparison is exact. No latency architecture change occurred.

| Rate | 60 samples in milliseconds |
|---|---|
| 44.1 kHz | 1.360544 |
| 48 kHz | 1.250000 |
| 88.2 kHz | 0.680272 |
| 96 kHz | 0.625000 |

## 6. Sample-rate results

44.1, 48, 88.2 and 96 kHz pass mono/stereo, all five modes, all 31 bank entries including INIT, HQ forced off/on and Bypass forced off/on. Each preset is serialized into a separate native processor with exact 10-value comparison, then processed with bit-exact restored audio. This exercises Tone, Body, global Crush, Mix and Output at the frozen factory combinations; previous suites and the extreme matrix cover their boundaries. [Rate/buffer results](Phase8/sample-rate-buffer.csv).

## 7. Buffer-size results

32/64/128/256/512/1024 samples pass all rates/layouts/factory presets/HQ/bypass. **{len(rate):,} rows**; each row processes four buffers before and four after explicit reprepare. The first sequence changes actual callback lengths after preparation for 32 samples, covering host buffer changes without reprepare; the second exercises explicit host reprepare. No state/latency/oversampling defect or tracked allocation was found.

## 8. Extreme DSP test results

**{len(extreme):,} cases**, all four rates × mono/stereo × six blocks × five modes × HQ off/on × Tone ±100 × ten sources. Drive 36 dB, Damage/Body/global Crush/Mix 100%, Output +12 dB. Every source runs at least 4096 samples per case; reset between cases; all outputs finite and below a conservative absolute 100 runaway guard. Silence is exactly zero. Inputs: silence, 1e-12 near-silence, full-scale sine, overload (up to 6 amplitude), DC, impulse, square, saw, bass-heavy and vocal-like. Maximum peak **{max(float(r['peak']) for r in extreme):.6f}**. Overs above 1 are intentionally allowed at +12 dB; no limiter or automatic tuning added. [Extreme matrix](Phase8/extreme-matrix.csv).

## 9. Soak-test results

Deterministic LCG seed **0xD15A8E32 / 3512372786** (recorded decimal in CSV is authoritative), **180,000 callbacks**, **{soak[-1]['frames']} frames**, **{soak[-1]['total_audio_seconds']} simulated audio seconds**, **{soak[-1]['wall_seconds']} actual wall seconds**. Simulated duration is sum(block/rate), not continuous real-time playback. All ten controls vary every 64 blocks, factory presets every 113, state capture/restore every 211, rates/layouts/prepare every 5000, editor creation every 2500, sources every 257, and six block sizes rotate. All samples finite, zero tracked callback allocations, maximum peak {soak[-1]['peak']}. [36 progress segments](Phase8/soak-segments.csv). Native soak complements actual hosted lifecycle and manual real-time host testing.

## 10. Final CPU benchmarks

Stereo, 64 samples at 48/96 kHz; five modes × both HQ × editor closed/open = **40 configurations**, 200 warm-up + **20,000 timed callbacks per configuration**. Drive 24/Damage 85/Tone 35/Body 45/global Crush 30, Mix 100/Output 0. Callback time excludes input fill. Percent budget = callback µs / (64/rate × 1e6) × 100. Test machine: {machine['cpu_name']}, {machine['logical_processors']} logical processors, AMD64; Windows registry build {machine['os_build']}.{machine['os_revision']} / {machine['os_display_version']}. [Recorded machine context](Phase8/test-machine.json). Machine/background scheduling is not isolated; these distributions are a measured review, not a real-time deadline guarantee.

{chr(10).join(cpu_table)}

## 11. UI-open versus UI-closed benchmarks

Actual JUCE 30 Hz message-thread timer runs concurrently with the audio worker. Each editor-open case lasts at least one second; changed frames paint into a preallocated opaque RGB canvas on the message thread. This avoids treating expensive transparent screenshot creation as production paint. Open/closed median change across 20 matched pairs spans **{min(differences):+.2f}% to {max(differences):+.2f}%**. These medians show no material callback interference in this harness run; inspect p95/p99 scheduling outliers before drawing real-time conclusions. [CPU CSV](Phase8/cpu.csv). Native tests do not settle FL wrapper GPU, OS DPI, driver xrun or real-time scheduling behavior; those remain manual.

## 12. Memory/resource audit

**520 editor object create/destroy cycles**, 13 processor lifetimes, 520 factory loads and 1560 mode/Damage/HQ/bypass processing steps. The native cycle test is mostly offscreen; the hosted 29-instance lifecycle test also opens actual desktop editor windows. Fixed geometry checked each cycle; resizing intentionally unsupported. Sampling process private bytes/working set/handle/GDI counts after warm-up and every 50 cycles shows private-byte end change from cycle 50 **{growth:+,} bytes**, handles **{handle_growth:+}**, GDI objects **{gdi_growth:+}**. Conservative review limits (64 MiB growth, +8 handles, +4 GDI) pass. This short process-level plateau review does not replace a dedicated heap leak detector or prove all lifetime paths leak-free. [Memory samples](Phase8/editor-lifecycle-memory.csv).

Official artwork is 1254 × 1254, **{(ROOT/'Resources/DisruptionArtwork.png').stat().st_size:,} compressed bytes**; transient ARGB decode about 6.00 MiB, retained 640 × 121 wordmark plus 128 × 124 emblem at most 373,248 pixel bytes/editor. Visualizer bridge 4112 bytes; UI visualizer object 5872 bytes in prior suite, fixed 1024-float incoming buffer, 256-float waveform and 24-particle pool (480 bytes). Seven knob cached pixel buffers about 775,224 bytes at 1× / 3,100,896 at 2×, excluding allocator/host backing stores. Factory XML **{(ROOT/'Resources/FactoryPresets.xml').stat().st_size:,} bytes**, 31 immutable records with 10 controls each; user catalog depends on user files and is scanned off callback. No unused production resource was found: both CMake binary-data assets are referenced. Docs/tests were not deleted.

## 13. Production-path audit

Production has no hardcoded development-directory dependency. Factory XML and official PNG use compiled BinaryData; UI reads/crops the embedded PNG. User presets use `%APPDATA%\\VoxxLab\\DISRUPTION\\Presets` and only saving creates the intended directory. No preset file parsing, image decoding or user scans were added to processing. [Path/resource scan](Phase8/resource-path-audit.txt), [source audit](Phase8/source-audit.txt).

Audit matches retained intentionally: lock-free atomic static_assert (compile-time contract); temporary full-image decode comment (true lifetime description); `juce::TemporaryFile` and temporary-save error text (production safe save). `setUserDirectory` permits test/user-data directory injection off callback; editor `refreshVisuals` and `animationFrameCount` expose real message-thread behavior without enabling production test branches. No TODO/FIXME/DBG/Logger/console logging/stale phase references or inappropriate release assertions were found in Source. CMake's Debug runtime option is legitimate configuration; Release uses /MT. Updated stale product description and version text. No valid comments were blindly removed.

## 14. Any hardcoded local paths found

**None in production Source**. Generated build files and test/development reports necessarily contain local build paths; they are not installed runtime resources. Dependency/PE audit does not show an external development DLL import. The existing `build.ps1` dist-copy workflow remains unused.

## 15. VST3 bundle contents and size

Windows x64 Release, product DISRUPTION, vendor VoxxLab, numeric version 1.0.0 / internal RC1. Exact bundle:

{chr(10).join('- `'+x['path']+'`: '+format(x['bytes'],',')+' bytes.' for x in files)}

Total **{audit['bundle_bytes']:,} bytes** ({audit['bundle_bytes']/1048576:.2f} MiB). DLL SHA256 `{audit['dll_sha256']}`. Permanent CIDs above unchanged. Embedded XML/art live in the DLL; external Resources contains only moduleinfo.json. No PDB, debug CRT, source files, test outputs, listening files or suspicious extra bundle files. PE debug directory contains COFF-group linker information, not a bundled PDB. [Machine-readable audit](Phase8/bundle-audit.json), [PE headers](Phase8/pe-headers.txt), [Windows file version](Phase8/file-version.txt).

## 16. Dependency/runtime analysis

MSVC 19.29 / VS2019 BuildTools, Windows SDK 10.0.19041, JUCE 8.0.12, static /MT Release. Dumpbin direct imports: **{len(dependencies)} Windows system DLLs**: {', '.join('`'+d+'`' for d in dependencies)}. No separate MSVCP/VCRUNTIME DLL and no debug CRT import. `dbghelp.dll` is a Windows diagnostics DLL, not a debug C runtime; networking-named Windows imports come from JUCE framework linkage, not an external preset/art dependency. [Dependencies](Phase8/dependencies.txt). Inspection covers direct imports; it is not an exhaustive second-machine or all optional/transitive OS components test. Confirm supported Windows versions and graphics/audio environment manually before distribution.

## 17. Clean-location load results

Copied the whole candidate to `build/CleanInstallSandbox/DISRUPTION.vst3`, verified both copied file hashes, and ran a **fresh process** with current directory `build/CleanInstallSandbox` and `--clean-location`. That branch does not invoke prior baseline archives or native artwork/preset fixture reads. Sandbox contains only copied bundle and test log. Executable/harness remains outside sandbox; development folders were not renamed/deleted and the process was not placed in a filesystem isolation container. Thus this demonstrates relocation and embedded resource loading under the test host, not complete proof on a fresh OS.

```text
{cleanlog}
```

Factory/image embedding also follows frozen BinaryData references, native all-bank/editor tests and DLL/source resource audit. Actual hosted editor creation does not prove visual sharpness, dropdown interaction or absence of every possible fallback access; real installation remains manual. No app installation was done.

## 18. Factory preset loudness analysis

All 30 regenerated Phase 6 raw representative renders copied byte-for-byte into engine folders. Raw factory Output stays intact; existing 10 ms edge fades retained. Six synthetic roles: vocal, drums, bass, guitar, synth, fx. Whole-file sample peak/RMS/crest/overs and 350 Hz low / 3 kHz residual energy proxies measured. No LUFS or intersample true-peak claim. Relative RMS gain uses regenerated pre-fade Phase 6 dry comparison. Flag thresholds and limitations: [method](Phase8/Loudness-Method.md). **{len(flagged)} presets flagged for review**, none automatically altered. Intentional bass programs can be flagged without being defects.

{chr(10).join(levels_table)}

## 19. Factory preset similarity flags

All **435** pairs on one identical mono composite source at 48 kHz, one second, 60-sample aligned; vectors RMS-normalized **for analysis only**. Candidate threshold normalized RMS difference < 0.10. Closest pair `{min_pair['preset_a']}` / `{min_pair['preset_b']}`: {min_pair['normalized_rms_difference']}. **{len(similar_flags)} candidates**. No presets removed or retuned. A single composite source/metric cannot establish perceptual redundancy; human listening on intended sources is decisive. [All pairs](Phase8/preset-similarity.csv).

{chr(10).join(pair_table) if similar_flags else 'No pair crossed the candidate threshold on this reference.'}

## 20. Human preset audition checklist

[Docs/Phase8/Preset-Audition-Checklist.md](Phase8/Preset-Audition-Checklist.md): all 30 names, engines, source roles, provisional character jobs, all 10 parameter values, HQ and blank quality/useful/loud/harsh/bassy/distinct/keep-adjust-remove/notes fields. Status **NOT RUN**; no human audition approval claimed.

## 21. Preset listening index

[Docs/Phase8/Factory-Listening.html](Phase8/Factory-Listening.html), grouped BURN/BREAK/RIP/CRUSH/CHAOS, dry and raw audio links, intended use, exact parameters and metrics. `Docs/Phase8/Audition/<ENGINE>/*Raw.wav` contains 30 raw files; `Audition/Sources` contains six dry files. No matched renders presented as natural factory levels.

## 22. FL Studio manual QA checklist

[Docs/Phase8/FL-Studio-Manual-QA.md](Phase8/FL-Studio-Manual-QA.md): whole-bundle manual install, verified scan, discovery, actual audio, every control automation including Mode/HQ/Bypass, exact project restart, file-independent user recall, user persistence, all rate/block settings, 1/4/8/16 instances and multi-instance project restart. Status **NOT RUN**, supported by linked official Image-Line documentation.

## 23. Windows DPI checklist

[Docs/Phase8/Windows-DPI-Checklist.md](Phase8/Windows-DPI-Checklist.md): PASS/FAIL fields at actual Windows 100/125/150/200%, geometry/readability/artwork/control hitboxes/dropdowns/mixed-monitor moves/reopen. [28 manual screenshot slots](Phase8/Screenshot-Slots.csv): INIT, Damage 100, CHAOS preset, dropdown, modified user, HQ and bypass for each scale. Slots intentionally blank; prior automated scaled captures remain evidence only of software rendering, not OS DPI acceptance.

## 24. README draft

[Docs/ReleaseDrafts/README-DRAFT.md](ReleaseDrafts/README-DRAFT.md): product/modes/controls/presets/HQ/latency/basic installation/Windows x64 requirements and pending support/OS/licensing decisions. [Installation draft](ReleaseDrafts/INSTALLATION-DRAFT.md). No invented minimum Windows compatibility claim.

## 25. Changelog draft

[Docs/ReleaseDrafts/CHANGELOG-DRAFT.md](ReleaseDrafts/CHANGELOG-DRAFT.md): initial **DISRUPTION 1.0.0 — UNRELEASED, internal candidate RC1** entry. [Future package document placement](ReleaseDrafts/PACKAGE-CONTENTS-DRAFT.md), [owner EULA placeholder](ReleaseDrafts/EULA-PENDING.md). No existing VoxxLab EULA found; JUCE third-party notice retained and owner licensing route still requires confirmation.

## 26. Exact current Release VST3 path

`C:\\Users\\Admin\\Desktop\\Coding Projects\\DISRUPTION\\build\\DISRUPTION_artefacts\\Release\\VST3\\DISRUPTION.vst3`

This is the validated internal candidate. **dist still contains Phase 2** and was untouched: identical before/after hashes for both files. Stale dist DLL SHA256 `{dist['dist'+chr(92)+'DISRUPTION.vst3'+chr(92)+'Contents'+chr(92)+'x86_64-win'+chr(92)+'DISRUPTION.vst3']}`. Do not publish it as RC1.

## 27. BLOCKER issues

- Missing recorded human factory-preset audition acceptance.
- Missing actual FL Studio audio/automation/project-restart/multi-instance acceptance.
- Missing actual Windows DPI/mixed-monitor/hitbox acceptance.
- Owner VoxxLab licensing/EULA/art distribution rights, JUCE licensing route/notices and support requirements have not been approved for distribution.

These are **RC packaging gates**, not undisclosed automated test failures. No known DSP crash/state/compatibility/latency defect remained after tests. The Windows metadata generation defect was corrected and revalidated.

## 28. IMPORTANT issues

- Review nominated bass/brightness/similarity presets with humans; synthetic flags do not justify automatic changes.
- Validate driver CPU/xruns and OS/graphics compatibility in actual supported hosts/machines; accelerated deterministic timings do not certify real-time operation.
- Supply supported Windows version policy and support/contact fields (current VST3 URL/e-mail metadata is blank) before public release.

## 29. MINOR issues

- A one-second similarity reference and simple spectral proxies can miss audible differences; longer recorded material can supplement the prepared human review.
- Version numeric metadata intentionally cannot encode RC1; preserve editor label, filename/context and DLL hash in acceptance records.
- Process-level memory sampling is bounded and empirical; a dedicated heap/GPU diagnostic session could strengthen confidence.

## 30. MANUAL VALIDATION REQUIRED

Run and sign the factory audition, FL Studio and DPI documents; provide 28 real screenshots; test clean installation on a second Windows machine where available; accept licensing/system/support requirements. Document tester/host/OS/driver/build hash, PASS/FAIL and any proposed correction. No human result has been filled in automatically.

## 31. Recommendation

**NOT READY FOR RC PACKAGING.** Automated Phase 8 validation and manual QA preparation are complete, with frozen sound/parameters/factory values/identity/layout and passing compatibility/stress tests. Stop here pending the manual gates. No installer, distribution refresh, release tag or public final 1.0.0 declaration.
'''
    # Use the recorded seed, rather than a hand-transcribed decimal representation.
    report=report.replace('3512372786',soak[-1]['seed'])
    write(ROOT/'Docs/Phase8-Report.md',report)
    print(f'Phase 8 report: {total:,} checks, {phase8:,} Phase 8; all freeze/dist/bundle/coverage/resource invariants pass.')

if __name__=='__main__':main()
