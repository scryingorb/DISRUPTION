# DISRUPTION Phase 6 completion — 2026-10-07

INIT + 30 embedded factory presets, versioned user presets and a functional temporary browser are implemented and built for Windows x64 Release. Final automated validation passed. Version remains 0.1.0. Development stops here before the final visual UI.

## Implementation and storage

**1. Files.** Created `Source/Presets/PresetManager.h`, `Source/Presets/PresetManager.cpp`, `Resources/FactoryPresets.xml`, `Tests/Phase6Tests.cpp`, `Tools/inspect-presets.py`, this report, and the `Docs/Phase6` measurements, inventory, revisions, listening index and WAVs. Modified `CMakeLists.txt`, `Source/PluginProcessor.h/.cpp`, `Source/PluginEditor.h/.cpp`, `Tests/DisruptionTests.cpp`, and `README.md`. Build outputs, logs, editor snapshot and an archived actual Phase 5 bundle were generated. Earlier suites regenerated their existing validation artifacts. No DSP implementation file was edited.

**2. Factory storage.** Human-readable `Resources/FactoryPresets.xml` is compiled into the `DisruptionPresets` BinaryData target and linked into the plugin. INIT plus 30 presets are available without any installed resource directory, absolute path, network or `dist` dependency. Factory entries are immutable through the preset API; saving the same name creates a separate user entry.

**3–4. User storage and directory.** One UTF-8 XML `.disruptionpreset` file per user preset. Stable UUID filenames keep user names separate from paths. JUCE `File::userApplicationDataDirectory` provides the platform base; Windows resolves to `%APPDATA%\VoxxLab\DISRUPTION\Presets`, here `C:\Users\Admin\AppData\Roaming\VoxxLab\DISRUPTION\Presets`. Directory creation happens on Save. Tests use only `build/Phase6UserTests`; no test presets were written to AppData. The directory accessor/injection keeps platform handling independent of the browser.

Saves write a temporary sibling file and use JUCE's checked replacement operation. A duplicate case-insensitive user name fails unless the caller explicitly sets overwrite; the UI asks for confirmation before doing that. Deletion requires confirmation and an existing catalog user ID; factory IDs are rejected. Navigation is INIT, six BURN, six BREAK, six RIP, six CRUSH, six CHAOS, then case-insensitively sorted users. It wraps both directions. An unavailable/custom project ID navigates to INIT. Directory candidates are sorted before duplicate-name filtering so winners are deterministic. User scans occur on editor opening and save/delete, not continuously; externally edited files appear after reopening the editor.

**5–6. Format and versioning.** Root `DISRUPTION_PRESET` has `version="1"`, `id`, `name`, `category`, `author`, `source`, `manufacturer="VoxxLab"`, `plugin="DISRUPTION"`; each `PARAM` contains its permanent `id` and plain numeric `value`. All ten parameters, including HQ and BYPASS, are serialized. BYPASS is included so user preset recall is exact; all factory BYPASS values are Off. Existing DAW `DISRUPTION_STATE` schema version remains 1; optional `PRESET_CONTEXT` stores the name/origin and loaded baseline in addition to the full original parameter state. Additive unknown metadata/parameter IDs are ignored within format 1. Missing known parameters use INIT defaults. A future incompatible format must receive explicit migration support; unsupported versions are rejected rather than guessed.

**7. Errors.** Files must exist and be no larger than 1 MiB. XML root, product, manufacturer, version and 1–64-character readable name are checked. Path/control characters are rejected in names. Duplicate known parameter IDs, nonnumeric/NaN/infinite values, fractional/out-of-range modes and nonbinary booleans fail. Continuous values are clamped to permanent ranges. Validation completes before application, so failed loads preserve parameters and preset context. Invalid catalog files are skipped with diagnostics; browser tooltips retain scan errors. Failed save/delete operations return readable errors. The parser does not evaluate instructions or other unknown document content.

