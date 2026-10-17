# DISRUPTION Phase 7 — Final Interface and Visual Identity

Completed 2026-10-07. Fixed 900 × 600 editor, official uploaded artwork, custom industrial controls, five mode buttons, complete preset browser and an audio-reactive 30 FPS display. The sound, factory values, permanent IDs, version 0.1.0, latency and saved formats remain frozen. Release packaging has not started.

[Editor preview gallery](Phase7/Editor-Previews.html) · [2× maximum-DAMAGE preview](Phase7/Screenshots/DAMAGE-100-2x.png)

## Files and architecture

**1. Created or significantly modified files.**

| File | Responsibility |
|---|---|
| `Source/UI/DisruptionLookAndFeel.h/.cpp` | Shared palette, metal rotary drawing, buttons, switches, menu and text styling |
| `Source/UI/DisruptionKnob.h` | Parameter-attached labeled rotary, units, resets and fine dragging |
| `Source/UI/ModeSelector.h` | Five mutually exclusive mode buttons with parameter gestures |
| `Source/UI/PresetBar.h/.cpp` | Categorized browser, current identity, dirty indication, save/overwrite/delete dialogs |
| `Source/UI/DisruptionVisualizer.h/.cpp` | Waveform styling, bounded segment renderer and fixed particle pool |
| `Source/UI/VisualizerBridge.h` | Preallocated single-producer/single-consumer audio bridge |
| `Source/PluginEditor.h/.cpp` | Final panel composition, official art, layout, 30 Hz refresh and tooltip window |
| `Source/PluginProcessor.h/.cpp` | Owns bridge and publishes read-only output samples after existing processing |
| `Resources/DisruptionArtwork.png` | Exact official uploaded source image, embedded by BinaryData |
| `CMakeLists.txt` | Compiles UI files, embeds art and includes Phase 7 tests |
| `Tests/Phase7Tests.cpp` | UI/attachment/lifecycle tests, captures, CPU/memory data and Phase 6 DLL comparison |
| `Tests/DisruptionTests.cpp` | Runs Phase 7 and checks the newly requested 900 × 600 dimensions |
| `Tools/inspect-ui.py` | Local screenshot contact sheet |
| `README.md`, `Docs/Phase7-Report.md` | Current build instructions and completion report |
| `Docs/Phase7/*` | Frozen hashes, regression/performance/memory CSVs, gallery and 15 PNG previews |
| `build/Phase6Baseline/DISRUPTION.vst3` | Actual Phase 6 DLL archived before editing |

Build logs and generated JUCE resources were produced; prior tests regenerated their existing numerical/listening artifacts. No `Source/DSP` file or `Resources/FactoryPresets.xml` was edited. The old editor-size assertion was updated to the requested dimensions and strengthened with a height check; prior DSP assertions remain intact.

**2. Dimensions.** Fixed **900 × 600 logical pixels**. User resizing is disabled. Windows/JUCE scaling can increase physical pixel size without changing component geometry. The 2× render is 1800 × 1200.

**3. Overall visual architecture.** Compact gunmetal header contains emblem, official fractured wordmark and VOXXLAB, preset controls and compact HQ/BYPASS switches. Five illuminated mode buttons occupy a consistent horizontal rail. A large central DAMAGE dial dominates; DRIVE/BODY/MIX sit left and TONE/global CRUSH/OUTPUT sit right. Engraved rails, screws, thin seams and restrained charcoal gradients establish a machine panel. The orange/red trace lives behind the central control, with lower intensity near side labels. Selected state uses text, outline and underline, not only a subtle color change. Footer communicates signal activity and the selected engine personality.

**4. Official artwork.** The exact uploaded PNG is copied into `Resources` and embedded; runtime never accesses Downloads or an external absolute image path. The header uses the relevant wordmark and emblem regions rather than displaying the whole square. JUCE takes clipped image views and resamples to 640×121 wordmark and 128×124 emblem assets. Proportional placement preserves their aspect ratios. The full decode is temporary and released after editor construction; retained art supports the header through 200% scaling. No generative replacement of the official logo or font redistribution was used.

