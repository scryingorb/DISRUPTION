# Phase 8 — Release Candidate validation and final QA preparation

Internal candidate **DISRUPTION 1.0.0 RC1**. Automated validation and QA preparation are complete. Recommendation: **NOT READY FOR RC PACKAGING** until human audition, FL Studio, real Windows DPI and owner licensing acceptance are recorded. No DSP defect was found; no factory tuning or interface redesign occurred. No installer, dist refresh or Git tag was created.

## 1. Files created or modified

Production changes: `CMakeLists.txt` (numeric version, RC compile definition, description and Windows resource dependency), `Source/PluginEditor.cpp` (header version string only). Harness changes: `Tests/DisruptionTests.cpp` (Phase 8 and clean-location dispatch), new `Tests/Phase8Tests.cpp`. Tools: new `Tools/prepare-rc-qa.py`, `Tools/report-rc-qa.py`. Root `README.md` now identifies this candidate and pending gates.

New `Docs/Phase8/`: frozen/dist manifests, bundle/invariant audit JSON, native version/dependency/PE/source-path audit text, regression/extreme/rate-buffer/soak/CPU/memory/similarity/loudness CSV files, audition checklist/index/method, 30 grouped raw WAVs and six dry sources, FL/DPI/manual screenshot plans. New `Docs/ReleaseDrafts/`: README, CHANGELOG, INSTALLATION, EULA placeholder and future package-layout drafts. This report is new. Build outputs include archived `build/Phase7Baseline/DISRUPTION.vst3`, clean-location bundle/log and build/test logs. Prior suites regenerate their measurements, renders and screenshots (including the header's RC label in Phase 7 captures); historical reports and artifacts are retained.

All **37 frozen source/resource files** have identical SHA256 before/after. [Frozen manifest](Phase8/Frozen-Hashes.json), [invariant checks](Phase8/invariants.json). Only the editor version text changes in production UI. The version label uses the existing header slot; layout and control geometry are unchanged.

## 2. RC versioning approach

CMake project/plugin numeric version **1.0.0**; internal label **RC1** is a cache variable compiled as `DISRUPTION_DISPLAY_VERSION="1.0.0 RC1"`. Editor displays the combined label. VST3 module and Windows PE version metadata use numeric 1.0.0; native numeric metadata alone cannot distinguish this candidate from a future final build, so retain its label, SHA256 and acceptance records.

Permanent identity unchanged: VoxxLab / DISRUPTION, bundle `com.voxxlab.disruption`, manufacturer `VxLb`, plugin `Dsrp`; processor CID `ABCDEF019182FAEB56784C6244737270`, controller CID `ABCDEF011234ABCD56784C6244737270`. All 10 IDs/ranges/defaults/version hints and schema/preset version 1 remain unchanged.

**Metadata defect found and fixed:** JUCE 8.0.12's generated Windows `.rc` build rule did not depend on changed `Info.txt`, retaining old 0.1.0 file-version resources despite new VST3 metadata. Added the missing input dependency in our CMake. Final Windows FileVersion/ProductVersion are 1.0.0; `IsDebug=False`. No DSP/state changes were needed.

## 3. Full regression check count

**350,132,333 assertions passed**, one full CTest run including Phases 1–8, **242.73 seconds actual wall time**. Phase 8 contributes **202,948,346** checks in **152.88 seconds**. Phase 1–7 checks remain 147,183,987; no older tests were removed or weakened. Build/test evidence: `phase8-build.log`, `phase8-tests.log` and `build/Testing/Temporary/LastTest.log`.

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

32/64/128/256/512/1024 samples pass all rates/layouts/factory presets/HQ/bypass. **5,952 rows**; each row processes four buffers before and four after explicit reprepare. The first sequence changes actual callback lengths after preparation for 32 samples, covering host buffer changes without reprepare; the second exercises explicit host reprepare. No state/latency/oversampling defect or tracked allocation was found.

## 8. Extreme DSP test results

**9,600 cases**, all four rates × mono/stereo × six blocks × five modes × HQ off/on × Tone ±100 × ten sources. Drive 36 dB, Damage/Body/global Crush/Mix 100%, Output +12 dB. Every source runs at least 4096 samples per case; reset between cases; all outputs finite and below a conservative absolute 100 runaway guard. Silence is exactly zero. Inputs: silence, 1e-12 near-silence, full-scale sine, overload (up to 6 amplitude), DC, impulse, square, saw, bass-heavy and vocal-like. Maximum peak **16.683900**. Overs above 1 are intentionally allowed at +12 dB; no limiter or automatic tuning added. [Extreme matrix](Phase8/extreme-matrix.csv).

## 9. Soak-test results

Deterministic LCG seed **0xD15A8E32 / 3512372786** (recorded decimal in CSV is authoritative), **180,000 callbacks**, **60480000 frames**, **986.786 simulated audio seconds**, **38.6302 actual wall seconds**. Simulated duration is sum(block/rate), not continuous real-time playback. All ten controls vary every 64 blocks, factory presets every 113, state capture/restore every 211, rates/layouts/prepare every 5000, editor creation every 2500, sources every 257, and six block sizes rotate. All samples finite, zero tracked callback allocations, maximum peak 21.5359. [36 progress segments](Phase8/soak-segments.csv). Native soak complements actual hosted lifecycle and manual real-time host testing.

## 10. Final CPU benchmarks

Stereo, 64 samples at 48/96 kHz; five modes × both HQ × editor closed/open = **40 configurations**, 200 warm-up + **20,000 timed callbacks per configuration**. Drive 24/Damage 85/Tone 35/Body 45/global Crush 30, Mix 100/Output 0. Callback time excludes input fill. Percent budget = callback µs / (64/rate × 1e6) × 100. Test machine: 13th Gen Intel(R) Core(TM) i5-13600K, 20 logical processors, AMD64; Windows registry build 26200.9457 / 25H2. [Recorded machine context](Phase8/test-machine.json). Machine/background scheduling is not isolated; these distributions are a measured review, not a real-time deadline guarantee.

| Rate | Mode | HQ | Editor | Median µs | p95 µs | p99 µs | Median % of budget | p95 % | Timer FPS |
|---|---|---|---|---|---|---|---|---|---|
| 48000 | BURN | 0 | closed | 23.4 | 24.5 | 35.4 | 1.755 | 1.8375 | 0 |
| 48000 | BURN | 0 | open | 23.5 | 32.6 | 44.3 | 1.7625 | 2.445 | 29.9449 |
| 48000 | BURN | 1 | closed | 83.7 | 91.8 | 119 | 6.2775 | 6.885 | 0 |
| 48000 | BURN | 1 | open | 83.8 | 94.3 | 147.9 | 6.285 | 7.0725 | 29.4107 |
| 48000 | BREAK | 0 | closed | 21.9 | 23.1 | 37.6 | 1.6425 | 1.7325 | 0 |
| 48000 | BREAK | 0 | open | 21.9 | 23 | 33.9 | 1.6425 | 1.725 | 29.952 |
| 48000 | BREAK | 1 | closed | 77.7 | 86.2 | 130.7 | 5.8275 | 6.465 | 0 |
| 48000 | BREAK | 1 | open | 77.9 | 88.8 | 130.4 | 5.8425 | 6.66 | 30.1244 |
| 48000 | RIP | 0 | closed | 21.5 | 22.4 | 30.1 | 1.6125 | 1.68 | 0 |
| 48000 | RIP | 0 | open | 21.6 | 22.7 | 38.8 | 1.62 | 1.7025 | 29.9505 |
| 48000 | RIP | 1 | closed | 72.9 | 79.9 | 111.1 | 5.4675 | 5.9925 | 0 |
| 48000 | RIP | 1 | open | 72.9 | 80.3 | 108 | 5.4675 | 6.0225 | 29.8865 |
| 48000 | CRUSH | 0 | closed | 20.7 | 21.6 | 27.5 | 1.5525 | 1.62 | 0 |
| 48000 | CRUSH | 0 | open | 20.8 | 21.7 | 29.5 | 1.56 | 1.6275 | 29.9927 |
| 48000 | CRUSH | 1 | closed | 20.8 | 27.4 | 39.2 | 1.56 | 2.055 | 0 |
| 48000 | CRUSH | 1 | open | 20.7 | 21.8 | 38.3 | 1.5525 | 1.635 | 29.983 |
| 48000 | CHAOS | 0 | closed | 21.6 | 22.7 | 37.5 | 1.62 | 1.7025 | 0 |
| 48000 | CHAOS | 0 | open | 21.6 | 23.2 | 39.2 | 1.62 | 1.74 | 29.9435 |
| 48000 | CHAOS | 1 | closed | 73.1 | 79.6 | 106.3 | 5.4825 | 5.97 | 0 |
| 48000 | CHAOS | 1 | open | 73.4 | 84.4 | 141.3 | 5.505 | 6.33 | 29.8927 |
| 96000 | BURN | 0 | closed | 22.5 | 29.9 | 52.7 | 3.375 | 4.485 | 0 |
| 96000 | BURN | 0 | open | 22.5 | 30.9 | 47.6 | 3.375 | 4.635 | 29.9523 |
| 96000 | BURN | 1 | closed | 82.5 | 108.4 | 174.6 | 12.375 | 16.26 | 0 |
| 96000 | BURN | 1 | open | 82.5 | 96.7 | 170.5 | 12.375 | 14.505 | 29.4019 |
| 96000 | BREAK | 0 | closed | 21.2 | 22.5 | 36.5 | 3.18 | 3.375 | 0 |
| 96000 | BREAK | 0 | open | 21.1 | 25.4 | 49 | 3.165 | 3.81 | 29.9754 |
| 96000 | BREAK | 1 | closed | 76.2 | 84.1 | 124.4 | 11.43 | 12.615 | 0 |
| 96000 | BREAK | 1 | open | 76.3 | 86.7 | 145.2 | 11.445 | 13.005 | 29.4475 |
| 96000 | RIP | 0 | closed | 20.5 | 21.6 | 37 | 3.075 | 3.24 | 0 |
| 96000 | RIP | 0 | open | 20.5 | 21.8 | 38.4 | 3.075 | 3.27 | 29.9762 |
| 96000 | RIP | 1 | closed | 71 | 78.2 | 125.2 | 10.65 | 11.73 | 0 |
| 96000 | RIP | 1 | open | 72.9 | 82.1 | 130.4 | 10.935 | 12.315 | 30.3068 |
| 96000 | CRUSH | 0 | closed | 20 | 24.6 | 35.5 | 3 | 3.69 | 0 |
| 96000 | CRUSH | 0 | open | 20 | 22.4 | 42.6 | 3 | 3.36 | 29.9878 |
| 96000 | CRUSH | 1 | closed | 20 | 21.2 | 38 | 3 | 3.18 | 0 |
| 96000 | CRUSH | 1 | open | 20 | 21.1 | 29.4 | 3 | 3.165 | 29.9572 |
| 96000 | CHAOS | 0 | closed | 20.6 | 21.7 | 26.2 | 3.09 | 3.255 | 0 |
| 96000 | CHAOS | 0 | open | 20.7 | 21.8 | 28.9 | 3.105 | 3.27 | 29.9748 |
| 96000 | CHAOS | 1 | closed | 71.8 | 104.2 | 141.2 | 10.77 | 15.63 | 0 |
| 96000 | CHAOS | 1 | open | 71.8 | 106.8 | 145.3 | 10.77 | 16.02 | 29.9438 |

## 11. UI-open versus UI-closed benchmarks

Actual JUCE 30 Hz message-thread timer runs concurrently with the audio worker. Each editor-open case lasts at least one second; changed frames paint into a preallocated opaque RGB canvas on the message thread. This avoids treating expensive transparent screenshot creation as production paint. Open/closed median change across 20 matched pairs spans **-0.48% to +2.68%**. These medians show no material callback interference in this harness run; inspect p95/p99 scheduling outliers before drawing real-time conclusions. [CPU CSV](Phase8/cpu.csv). Native tests do not settle FL wrapper GPU, OS DPI, driver xrun or real-time scheduling behavior; those remain manual.

## 12. Memory/resource audit

**520 editor object create/destroy cycles**, 13 processor lifetimes, 520 factory loads and 1560 mode/Damage/HQ/bypass processing steps. The native cycle test is mostly offscreen; the hosted 29-instance lifecycle test also opens actual desktop editor windows. Fixed geometry checked each cycle; resizing intentionally unsupported. Sampling process private bytes/working set/handle/GDI counts after warm-up and every 50 cycles shows private-byte end change from cycle 50 **-37,945,344 bytes**, handles **+0**, GDI objects **+0**. Conservative review limits (64 MiB growth, +8 handles, +4 GDI) pass. This short process-level plateau review does not replace a dedicated heap leak detector or prove all lifetime paths leak-free. [Memory samples](Phase8/editor-lifecycle-memory.csv).

Official artwork is 1254 × 1254, **1,945,835 compressed bytes**; transient ARGB decode about 6.00 MiB, retained 640 × 121 wordmark plus 128 × 124 emblem at most 373,248 pixel bytes/editor. Visualizer bridge 4112 bytes; UI visualizer object 5872 bytes in prior suite, fixed 1024-float incoming buffer, 256-float waveform and 24-particle pool (480 bytes). Seven knob cached pixel buffers about 775,224 bytes at 1× / 3,100,896 at 2×, excluding allocator/host backing stores. Factory XML **17,625 bytes**, 31 immutable records with 10 controls each; user catalog depends on user files and is scanned off callback. No unused production resource was found: both CMake binary-data assets are referenced. Docs/tests were not deleted.

## 13. Production-path audit

Production has no hardcoded development-directory dependency. Factory XML and official PNG use compiled BinaryData; UI reads/crops the embedded PNG. User presets use `%APPDATA%\VoxxLab\DISRUPTION\Presets` and only saving creates the intended directory. No preset file parsing, image decoding or user scans were added to processing. [Path/resource scan](Phase8/resource-path-audit.txt), [source audit](Phase8/source-audit.txt).

Audit matches retained intentionally: lock-free atomic static_assert (compile-time contract); temporary full-image decode comment (true lifetime description); `juce::TemporaryFile` and temporary-save error text (production safe save). `setUserDirectory` permits test/user-data directory injection off callback; editor `refreshVisuals` and `animationFrameCount` expose real message-thread behavior without enabling production test branches. No TODO/FIXME/DBG/Logger/console logging/stale phase references or inappropriate release assertions were found in Source. CMake's Debug runtime option is legitimate configuration; Release uses /MT. Updated stale product description and version text. No valid comments were blindly removed.

## 14. Any hardcoded local paths found

**None in production Source**. Generated build files and test/development reports necessarily contain local build paths; they are not installed runtime resources. Dependency/PE audit does not show an external development DLL import. The existing `build.ps1` dist-copy workflow remains unused.

## 15. VST3 bundle contents and size

Windows x64 Release, product DISRUPTION, vendor VoxxLab, numeric version 1.0.0 / internal RC1. Exact bundle:

- `Contents\Resources\moduleinfo.json`: 1,080 bytes.
- `Contents\x86_64-win\DISRUPTION.vst3`: 10,319,872 bytes.

Total **10,320,952 bytes** (9.84 MiB). DLL SHA256 `9e184973c0cc179bc0f633a84d794814946eb4bebb3b461eccb32a29490e65f5`. Permanent CIDs above unchanged. Embedded XML/art live in the DLL; external Resources contains only moduleinfo.json. No PDB, debug CRT, source files, test outputs, listening files or suspicious extra bundle files. PE debug directory contains COFF-group linker information, not a bundled PDB. [Machine-readable audit](Phase8/bundle-audit.json), [PE headers](Phase8/pe-headers.txt), [Windows file version](Phase8/file-version.txt).

## 16. Dependency/runtime analysis

MSVC 19.29 / VS2019 BuildTools, Windows SDK 10.0.19041, JUCE 8.0.12, static /MT Release. Dumpbin direct imports: **21 Windows system DLLs**: `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `SHELL32.dll`, `ole32.dll`, `OLEAUT32.dll`, `COMDLG32.dll`, `ADVAPI32.dll`, `WININET.dll`, `WS2_32.dll`, `VERSION.dll`, `SHLWAPI.dll`, `WINMM.dll`, `dbghelp.dll`, `d2d1.dll`, `dxgi.dll`, `d3d11.dll`, `dcomp.dll`, `IMM32.dll`, `COMCTL32.dll`, `dwmapi.dll`. No separate MSVCP/VCRUNTIME DLL and no debug CRT import. `dbghelp.dll` is a Windows diagnostics DLL, not a debug C runtime; networking-named Windows imports come from JUCE framework linkage, not an external preset/art dependency. [Dependencies](Phase8/dependencies.txt). Inspection covers direct imports; it is not an exhaustive second-machine or all optional/transitive OS components test. Confirm supported Windows versions and graphics/audio environment manually before distribution.

## 17. Clean-location load results

Copied the whole candidate to `build/CleanInstallSandbox/DISRUPTION.vst3`, verified both copied file hashes, and ran a **fresh process** with current directory `build/CleanInstallSandbox` and `--clean-location`. That branch does not invoke prior baseline archives or native artwork/preset fixture reads. Sandbox contains only copied bundle and test log. Executable/harness remains outside sandbox; development folders were not renamed/deleted and the process was not placed in a filesystem isolation container. Thus this demonstrates relocation and embedded resource loading under the test host, not complete proof on a fresh OS.

```text
Phase8 clean-location 1/4/8/16 simultaneous instances; 29 recreate/state/editor lifetimes; four format-manager reloads; wall 4.08154 seconds
1908519 clean-location checks passed
```

Factory/image embedding also follows frozen BinaryData references, native all-bank/editor tests and DLL/source resource audit. Actual hosted editor creation does not prove visual sharpness, dropdown interaction or absence of every possible fallback access; real installation remains manual. No app installation was done.

## 18. Factory preset loudness analysis

All 30 regenerated Phase 6 raw representative renders copied byte-for-byte into engine folders. Raw factory Output stays intact; existing 10 ms edge fades retained. Six synthetic roles: vocal, drums, bass, guitar, synth, fx. Whole-file sample peak/RMS/crest/overs and 350 Hz low / 3 kHz residual energy proxies measured. No LUFS or intersample true-peak claim. Relative RMS gain uses regenerated pre-fade Phase 6 dry comparison. Flag thresholds and limitations: [method](Phase8/Loudness-Method.md). **7 presets flagged for review**, none automatically altered. Intentional bass programs can be flagged without being defects.

| Family | Preset | Source | Peak | RMS | Crest dB | RMS gain dB | Overs | Review flag |
|---|---|---|---|---|---|---|---|---|
| BURN | Vocal Heat | vocal | 0.1618 | 0.0801 | 6.10 | +0.24 | 0 | none |
| BURN | Warm Blood | vocal | 0.2036 | 0.1040 | 5.84 | +2.50 | 0 | none |
| BURN | Bass Furnace | bass | 0.4473 | 0.2993 | 3.49 | +5.58 | 0 | BASS_HEAVY_REVIEW |
| BURN | Drum Ember | drums | 0.2629 | 0.0578 | 13.16 | +1.91 | 0 | none |
| BURN | Melted Synth | synth | 0.2912 | 0.1649 | 4.94 | +4.00 | 0 | none |
| BURN | Full Burn | guitar | 0.1645 | 0.1290 | 2.11 | +5.00 | 0 | none |
| BREAK | Redline Vocal | vocal | 0.2208 | 0.1101 | 6.04 | +3.00 | 0 | none |
| BREAK | Snare Breaker | drums | 0.3207 | 0.0735 | 12.80 | +4.00 | 0 | none |
| BREAK | Flatline 808 | bass | 0.3165 | 0.2354 | 2.57 | +3.50 | 0 | BASS_HEAVY_REVIEW |
| BREAK | Concrete Drums | drums | 0.3158 | 0.0778 | 12.17 | +4.50 | 0 | BASS_HEAVY_REVIEW |
| BREAK | Hardwired | synth | 0.3006 | 0.1649 | 5.21 | +4.00 | 0 | none |
| BREAK | Total Collapse | fx | 0.4774 | 0.2059 | 7.30 | +5.00 | 0 | none |
| RIP | Torn Voice | vocal | 0.2061 | 0.1101 | 5.45 | +3.00 | 0 | none |
| RIP | Blown Cone | guitar | 0.1998 | 0.1218 | 4.30 | +4.50 | 0 | none |
| RIP | Crooked Bass | bass | 0.3485 | 0.2356 | 3.40 | +3.50 | 0 | BASS_HEAVY_REVIEW |
| RIP | Razor Lead | synth | 0.2781 | 0.1650 | 4.53 | +4.00 | 0 | none |
| RIP | Factory Floor | drums | 0.3623 | 0.0778 | 13.37 | +4.50 | 0 | BASS_HEAVY_REVIEW |
| RIP | Shredded | fx | 0.4100 | 0.2064 | 5.96 | +5.00 | 0 | none |
| CRUSH | Digital Dust | vocal | 0.1485 | 0.0733 | 6.13 | -0.53 | 0 | none |
| CRUSH | Broken Codec | vocal | 0.1976 | 0.0981 | 6.08 | +2.00 | 0 | none |
| CRUSH | Bit Wound | synth | 0.3865 | 0.1471 | 8.39 | +3.00 | 0 | none |
| CRUSH | Dead Circuit | drums | 0.3481 | 0.0734 | 13.52 | +4.00 | 0 | BASS_HEAVY_REVIEW |
| CRUSH | Low Resolution | synth | 0.3711 | 0.1389 | 8.54 | +2.50 | 0 | none |
| CRUSH | System Failure | fx | 0.3493 | 0.1946 | 5.08 | +4.50 | 0 | none |
| CHAOS | Fracture | guitar | 0.1752 | 0.0757 | 7.29 | +0.38 | 0 | none |
| CHAOS | Voice Damage | vocal | 0.1998 | 0.1166 | 4.67 | +3.50 | 0 | none |
| CHAOS | Folded Mass | bass | 0.4640 | 0.2356 | 5.89 | +3.50 | 0 | BASS_HEAVY_REVIEW |
| CHAOS | Reactor | synth | 0.3101 | 0.1649 | 5.49 | +4.00 | 0 | none |
| CHAOS | Signal Collapse | fx | 0.5469 | 0.1683 | 10.24 | +3.25 | 0 | none |
| CHAOS | MELTDOWN | fx | 0.5719 | 0.1282 | 12.99 | +0.90 | 0 | none |

## 19. Factory preset similarity flags

All **435** pairs on one identical mono composite source at 48 kHz, one second, 60-sample aligned; vectors RMS-normalized **for analysis only**. Candidate threshold normalized RMS difference < 0.10. Closest pair `Vocal Heat` / `Digital Dust`: 0.0153095. **10 candidates**. No presets removed or retuned. A single composite source/metric cannot establish perceptual redundancy; human listening on intended sources is decisive. [All pairs](Phase8/preset-similarity.csv).

| Pair | RMS-normalized difference | Correlation |
|---|---|---|
| Vocal Heat / Drum Ember | 0.0990342 | 0.995096 |
| Vocal Heat / Digital Dust | 0.0153095 | 0.999883 |
| Vocal Heat / Fracture | 0.0254507 | 0.999676 |
| Warm Blood / Redline Vocal | 0.0935918 | 0.99562 |
| Warm Blood / Broken Codec | 0.0756361 | 0.99714 |
| Drum Ember / Digital Dust | 0.0997012 | 0.99503 |
| Drum Ember / Fracture | 0.0888298 | 0.996055 |
| Melted Synth / Crooked Bass | 0.0970263 | 0.995293 |
| Blown Cone / Factory Floor | 0.0483288 | 0.998832 |
| Digital Dust / Fracture | 0.0339495 | 0.999424 |

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

`C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3`

This is the validated internal candidate. **dist still contains Phase 2** and was untouched: identical before/after hashes for both files. Stale dist DLL SHA256 `5933b0d5942ed2e50d910f670435c40f544b32c9d49830ed874d23bb881f7699`. Do not publish it as RC1.

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