**8. Switching.** Loading notifies the existing APVTS parameters as a serialized ten-parameter batch. An atomic odd/even revision marks the batch. The callback takes a complete stable control snapshot, or keeps the prior one if a batch overlaps its read; it never waits for the writer. Once loading returns, the complete logical parameter state is active. Established mode crossfade, DRIVE/DAMAGE/TONE/BODY/global CRUSH/MIX/OUTPUT smoothing, HQ fade and BYPASS smoothing remain responsible for the sound transition. No smoothing constants, engine tuning or latency behavior changed.

**9. Modified status.** The message-thread timer compares all ten values with the loaded/saved baseline using a tolerance below one 0.01 parameter step. Edits or automation show `*`; returning to the baseline clears it. Name and baseline survive project recall, including a project saved while modified. Metadata locks, strings, XML, catalog copies and files stay outside `processBlock`; the callback adds only atomic revision reads and a POD control copy.

**10. Temporary browser.** Existing 840×460 dark/orange editor retains its knobs/layout. Added current name/modified/origin status, categorized dropdown, previous/next, Save Preset with text entry and confirmed overwrite, and confirmed Delete enabled only for catalog user presets. HQ is correctly labeled `HQ 4x`. Screenshot: `build/editor-preview.png`. No final artwork, custom knob redesign, animation or waveform UI was started.

## Factory bank

**11–15. Full names, categories, values, HQ and INIT.** Mode numbers remain BURN=0, BREAK=1, RIP=2, CRUSH=3, CHAOS=4. DRIVE and OUTPUT are dB; other continuous controls are percent. Every factory BYPASS is Off. HQ 1 means On. Source indicates the intended starting job; names are original/descriptive, unique and have no artist/product attribution. INIT uses the specified safe default ten-parameter state. [Machine-readable inventory](Phase6/Factory-Parameters.csv) and [canonical bank](../Resources/FactoryPresets.xml).