**5. LookAndFeel.** `DisruptionLookAndFeel` centralizes rotary surfaces/rings/markings, button and toggle geometry, preset menu colors and value-field styling. Components own their attachments and behavior; `PluginEditor` composes them. Windows Bahnschrift is requested for industrial panel typography, with JUCE/system fallback for ordinary text and editable values. No font file is bundled. LookAndFeel lifetime outlasts the controls and is detached on editor destruction.

## Controls

**6. DAMAGE.** A 282×308 component contains the signature dial. Gradient metal face, outer ticks, internal ring, subtle engraved fracture and a near-white indicator establish clear rotation. Center percent readout stays readable while the glow interpolates from dim ember to orange/red as value increases. The control remains physically stationary. The lower editable value is also a percent, with no unnecessary decimal precision. JUCE caches knob painting; parameter changes invalidate the cache.

**7. Smaller rotaries.** The six 155×115 groups share the same face/ring/indicator language, simplified at smaller scale. DRIVE and OUTPUT show signed one-decimal dB values, e.g. `+12.0 dB` and `-3.0 dB`; other values show rounded percent, while underlying 0.01-step parameter precision remains unchanged. Editable text accepts numeric values with or without the displayed suffix. Double-click resets use permanent parameter defaults: DRIVE/DAMAGE/TONE/BODY/CRUSH/OUTPUT → 0; MIX → 100. Normal vertical/horizontal JUCE dragging remains intact; holding Shift when beginning a drag changes sensitivity from 180 to 900 for fine control. Labels, accessible slider titles and concise help tooltips are provided.

**8. Modes.** Five coherent BURN/BREAK/RIP/CRUSH/CHAOS buttons have selected orange edges/underlines and progressively fractured slash details. `juce::ParameterAttachment` reads the existing unnormalized mode and uses `setValueAsCompleteGesture` on clicks. Host automation and preset loads synchronize all five selected states. No new mode parameter, range or engine is introduced.

**9. Presets.** Phase 6 functionality is retained in a modular `PresetBar`: previous/next, category menu, exact current name with `*`, factory/user/project status, Save and safe confirmed Delete. All 31 factory entries remain available. Missing-file project identity still displays correctly even without a matching menu ID. Overwrite confirmation and factory protection remain unchanged. SafePointer callbacks prevent modal actions from accessing a destroyed editor. The preset model/file format is untouched; user tests inject `build/Phase7UserTests` and remove their test preset. No user preset was created in AppData.

**10. HQ/BYPASS.** Compact illuminated switches show explicit ON/OFF text and remain bound through APVTS ButtonAttachments. BYPASS dims waveform activity without hiding values; signal status also identifies bypass. HQ behavior, native digital CRUSH and the fixed 60-sample timing are unchanged. Tooltips explain their purpose.

## Visualization and real-time safety

**11. Architecture.** `DisruptionVisualizer` paints sampled actual processed output, stylized by engine: smoothed BURN, flattened BREAK, asymmetric RIP, stepped CRUSH and folded/fractured CHAOS. The trace uses the newest 96 display points from a 256-sample UI history. This is an expressive activity display, not a calibrated oscilloscope or reconstruction of the DSP. Horizontal sampling is intentionally decimated. Glow intensity and subtle breaks respond to DAMAGE. The renderer uses bounded individual line segments rather than a large self-intersecting stroked path.

**12. Bridge.** `VisualizerBridge` has a fixed 1,024-float JUCE AbstractFifo. When an editor is active, the callback publishes one sample every 32 frames after the existing HQ/bypass/output processing. Stereo chooses the channel with greater absolute amplitude to avoid cancelling anti-phase material. Values are read, never written back into the audio buffer. A full FIFO drops excess display samples; it never waits or overwrites unread slots. The UI drains at most 1,024 floats per frame. Only the processor's single editor consumes the FIFO. Closed editors disable publishing with a lock-free atomic flag. Compile-time lock-free atomic requirements are checked. No parsing, strings, file access, dynamic particles or locks enter the callback.