| Category | Name | Source / job | Mode | Drive | Damage | Tone | Body | Crush | Mix | Output | HQ | Bypass |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| INIT | INIT | reference | 0 | 0 | 0 | 0 | 0 | 0 | 100 | 0 | 0 | 0 |
| BURN | Vocal Heat | vocal | 0 | 3 | 18 | 8 | 10 | 0 | 35 | -1 | 1 | 0 |
| BURN | Warm Blood | vocal | 0 | 8 | 38 | -25 | 45 | 0 | 65 | -6.68 | 1 | 0 |
| BURN | Bass Furnace | bass | 0 | 14 | 55 | -45 | 70 | 0 | 85 | -11.5 | 1 | 0 |
| BURN | Drum Ember | drums | 0 | 12 | 45 | 40 | 20 | 0 | 50 | -4 | 0 | 0 |
| BURN | Melted Synth | synth | 0 | 18 | 65 | -32 | 25 | 15 | 85 | -12.88 | 1 | 0 |
| BURN | Full Burn | guitar | 0 | 30 | 90 | -12 | 20 | 0 | 100 | -17.28 | 1 | 0 |
| BREAK | Redline Vocal | vocal | 1 | 12 | 58 | -18 | 15 | 0 | 55 | -7.03 | 1 | 0 |
| BREAK | Snare Breaker | drums | 1 | 18 | 75 | 28 | 35 | 20 | 80 | -9.43 | 0 | 0 |
| BREAK | Flatline 808 | bass | 1 | 24 | 85 | -35 | 60 | 0 | 90 | -13.97 | 1 | 0 |
| BREAK | Concrete Drums | drums | 1 | 22 | 78 | -15 | 35 | 35 | 65 | -10.75 | 1 | 0 |
| BREAK | Hardwired | synth | 1 | 26 | 90 | 35 | 0 | 0 | 100 | -10.61 | 1 | 0 |
| BREAK | Total Collapse | fx | 1 | 36 | 100 | -60 | 25 | 70 | 100 | -9.74 | 1 | 0 |
| RIP | Torn Voice | vocal | 2 | 12 | 55 | 15 | 30 | 0 | 65 | -7.38 | 1 | 0 |
| RIP | Blown Cone | guitar | 2 | 20 | 80 | -65 | 15 | 25 | 100 | -17.24 | 0 | 0 |
| RIP | Crooked Bass | bass | 2 | 16 | 60 | -22 | 55 | 0 | 85 | -12.04 | 1 | 0 |
| RIP | Razor Lead | synth | 2 | 24 | 85 | 40 | 0 | 35 | 90 | -9.15 | 1 | 0 |
| RIP | Factory Floor | drums | 2 | 22 | 78 | -25 | 25 | 60 | 80 | -11.6 | 0 | 0 |
| RIP | Shredded | fx | 2 | 36 | 100 | 25 | 15 | 80 | 100 | -9.94 | 1 | 0 |
| CRUSH | Digital Dust | vocal | 3 | 2 | 22 | 10 | 0 | 0 | 30 | -1 | 0 | 0 |
| CRUSH | Broken Codec | vocal | 3 | 7 | 55 | -25 | 20 | 35 | 70 | -5.64 | 0 | 0 |
| CRUSH | Bit Wound | synth | 3 | 10 | 72 | 25 | 0 | 0 | 100 | -5.86 | 0 | 0 |
| CRUSH | Dead Circuit | drums | 3 | 14 | 78 | -15 | 35 | 55 | 75 | -9.52 | 0 | 0 |
| CRUSH | Low Resolution | synth | 3 | 5 | 88 | -40 | 20 | 20 | 100 | -5.84 | 0 | 0 |
| CRUSH | System Failure | fx | 3 | 24 | 100 | -35 | 35 | 100 | 100 | -17.06 | 0 | 0 |
| CHAOS | Fracture | guitar | 4 | 3 | 18 | 10 | 15 | 0 | 25 | -1 | 0 | 0 |
| CHAOS | Voice Damage | vocal | 4 | 10 | 48 | -35 | 35 | 0 | 55 | -10.68 | 1 | 0 |
| CHAOS | Folded Mass | bass | 4 | 16 | 55 | -30 | 65 | 15 | 85 | -9.19 | 1 | 0 |
| CHAOS | Reactor | synth | 4 | 22 | 78 | 25 | 10 | 0 | 90 | -8.84 | 1 | 0 |
| CHAOS | Signal Collapse | fx | 4 | 20 | 88 | -50 | 20 | 65 | 65 | -4 | 0 | 0 |
| CHAOS | MELTDOWN | fx | 4 | 36 | 100 | -25 | 40 | 85 | 100 | -6 | 1 | 0 |

The six jobs in each family progress from practical color to heavier effects: BURN vocal warmth, bass weight, drum heat, synth melt and overdrive; BREAK vocal clipping, transient clipping, 808 clipping, dense drum bus, hard synth and collapse; RIP asymmetric voice, blown speaker, bass, lead, industrial percussion and shredding; CRUSH subtle grit, codec degradation, game texture, degraded percussion, low-resolution synth and digital failure; CHAOS folded color, damaged voice, folded bass, reactive synth, transition effect and maximum folding. MIX spans 25–100%, DAMAGE spans 18–100% outside INIT, global CRUSH is deliberately absent in many entries, and HQ is mixed rather than universal.

## Measurement and validation

**16. Levels.** All presets were rendered through the production processor at 48 kHz, stereo, on deterministic four-second role-appropriate synthetic material. These are vocal-like harmonics/envelopes, drum transients/noise, bass partials, guitar-like plucks, synth partials and an effects sweep, not recorded singers or instruments. Metrics are measured before audition edge fades. No runtime gain normalization or limiter was added. OUTPUT was adjusted per preset while retaining natural loudness differences. The low-band proxy is energy after a 350 Hz one-pole lowpass divided by full-band energy; it is not a loudness or spectral certification.

| Category | Preset | Input RMS | Output RMS | Peak | RMS gain dB | Low-band proxy % |
|---|---|---:|---:|---:|---:|---:|
| INIT | INIT | 0.1042 | 0.1038 | 0.2473 | -0.04 | 67.7 |
| BURN | Vocal Heat | 0.0780 | 0.0802 | 0.1618 | +0.24 | 71.6 |
| BURN | Warm Blood | 0.0780 | 0.1041 | 0.2036 | +2.50 | 74.3 |
| BURN | Bass Furnace | 0.1576 | 0.2997 | 0.4473 | +5.58 | 97.0 |
| BURN | Drum Ember | 0.0464 | 0.0578 | 0.2629 | +1.91 | 83.6 |
| BURN | Melted Synth | 0.1042 | 0.1651 | 0.2912 | +4.00 | 71.1 |
| BURN | Full Burn | 0.0726 | 0.1292 | 0.1699 | +5.00 | 70.4 |
| BREAK | Redline Vocal | 0.0780 | 0.1102 | 0.2208 | +3.00 | 72.9 |
| BREAK | Snare Breaker | 0.0464 | 0.0735 | 0.3207 | +4.00 | 81.1 |
| BREAK | Flatline 808 | 0.1576 | 0.2357 | 0.3165 | +3.50 | 95.5 |
| BREAK | Concrete Drums | 0.0464 | 0.0778 | 0.3158 | +4.50 | 90.2 |
| BREAK | Hardwired | 0.1042 | 0.1652 | 0.3006 | +4.00 | 57.0 |
| BREAK | Total Collapse | 0.1161 | 0.2064 | 0.4774 | +5.00 | 36.4 |
| RIP | Torn Voice | 0.0780 | 0.1103 | 0.2061 | +3.00 | 72.7 |
| RIP | Blown Cone | 0.0726 | 0.1220 | 0.2096 | +4.50 | 70.0 |
| RIP | Crooked Bass | 0.1576 | 0.2359 | 0.3485 | +3.50 | 96.9 |
| RIP | Razor Lead | 0.1042 | 0.1653 | 0.2935 | +4.00 | 59.8 |
| RIP | Factory Floor | 0.0464 | 0.0778 | 0.3623 | +4.50 | 92.8 |
| RIP | Shredded | 0.1161 | 0.2065 | 0.4100 | +5.00 | 16.3 |
| CRUSH | Digital Dust | 0.0780 | 0.0734 | 0.1485 | -0.53 | 71.2 |
| CRUSH | Broken Codec | 0.0780 | 0.0983 | 0.1976 | +2.00 | 73.4 |
| CRUSH | Bit Wound | 0.1042 | 0.1473 | 0.3865 | +3.00 | 63.5 |
| CRUSH | Dead Circuit | 0.0464 | 0.0734 | 0.3481 | +4.00 | 95.5 |
| CRUSH | Low Resolution | 0.1042 | 0.1390 | 0.3711 | +2.50 | 69.9 |
| CRUSH | System Failure | 0.1161 | 0.1948 | 0.3493 | +4.50 | 78.9 |
| CHAOS | Fracture | 0.0726 | 0.0759 | 0.1752 | +0.38 | 64.4 |
| CHAOS | Voice Damage | 0.0780 | 0.1168 | 0.1998 | +3.50 | 74.4 |
| CHAOS | Folded Mass | 0.1576 | 0.2359 | 0.4640 | +3.50 | 87.0 |
| CHAOS | Reactor | 0.1042 | 0.1651 | 0.3101 | +4.00 | 17.2 |
| CHAOS | Signal Collapse | 0.1161 | 0.1688 | 0.5469 | +3.25 | 32.5 |
| CHAOS | MELTDOWN | 0.1161 | 0.1287 | 0.5719 | +0.90 | 30.0 |

Final reference RMS gains span -0.53 to +5.58 dB. Highest raw sample peak is 0.5719; every render is finite and below full scale. Bass-oriented entries naturally concentrate energy below 350 Hz; that result alone does not indicate BODY overload. MELTDOWN remains a destructive texture rather than a loudness maximizer: heavy folding reduces coherent energy, so its modest RMS gain is intentional. These limits describe the tested inputs; arbitrary hotter inputs are not guaranteed to remain below full scale.