**13. Refresh.** One 30 Hz editor timer updates visualization, footer and preset identity. Independent ordinary preset catalog polling remains on the message thread. Measured timer cadence in concurrent rendering benchmarks is 29.75–29.91 FPS. This measures timer dispatch/offscreen paints under the test message loop, not a DAW compositor's displayed presentation rate.

**14. Particles/glitches.** Exactly 24 preallocated particles, each five floats. Peaks with DAMAGE >75% spawn at most one particle every three frames; lifetime, velocity and gentle gravity update in place. Small segmented gaps and bounded CHAOS offsets provide corruption character. The dial and hit targets never move. Bypass suppresses spawning. Effects remain subordinate to values and controls.

**15. Allocations.** **Zero tracked ordinary `new`/`new[]` allocations in instrumented audio callbacks**, including bridge publishing, open-editor playback and concurrent editor lifecycle. This counter does not intercept every malloc/aligned path or a separately loaded DLL's CRT. Hot-path inspection confirms fixed arrays, stack indices and lock-free FIFO operations; artwork decoding, cached-image creation, dialogs and drawing allocations stay on the UI thread.

## Performance and memory

**16. Open versus closed performance.** Windows Release, stereo, 48 kHz, 64 samples. BURN and CHAOS each tested HQ Off/On with active post stages, DAMAGE 95%, editor closed versus open. An audio worker ran 200 warmup + 10,000 timed callbacks while the message thread dispatched the real timer and captured offscreen paints. Input generation and UI rendering are excluded from callback timing. Ordinary Windows scheduling and shared-system load affect tails; this is not a real DAW xrun test.

| Mode | HQ | Editor | Callback median µs | Callback p95 µs | Full paint median ms | Full paint p95 ms | Timer FPS |
|---|---:|---|---:|---:|---:|---:|---:|
| BURN | 0 | closed | 23.50 | 41.20 | 0.000 | 0.000 | 0.00 |
| BURN | 0 | open | 23.50 | 41.90 | 4.343 | 5.738 | 29.75 |
| BURN | 1 | closed | 83.70 | 96.30 | 0.000 | 0.000 | 0.00 |
| BURN | 1 | open | 83.50 | 92.60 | 2.715 | 5.253 | 29.80 |
| CHAOS | 0 | closed | 21.40 | 22.30 | 0.000 | 0.000 | 0.00 |
| CHAOS | 0 | open | 21.50 | 22.30 | 2.783 | 5.447 | 29.91 |
| CHAOS | 1 | closed | 72.10 | 81.10 | 0.000 | 0.000 | 0.00 |
| CHAOS | 1 | open | 72.10 | 87.90 | 3.073 | 5.593 | 29.84 |

Open/closed medians are effectively unchanged at this measurement resolution; tail timings can increase with concurrent painting. Reported callback p95 values remain well below the 1,333 µs block budget for these cases. That does not guarantee every callback or every machine avoids a scheduling stall. Cached knob painting and limited footer repaint reduce repeated drawing. Full offscreen captures include the complete software-rendered scene and image creation, so their millisecond cost is a conservative rendering workload rather than just animation math. Multiple open instances still consume UI resources.

A separate visualizer benchmark uses a preallocated canvas, actual audio-fed history, maximum DAMAGE/CHAOS styling and active particles; it avoids fresh transparent snapshot allocation when measuring drawing itself:

| Work | Median µs | p95 µs |
|---|---:|---:|
| update clear and wave particles max damage | 51.70 | 91.10 |
| ui update | 0.30 | 0.40 |
| direct paint on opaque background | 35.70 | 60.40 |

At 30 Hz, the measured isolated update/clear/paint median of 51.7 µs corresponds to roughly 0.155% of one core. It does not include the rest of the editor. A previous fresh transparent-component screenshot benchmark was dominated by snapshot creation; it is not used as the live visualizer paint measurement. [All open/closed data](Phase7/ui-performance.csv) · [Isolated rendering data](Phase7/visualizer-performance.csv).

**17. Additional memory.** Embedded official PNG: **1,945,835 bytes** (~1.86 MiB of read-only resource data). Retained wordmark/emblem pixel storage is at most **373,248 bytes** (~365 KiB assuming ARGB; RGB can be smaller). The temporary 1254×1254 decode can use up to ~6 MiB while opening an editor and is then released. No giant square bitmap remains in the live editor.

| Fixed store/cache | Bytes | Interpretation |
|---|---:|---|
| processor_bridge | 4,112 | actual sizeof including FIFO and 1024 floats |
| visualizer_object | 5,872 | actual sizeof including Component and preallocated wave and particles |
| particle_pool | 480 | 24 particles with five floats each |
| knob_cache_100_percent | 775,224 | upper bound ARGB cache pixel storage excluding image overhead |
| knob_cache_200_percent | 3,100,896 | upper bound ARGB cache pixel storage at 2x |

The particle store is already included in the visualizer object, not additional to it. Knob cache figures bound ARGB pixel storage; 2× DPI uses four times the pixels. Other JUCE widgets, fonts, image headers, preset strings and host backing surfaces add implementation-dependent overhead. A 900×600 ARGB backing surface would be about 2.06 MiB (2× about 8.24 MiB); those are renderer/host buffers, not a permanent asset allocation. Total OS working-set delta was not isolated. [Memory inventory](Phase7/ui-memory.csv).

## Validation and previews

**18. High-DPI.** Generated real component-tree captures at 100%, 125%, 150%, 200% with dimensions 900×600, 1125×750, 1350×900, 1800×1200. Geometry stays in logical pixels; vector controls/fonts and JUCE's scale-aware cached drawing remain aligned. Header crops have enough resolution for 200% placement. INIT and the 2× maximum-DAMAGE image were visually inspected; the logo is proportional, values retain units and important controls remain readable. Actual Windows monitor-scale changes and mixed-DPI DAW window movement were not exercised and remain a manual integration check.

**19. Lifecycle.** Forty sequential editor creations/destructions with mode/DAMAGE changes and live processing passed. Ten further creations, message-loop updates and snapshot paints ran while a separate audio worker processed continuously. Output remained finite, callback allocations stayed zero, and close disabled the bridge each time. Modal callbacks use SafePointer. No dangling LookAndFeel or retained editor audio subscription was observed.

**20. Attachment/automation.** Every factory load checked all seven slider values, mode button selection, HQ and BYPASS state and exact preset display. Slider changes, all five mode clicks and switch changes wrote the existing APVTS values. Double-click reset defaults, unit formatting and tooltips were verified. Modified user/project identity and all restored slider values were checked after saved-state recall. ParameterAttachment and APVTS attachments retain standard host gestures; no custom normalized parameter path or extra parameter was introduced. Manual recording/undo testing in a real DAW was not performed.

**21. Phase 6 audio regression.** Actual archived Phase 6 VST3 against current Phase 7 VST3: **240 configurations**, five modes × four rates (44.1/48/88.2/96 kHz) × mono/stereo × HQ Off/On × three representative/extreme parameter states. Each processed 4,096 frames; the editor was open in 80 configurations. All parameter state values restored exactly, both builds reported **60 samples**, and every compared output sample matched exactly: **maximum difference 0**. No delay adjustment was needed. [All configurations](Phase7/phase6-audio-regression.csv). Existing earlier audio/state/preset/rate/automation regression suites also passed.

**22. Frozen factory bank and DSP.** SHA256 of `Resources/FactoryPresets.xml`: `4C84B91C0C12674E2C5525F1E3334C7B55DB8F3DFAE0DD225D7F6E6E81BCABD1`. It matches the pre-edit frozen hash. Every file in `Source/DSP` also matches its pre-edit hash in [Frozen-Hashes.json](Phase7/Frozen-Hashes.json). INIT and all 30 preset values, HQ/BYPASS choices, parser/file versions and metadata were left unchanged. Human preset audition remains pending and is not replaced by UI work.