Within-family distinctness uses all 75 pairs on the same synth reference, after settling, with RMS gain removed. All normalized difference RMS values exceed 0.05. The closest is Blown Cone / Factory Floor at 0.05401. This detects near-identical waveforms rather than proving subjective distinctness. [All pairs](Phase6/distinctness.csv).

**17. Revisions and listening status.** Initial output gains reached approximately +15.74 dB and System Failure peaked above full scale on its reference. OUTPUT reductions corrected the measured level problems. Drum Ember's shared-reference difference from Vocal Heat was only 0.01874; DRIVE/DAMAGE/TONE/MIX were revised. Bass Furnace initially remained close to Warm Blood (0.03929); darker TONE, greater BODY and stronger DRIVE/DAMAGE/MIX separated their jobs. No DSP retuning was used. [Exact revision log](Phase6/Revisions.txt). These changes were guided by measured renders; no human audition has been performed or claimed. Real vocal/drum/guitar listening and subjective harshness judgement remain the principal listening limitation before release.

**18. Listening files.** `Docs/Phase6/Listening` contains 31 raw factory renders, 31 offline RMS-matched versions, seven dry source files, and `INIT-and-30-Matched-Sequence.wav` (124 seconds, INIT then factory navigation order). All are stereo 48 kHz 32-bit float WAVs. Matched copies target 0.1 RMS offline; file-edge fades avoid export cuts. [Open the local listening index](Phase6/Factory-Listening.html), which provides dry/raw/matched controls and the full sequence. Regenerate with `python Tools/inspect-presets.py` after tests.

**19. Tests.** Final CTest run: 1/1 suite passed, **145,704,983 assertions total**, including **12,641,693 Phase 6 assertions** and all prior suites. These counts include per-sample assertions, not that many independent scenarios. Parsing/count/unique names/IDs/categories/defaults/ranges/INIT, circular navigation, save/load, case-insensitive denied and allowed overwrite, corrupt XML, nonfinite/invalid enums, duplicate parameters, unknown fields, missing fields, dirty/revert, protected factory deletion, user deletion and exact independent project recall passed. Factory application verifies every parameter, including HQ and BYPASS. Mono/stereo and 44.1/48/88.2/96 kHz are exercised, with both quality settings in the bank and all mode/rate/layout/quality combinations in archive comparison. Existing tests retain sample-rate reduction, full-stack MIX, layout, smoothing, lifecycle, alias analysis and earlier-state coverage. Log: `phase6-tests.log`.

**20. Rapid switching.** 620 switches per rate/layout = 4,960 changes at 64-frame callback boundaries; every preset is repeatedly exercised. All output samples were finite; latency remained 60. Highest switch-run peak 0.5538; greatest adjacent-sample step 0.2307. Digital holds/folding deliberately produce discontinuities; this raw step is not an isolated click audibility threshold. A deliberately odd revision with partial parameter changes produced output exactly equal to the preceding stable controls. An additional writer thread loaded 500 presets while audio processed at least 500 stereo blocks; finite output and zero tracked callback allocations passed. [Switch matrix](Phase6/switching.csv).

**21–22. State and external independence.** Actual archived Phase 5 VST3 states restore into Phase 6 across all five modes, both qualities, four rates and both layouts (80 configurations). Both DLLs report 60 samples and output matches sample-for-sample with maximum difference **0**. Phase 5 also reads the new state while ignoring optional preset metadata, preserving all ten parameters. A user preset was saved, loaded, modified, its project state captured, and the external file removed; a new processor restored exact parameters, name and dirty status without reading that file. Legacy states without context receive a Custom project label. Earlier Phase 1–4 regression suites also passed. DAW recall stores actual values and never resolves a preset filename.

**23. Real-time behavior.** Tracked ordinary `new`/`new[]` calls during the instrumented test executable's processing callbacks: **0**. Preparation, catalog parsing, file I/O and user actions execute outside those scopes. `processBlock` contains no preset filesystem calls, directory scan, XML, string work, metadata lock or waiting loop. The allocation counter does not intercept all malloc/aligned allocation paths or a separately loaded DLL's CRT; the result is accurately a tracked callback-allocation test, supported by hot-path inspection.

**24. CPU against Phase 5.** Actual archived Phase 5 and current Phase 6 VST3 DLLs, same full active stack, stereo 64 samples, 48/96 kHz, all five modes and both HQ states. Each row has 200 warmup and 1,000 timed blocks, excluding input generation. Median callback difference across 20 paired settings: +0.00%; range -1.29% to +0.83%. This is a local timing comparison, not a portable hardware promise. The normal-playback preset infrastructure introduces no meaningful regression in these measurements. [Median/p95 data](Phase6/phase5-cpu.csv).

| Rate | Mode | HQ | Phase 5 median µs | Phase 6 median µs | Change % |
|---:|---|---:|---:|---:|---:|
| 48000 | BURN | 0 | 43.00 | 43.00 | +0.00 |
| 48000 | BURN | 1 | 156.20 | 157.50 | +0.83 |
| 48000 | BREAK | 0 | 41.40 | 41.20 | -0.48 |
| 48000 | BREAK | 1 | 148.90 | 149.30 | +0.27 |
| 48000 | RIP | 0 | 39.10 | 39.10 | +0.00 |
| 48000 | RIP | 1 | 135.50 | 136.10 | +0.44 |
| 48000 | CRUSH | 0 | 38.60 | 38.40 | -0.52 |
| 48000 | CRUSH | 1 | 38.80 | 38.30 | -1.29 |
| 48000 | CHAOS | 0 | 39.20 | 39.10 | -0.26 |
| 48000 | CHAOS | 1 | 136.00 | 135.60 | -0.29 |
| 96000 | BURN | 0 | 42.40 | 42.60 | +0.47 |
| 96000 | BURN | 1 | 153.00 | 153.60 | +0.39 |
| 96000 | BREAK | 0 | 40.50 | 40.20 | -0.74 |
| 96000 | BREAK | 1 | 146.50 | 146.80 | +0.20 |
| 96000 | RIP | 0 | 38.10 | 38.40 | +0.79 |
| 96000 | RIP | 1 | 133.50 | 133.20 | -0.22 |
| 96000 | CRUSH | 0 | 37.60 | 37.60 | +0.00 |
| 96000 | CRUSH | 1 | 37.60 | 37.50 | -0.27 |
| 96000 | CHAOS | 0 | 37.90 | 37.90 | +0.00 |
| 96000 | CHAOS | 1 | 134.10 | 133.40 | -0.52 |

## Current build and remaining concerns

**25. Exact Release VST3.** `C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3`

Inner Windows x64 binary: `C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3\Contents\x86_64-win\DISRUPTION.vst3`

SHA256: `155E9CF7EF3220C1FC914D09E65C65319BB2C6C722FC986F319B613D992B1140`

Archived Phase 5 binary SHA256: `BBAAAC2B58C9C5C9F9F9B40F08778E531DEF59BF5707E0E47D5A9D5B538530D6`.

`dist/DISRUPTION.vst3` is still the stale Phase 2 package and was not refreshed (inner SHA256 `5933B0D5942ED2E50D910F670435C40F544B32C9D49830ED874D23BB881F7699`). Do not use it as this phase's build. No installer, installation or release packaging was performed.

**26. Before the final UI phase.** Preset architecture and automated validation are ready for UI integration. Human audition on real recordings and representative DAW automation/undo workflows remains advisable; generated synthetic renders cannot settle subjective harshness or musical usefulness. HQ CPU cost remains the established oversampling cost. Catalog editing is a non-real-time API contract; simultaneous host automation can intentionally change a loaded preset and mark it modified. Scan refresh is currently tied to browser opening and save/delete. The temporary UI handles save/overwrite/delete errors but is not a polished final browser. No new DSP defect was found. Permanent product/manufacturer codes, bundle ID, parameter IDs/ranges, schema compatibility, 60-sample timing and version remain unchanged. Stop after Phase 6; no final UI work has begun.