**23. Automated result.** CTest **1/1 passed**, **147,183,987 total assertions**, including **1,479,004 Phase 7 assertions**; final failures **0**. Counts include per-sample assertions rather than that many independent scenarios. Full suite log: `phase7-tests.log`. The final build compiled successfully; remaining C4244 conversion warnings are test numeric conversions, not build errors.

**24–25. Screenshots and 2× preview.** [Contact sheet](Phase7/Editor-Previews.html). All are actual editor captures:

| Preview | File | Pixels |
|---|---|---|
| 01-INIT | [01-INIT.png](Phase7/Screenshots/01-INIT.png) | 900×600 |
| 02-BURN | [02-BURN.png](Phase7/Screenshots/02-BURN.png) | 900×600 |
| 03-BREAK | [03-BREAK.png](Phase7/Screenshots/03-BREAK.png) | 900×600 |
| 04-RIP | [04-RIP.png](Phase7/Screenshots/04-RIP.png) | 900×600 |
| 05-CRUSH | [05-CRUSH.png](Phase7/Screenshots/05-CRUSH.png) | 900×600 |
| 06-CHAOS | [06-CHAOS.png](Phase7/Screenshots/06-CHAOS.png) | 900×600 |
| 07-DAMAGE-100 | [07-DAMAGE-100.png](Phase7/Screenshots/07-DAMAGE-100.png) | 900×600 |
| 08-BYPASS | [08-BYPASS.png](Phase7/Screenshots/08-BYPASS.png) | 900×600 |
| 09-HQ-ON | [09-HQ-ON.png](Phase7/Screenshots/09-HQ-ON.png) | 900×600 |
| 10-USER-MODIFIED | [10-USER-MODIFIED.png](Phase7/Screenshots/10-USER-MODIFIED.png) | 900×600 |
| DAMAGE-100-2x | [DAMAGE-100-2x.png](Phase7/Screenshots/DAMAGE-100-2x.png) | 1800×1200 |
| DPI-100 | [DPI-100.png](Phase7/Screenshots/DPI-100.png) | 900×600 |
| DPI-125 | [DPI-125.png](Phase7/Screenshots/DPI-125.png) | 1125×750 |
| DPI-150 | [DPI-150.png](Phase7/Screenshots/DPI-150.png) | 1350×900 |
| DPI-200 | [DPI-200.png](Phase7/Screenshots/DPI-200.png) | 1800×1200 |

## Build and remaining concerns

**26. Exact current Release VST3.** `C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3`

Inner x64 binary: `C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3\Contents\x86_64-win\DISRUPTION.vst3`

SHA256: `D358EC6A7434F855F90B58C9D84D64109AB51F8811ED5859C06471B7D467DDEA`

Archived Phase 6 baseline SHA256: `155E9CF7EF3220C1FC914D09E65C65319BB2C6C722FC986F319B613D992B1140`.

Product DISRUPTION / manufacturer VoxxLab / codes `VxLb` and `Dsrp` / bundle `com.voxxlab.disruption` / version 0.1.0 are unchanged. `dist/DISRUPTION.vst3` still contains Phase 2 and was not refreshed; inner SHA256 remains `5933B0D5942ED2E50D910F670435C40F544B32C9D49830ED874D23BB881F7699`. No installer, final packaging or installation was performed.

**27. Before release-candidate work.** Final interface implementation and automated validation are complete. Real DAW checks of editor placement, mixed-monitor DPI, automation recording/undo, accessibility tooling, modal dialogs and many simultaneous open instances remain manual integration work. The waveform is intentionally a stylized decimated activity display; it must not be presented as a measurement instrument. Callback timing is locally measured, and OS scheduling tails vary. Factory human listening evaluation is still pending and may tune presets only in the separate authorized audition phase. No audio defect was found or silently fixed. Stop here: no packaging or release work was started.
