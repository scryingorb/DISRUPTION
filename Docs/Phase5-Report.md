# VoxxLab DISRUPTION 0.1.0 — Phase 5 completion

Completed 2026-10-05. HQ enables 4× oversampling for BURN, BREAK, RIP and CHAOS. Intentional digital CRUSH processing remains native-rate. Windows x64 Release VST3 builds and all automated suites pass. Phase 5 stops here: no Phase 6, presets, final UI, installer or distribution packaging.

## 1. Files created or modified

Created `Source/DSP/HQProcessor.h`, `Tests/Phase5Tests.cpp`, `Tools/inspect-phase5.py`, this report, and numerical/spectral/listening artifacts in `Docs/Phase5/`. Archived the pre-change compiled bundle at `build/Phase4Baseline/DISRUPTION.vst3`.

Modified `Source/DSP/DisruptionEngine.h/.cpp`, `Source/PluginProcessor.h/.cpp`, `Source/PluginEditor.cpp`, `CMakeLists.txt`, `Tests/DisruptionTests.cpp`, `Tests/Phase2Tests.cpp`, `Tests/Phase3Tests.cpp`, `Tests/Phase4Tests.cpp` and `README.md`. CMake links JUCE DSP and includes the Phase 5 tests. The temporary editor layout and controls remain; only the phase/HQ footer changes.

Parameter layout, IDs, ranges, defaults, version hints and saved-state format remain unchanged. Product DISRUPTION, manufacturer VoxxLab, code VxLb/Dsrp, bundle `com.voxxlab.disruption`, version 0.1.0 and VST3 identities remain permanent. No Git repository exists here; no commit was made. Re-running the old suites refreshes their generated CSV/WAV artifacts, including current-executable CPU tables, but their historical reports are retained. Temporary orchestration scripts live in `build/`; final build/test logs are `phase5-build.log` and `phase5-tests.log`.

## 2–6. Oversampling architecture, factor, filter and domains

`HQProcessor` owns a native Phase 4 chain and a prepared high-quality chain. HQ OFF runs the native chain and applies a fixed 60-sample output delay. HQ ON for BURN/BREAK/RIP/CHAOS runs the high chain and crossfades its aligned output against the continuing native chain. Keeping the native chain warm preserves smooth transitions and costs one additional native render while HQ is active.

High-quality flow is original input → native-rate smoothed DRIVE → **4× FIR upsampling** → nonlinear engine processing / mode crossfade → FIR downsampling → **native global CRUSH → native TONE → native BODY → MIX with delayed original dry input → OUTPUT → final DC/safety → compensated bypass**.

The up/down filters are explicitly constructed with JUCE 8.0.12 `Oversampling<float>` and two cascaded 2× half-band equiripple FIR stages. Each stereo channel has independent histories. The preparation allocates buffers and filter states outside processing; `initProcessing(1)` prepares a one-host-frame/4-internal-frame pipeline. Processing per frame keeps control scheduling and synchronized digital clocks exact without resizing for host block changes. The wrapper owns two 1024-frame stereo scratch buffers and handles larger host callbacks as preallocated chunks.

| Stage | Up transition width | Up stop target | Up FIR order | Down transition width | Down stop target | Down FIR order | Host-rate group delay |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 (1×→2×) | 0.05 | −90 dB | 118 | 0.06 | −75 dB | 78 | 49 samples |
| 2 (2×→4×) | 0.10 | −80 dB | 50 | 0.12 | −70 dB | 38 | 11 samples |

These are JUCE's normalized half-band transition-width arguments and design attenuation targets, not measured whole-plugin alias suppression. FIR length is order+1. Half-band symmetry gives linear phase. Changing the second down-filter attenuation from the provisional −65 dB to −70 dB changes its order from 34 to 38, making the total delay exactly **60.0 samples** instead of 59.5. Integer-latency support is enabled but its fractional correction is zero; no Thiran correction/all-pass phase warp is applied. Two stages yield 4×, not 2× or 8×. The design favors exact dry/wet phase alignment over a cheaper nonlinear-phase IIR design, with approximately 0.6–1.4 ms latency. 2× was not adopted; the 4× target meets the measured quality goal.

BURN, BREAK, RIP and CHAOS run their unchanged nonlinear algorithms at 4× the host rate. RIP and CHAOS prepare their 5 Hz DC filters at the internal rate and update them once per internal sample. DAMAGE and mode weights retain native-rate 10 ms / 5 ms smoothing and feed four subframes each. DRIVE remains before the upsampler; post controls retain Phase 4 20 ms smoothing.

Dedicated CRUSH has no analog nonlinear section that would benefit from resampling: it remains its native-rate sample-hold/quantizer. During quality/mode crossfades, its native branch is delayed to align with the other high-chain engines and is added after downsampling rather than fed through the FIR. Once CRUSH is the selected mode, the quality-path crossfade moves to the continuing native chain, and the oversampled render stops. HQ therefore preserves CRUSH's entire steady full-stack output exactly and avoids unnecessary CPU. Selecting another nonlinear mode while HQ remains on resumes the prepared high path with a safe fade.

Global CRUSH remains after downsampling at the host rate, preserving intentional quantization/hold aliasing. Its Phase 4 bit/rate maps, stereo timing and amount blend are unchanged. TONE and BODY remain native; their fixed filter regions and the small BODY saturation do not justify extra resampling. MIX, OUTPUT, final protection and bypass are also native. No permanent distortion transfer curve was retuned.

## 7–10. Latency and host reporting

**HQ OFF latency is 60 samples. HQ ON latency is 60 samples**, including CRUSH and bypass. This deliberate fixed-latency decision differs from Phase 4's zero latency, but preserves its HQ-OFF audio values after a pure delay and prevents dynamic PDC changes while switching quality.

| Host rate | HQ OFF / ON latency | Milliseconds |
|---|---:|---:|
| 44.1 kHz | 60 samples | 1.360544 ms |
| 48 kHz | 60 samples | 1.250000 ms |
| 88.2 kHz | 60 samples | 0.680272 ms |
| 96 kHz | 60 samples | 0.625000 ms |

`prepareToPlay` calls `setLatencySamples(60)` after preparation, notifying the host when the latency changes from its initial zero. HQ parameter changes do not change reported latency. A processor listener verifies one latency notification at first preparation and no extra notification during repeated processing/toggles; actual hosted VST3 instances report 60 in both HQ states. `setLatencySamples` is never called inside processBlock, avoiding JUCE's listener-lock/host-notification work in the callback. Buffer/layout/rate reprepare retains the fixed sample count and updates the millisecond duration naturally.

## 11. Dry/wet compensation

The native chain's complete output is delayed by a fixed 60-sample ring, preserving Phase 4 output including its digital clock phases. The high chain delays original clean input by the same 60 samples before MIX/bypass. Its downsampled nonlinear wet path has exactly 60 samples of linear-phase filter delay. Dedicated CRUSH's high-chain transition branch also receives the matching delay.

MIX0 remains the original source, delayed by the reported latency, with no BODY/global CRUSH leak. Fully active stages at Drive36/Damage100/Tone−100/Body100/Crush100 verify exact MIX0 values for all modes, mono/stereo, all rates and both HQ states. MIX25/50/75 full-stack checks compare against independently combined wet output and delayed dry DC response; maximum measured error is **0.000002205** (<1e−4 required). This accounts for the existing final DC filter at nonzero MIX; MIX0 retains the prior fully-dry DC bypass.

Low-level sine tests at 40/250/1000/5000/10000/15000/18000 Hz cover MIX0/25/50/75/100 and both HQ states at all four rates. Measured amplitude spans **-0.0685859 to 0.00309277 dB** relative to a compensated dry reference. Maximum phase error is 7.12527° at 40 Hz, due to the existing final 5 Hz DC blocker; the FIR contributes a pure compensated delay. No latency-induced comb notch is found in the tested band. Existing mode/TONE/BODY phase changes remain intentional.

## 12–13. Switching, reset and bypass

HQ uses a **20 ms sample-by-sample linear crossfade** between aligned complete chain outputs. The native chain always runs. The high chain runs while requested or while fading out; once inactive it consumes no processing CPU. On re-entry, its already allocated filters/histories are cleared with `reset()` and it fades in, masking the cold FIR/DC startup. No topology creation, buffer resize, object allocation or latency change occurs on a toggle. Repeated target changes reverse the current fade smoothly.

Existing mode weights still crossfade over 5 ms. Entering/leaving native-only CRUSH also changes the quality-path target over 20 ms; this is documented additional quality blending, not a change to its native engine transfer/mode-weight smoothing. Filter reset clears histories rather than reconstructing the oversampler. `prepareToPlay` initializes fade state from restored HQ/mode settings; preparation handles sample rate/channel/block changes outside the callback. Repeated reset/reprepare tests remain silent and valid.

Both parameter BYPASS and `processBlockBypassed` use the existing 20 ms engine bypass mechanism, with the clean bypass reference delayed to 60 samples. Thus bypass preserves host-compensated timing and does not jump back to un-delayed input. Settled bypass is exact delayed clean source; dry bypass is not resampled or filtered. Latency remains reported while bypassed. An HQ-only 130 Hz toggle test checks boundary steps <0.03; the larger combined automation test distinguishes deliberate fold/hold waveform edges from control defects.

## 14–17. Objective alias measurements

Drive36/Damage100, input amplitude0.3, stages neutral/off, source frequencies approximately 250/1000/5000/10000/15000 Hz at 44.1/48/88.2/96 kHz. Source frequencies are snapped to integer bins of a **16,384-point FFT**, with actual values recorded in `aliasing.csv`, avoiding window leakage. Each render warms for at least **1.5 seconds** to settle the 5 Hz DC filters, then captures one full coherent FFT period. A provisional shorter warmup was replaced because DC startup decay could be mistaken for alias energy.

The metric sums non-harmonic positive-frequency bin energy, excluding DC, Nyquist and all below-Nyquist integer multiples of the actual source bin. Converted non-harmonic RMS is compared HQ OFF vs ON; total RMS and alias/total ratios are recorded separately. Aliases landing exactly on a legitimate harmonic bin cannot be separated with this test, so this is a defined non-harmonic foldback measurement rather than a claim to count all alias energy. It does not equate reduced total high-frequency energy with improved alias rejection.

For each nonlinear mode, all **20/20** measured cases improve. The table gives mean/min/max dB reductions in measured non-harmonic RMS:

| Mode | Mean reduction | Minimum | Maximum | Improved cases |
|---|---:|---:|---:|---:|
| BURN | 16.5069 dB | 8.2313 dB | 32.6521 dB | 20/20 |
| BREAK | 18.0096 dB | 11.1701 dB | 28.0935 dB | 20/20 |
| RIP | 17.0817 dB | 11.4425 dB | 24.1884 dB | 20/20 |
| CRUSH | 0.0000 dB | 0.0000 dB | 0.0000 dB | 0/20 |
| CHAOS | 10.4570 dB | 2.7215 dB | 23.6646 dB | 20/20 |

BURN retains the warm transfer function; BREAK retains its aggressive clipping; RIP retains asymmetric even/odd shaping and rate-correct DC filtering; CHAOS retains folding/asymmetry/harmonic density. Four-times processing reduces their unwanted folded components instead of changing the curves. CHAOS is the hardest case: its weakest measured improvement is approximately 2.72 dB at 15 kHz/48 kHz, and its mean is 10.46 dB. Residual aliasing remains because finite 4× sampling cannot remove every very high-order fold harmonic.

`rip-dc-response.csv` measures the 5 Hz low-level response at every rate with HQ OFF/ON. RIP's own 5 Hz blocker plus the existing final 5 Hz blocker give about −5.04 dB including its 1.12 small-signal gain. Measured responses remain within 0.08 dB of this expectation; the plateau DC test also settles below 1e−4. No DC cutoff change is introduced by preparing RIP at 4×.

## 18–19. Intentional CRUSH behavior

CRUSH's dedicated 16→3-bit / host→600 Hz mapping and global CRUSH's independent Phase 4 16→5-bit / host→2200 Hz map are unchanged and remain clocked once per native frame with synchronized stereo timing and independent held values. All 20 CRUSH spectra match HQ OFF/ON; full-stack CRUSH comparisons with active TONE/BODY/global CRUSH also match exactly. At commensurate sine/hold frequencies, CRUSH may capture only zero samples and become silent; this intentional deterministic clock/grid behavior is identical in both states, not a quality-mode failure.

In the other modes, global CRUSH follows the cleaner downsampled nonlinear output but still performs native hold/quantization. HQ does not filter its newly created digital steps. Phase 4 global-clock/rate/quantization tests remain, and extreme interaction/listening cases exercise it under HQ. Its input waveform can change because upstream aliasing changes, but its algorithm/rate/bit-depth/amount are not weakened.

## 20–21. Levels and compensation

The common musical source is the established ten-second synthetic vocal/bass/percussion/saw/chord sequence. HQ changes its raw RMS by at most **0.0863 dB** across engine-only and full-stack settings. Raw peak changes are larger because reconstructed/filtered nonlinear waves can overshoot between native samples; no limiter was introduced.

| Mode | Source | HQ RMS change | HQ peak change |
|---|---|---:|---:|
| BURN | Engine | -0.0028 dB | +1.1709 dB |
| BURN | Full stack | -0.0023 dB | +0.0125 dB |
| BREAK | Engine | -0.0030 dB | +1.6032 dB |
| BREAK | Full stack | -0.0031 dB | -0.0030 dB |
| RIP | Engine | -0.0028 dB | +0.6937 dB |
| RIP | Full stack | -0.0026 dB | +0.5264 dB |
| CRUSH | Engine | +0.0000 dB | +0.0000 dB |
| CRUSH | Full stack | +0.0000 dB | +0.0000 dB |
| CHAOS | Engine | -0.0863 dB | +1.1392 dB |
| CHAOS | Full stack | -0.0071 dB | -0.0025 dB |

Engine-only settings are Drive24/Damage90 with post stages off. Full stack uses Drive18/Damage75, BODY60, TONE−35 except RIP+35; global CRUSH20 for BURN, 55 for RIP, 35 for BREAK/CRUSH/CHAOS. Raw RMS/peak values and offline matching gains are in `listening-levels.csv`.

Aggressive high-frequency sine RMS can drop substantially when much of the native output consisted of folded harmonics. The largest measured spectral-source RMS change is **-6.7766 dB** for CHAOS, approximately 15000 Hz at 88200 Hz host rate. This is explicitly different from the common musical-source result and is not hidden by gain normalization.

**No runtime gain compensation** was added: usual musical-source RMS remains close, and raising HQ gain to restore removed alias energy would alter distortion balance and mislead comparisons. Listening raw files retain real levels; RMS-matched files apply only offline RMS0.1 gain with identical 10 ms edge fades. The peak in the 0 dB OUTPUT extreme matrix is **4.14789**; positive OUTPUT gain still intentionally raises this. No brick-wall limiter, auto loudness system or new output rail was added.

## 22. Phase 4 HQ-OFF regression and old tests

The actual pre-change Phase 4 DLL is archived in `build/Phase4Baseline/DISRUPTION.vst3` (SHA256 `25DB1EC953021662B79B67F3BD0B3667255F7A3F5FD29665D63A26DDE65E3FE9`). Both real VST3s are loaded side by side. Tests cover all modes × four rates × mono/stereo × six settings (**240 cases**, 8192 frames each), including neutral stages, active TONE/BODY/global CRUSH, partial/full/dry MIX and extreme DRIVE/DAMAGE. Old saved state is obtained from the archive and restored into the new DLL; all ten parameter values match. Comparing the new HQ-OFF output with Phase 4 delayed by exactly 60 samples gives **maximum sample difference 0** (threshold <1e−6).

Existing Phase 3 neutral comparison (360 cases) and Phase 2 original-three comparison are likewise delay-aligned and still have zero sample difference. All earlier suites remain. Old zero-latency assertions now require the intentional nonzero compensated latency; impulse expectations index the reported delay; archived host comparisons delay the reference. HQ=true in old Phase 1 fixtures is explicitly disabled after verifying its saved value for engine-only legacy tests. The obsolete HQ-placeholder null check is replaced by a meaningful positive check: HQ changes each analog nonlinear mode and preserves CRUSH exactly. Its per-sample finite checks remain. No parameter/state compatibility checks were removed to pass the suite.

## 23–25. Channels, blocks, rates and lifecycle

44.1/48/88.2/96 kHz pass. HQ nonlinear rates are 176.4/192/352.8/384 kHz; digital/post stages remain at their host rates. Mono and stereo pass, with no reported layout change and no crosstalk. Each stereo channel equals its independently rendered mono counterpart for active full-stack settings and both quality states, including opposing signals and asymmetric engines.

All required host sizes **32/64/128/256/512/1024** are tested at every rate in mono/stereo, all modes and both extreme TONE directions. This matrix has 48 rate/block/layout rows and 110 signal/mode/tone cases per row (**5280 cases**), each processed for four callbacks. It includes silence, near-silence, fixed sine, square, saw, impulse, DC, bass/vocal-like signals, sweep, full scale and 1000× overload. All remain finite and bounded under the tested 0 dB OUTPUT conditions.

Additional streaming checks compare 32-frame chunks against a 4096-frame callback, proving block-partition invariance and handling larger callbacks without resizing. Reprepare/reset tests reuse the same prepared class over rates/layout-compatible buffers. The actual hosted DLL receives repeated HQ changes at all required rates/layouts. `rate-block-results.csv` records case counts/peaks/latency; `phase4-regression.csv` records actual-host comparisons.

## 26. Automation

All ten controls are exercised: DRIVE, DAMAGE, MODE, TONE, BODY, global CRUSH, MIX, OUTPUT, HQ and BYPASS. Combined tests cover every mode/rate and rapidly toggle HQ during mode/DAMAGE sweeps, partial MIX and output/bypass changes. No nonfinite output, crash, corrupted state, giant runaway impulse or tracked allocation occurs. Largest measured peak **2.81475**, largest raw adjacent step **2.00156**, largest block-boundary step **0.378524**. These combined steps include intentional fold/hold transitions and +12 dB OUTPUT; isolated 130 Hz HQ toggles separately pass <0.03 boundary-step checks. The existing parameter and mode automation suites also pass.

HQ toggles use no new topology construction, no memory resizing and no host-latency updates inside the callback. Fixed FIR state clearing and the fade provide the safe transition. Steady CRUSH remains native even with the HQ parameter on.

## 27–28. Tests and real-time safety

Final Release CTest: **133,063,290 assertions passed, 0 failed**, 1/1 aggregate executable, 56.85 seconds. Phase 1: 311,653; Phase 2: 8,178,577; Phase 3: 10,842,191; Phase 4: 97,653,864; Phase 5: 16,077,005. Counts include many per-sample assertions and are not independent scenario counts.

**Tracked callback new/new[] allocations remain 0**, including prepared 4× processing, repeated toggles, large buffers, mono/stereo, automation, matrices and listening generation. Existing ordinary new/new[] instrumentation covers the native executable, not every allocator API or each loaded DLL CRT; this is a precise tracked-allocation claim rather than a system-wide allocator audit.

The new processing uses prepared buffers, fixed arrays/rings, scalar smoothing and non-owning stack views. Oversampler construction, FIR design, owned buffer sizing and initialization happen in prepare, never in processBlock. Reset clears existing state. There are no mutex waits, filesystem operations, logging, dynamic topology construction or throwing operations in the new processing. CSV/WAV/FFT artifact work is outside tracked callbacks. Existing finite protection and ScopedNoDenormals are retained. Latency notifications occur only during preparation.

## 29–31. CPU and Phase 4 comparison

Primary comparison loads the archived Phase 4 and new Phase 5 DLLs in the same host executable, with the same stereo 64-frame input and Drive18/Damage75/TONE50/BODY60/global CRUSH60. For each version/rate/mode, 200 callbacks warm up and 1000 are timed. Median and p95 are wall-clock callback times; core use is median divided by the available block duration. Controls settle; these numbers do not bound heavy continuous coefficient automation.

### 48 kHz

| Mode | Archived Phase 4 median | HQ OFF median / p95 | HQ ON median / p95 | HQ ON core use |
|---|---:|---:|---:|---:|
| BURN | 22.2 µs | 22.7 / 24.5 µs | 83.1 / 90.1 µs | 6.2325% |
| BREAK | 20.7 µs | 21.3 / 22.6 µs | 76.6 / 150.8 µs | 5.745% |
| RIP | 20.1 µs | 20.7 / 39 µs | 71.5 / 81.3 µs | 5.3625% |
| CRUSH | 19.8 µs | 20.4 / 38.8 µs | 20.3 / 21 µs | 1.5225% |
| CHAOS | 20.2 µs | 20.7 / 21.4 µs | 71.5 / 76 µs | 5.3625% |

### 96 kHz

| Mode | Archived Phase 4 median | HQ OFF median / p95 | HQ ON median / p95 | HQ ON core use |
|---|---:|---:|---:|---:|
| BURN | 21.1 µs | 21.8 / 22.8 µs | 81.9 / 93.7 µs | 12.285% |
| BREAK | 20 µs | 20.9 / 46.3 µs | 75.4 / 91.3 µs | 11.31% |
| RIP | 19.2 µs | 19.8 / 20.7 µs | 70 / 75.7 µs | 10.5% |
| CRUSH | 19.1 µs | 19.6 / 20.5 µs | 19.6 / 20.4 µs | 2.94% |
| CHAOS | 19.4 µs | 20.1 / 38.7 µs | 70.3 / 102.2 µs | 10.545% |

`archive-cpu-comparison.csv` includes the archived Phase 4 p95 values too. `cpu-benchmark.csv` separately times the native prepared HQProcessor class with allocation tracking enabled. Normal-path cost is close to Phase 4, with a small copy/delay/fade overhead. CRUSH has the same native cost with HQ OFF/ON. The four oversampled modes typically cost roughly 3–4× the fully active normal chain.

The 96 kHz HQ path is **meaningfully expensive**, approximately 10–12% of one core for 64-frame stereo blocks in this local benchmark, highest for BURN. Multiple instances can consume a substantial core budget. It includes the continuing native renderer, 4× warm nonlinear engines and linear-phase FIR processing. This is flagged for later profiling; normal mode remains the low-cost option. No 8×/16× factor or speculative micro-optimization was added. FIR quality/alignment and safe switching were retained. Windows scheduling affects p95/outliers and results are not a guaranteed DAW performance budget.

## 32. Spectral artifacts

[HQ spectral and listening inspector](Phase5/HQ-Spectral-Inspector.html), generated by `python Tools/inspect-phase5.py`. It displays BURN/BREAK/RIP/CRUSH/CHAOS HQ OFF/ON spectra at approximately 5/10/15 kHz, host48 kHz, and offers engine/full-stack listening pairs. Full numerical alias data cover all five requested frequencies at all four rates.

- `aliasing.csv`: coherent input frequency, non-harmonic RMS, total RMS, peak and alias/total ratio.
- `spectra.csv`: every positive FFT bin for the plotted 48 kHz cases; no bin thinning in the source artifact.
- The HTML groups eight bins and keeps the strongest bin, retaining narrow peaks instead of accidentally discarding coherent harmonic/alias spikes.
- `filter-configuration.csv`, `rip-dc-response.csv`, `mix-timing.csv`, `full-stack-mix.csv`, `rate-block-results.csv`, `automation.csv`, `phase4-regression.csv`, `cpu-benchmark.csv`, `archive-cpu-comparison.csv`, `listening-levels.csv`.

This inspector is developer-only and has no plugin runtime/UI integration or external dependency. Its JavaScript passes Node syntax checking. Browser interaction/audio decoding has not been manually auditioned or visually inspected; no subjective listening approval is claimed. Editor screenshot: `Phase5/editor-preview.png`.

## 33. Listening files

**50 float32 stereo WAVs** at 48 kHz in `Docs/Phase5/`:

For each of BURN/BREAK/RIP/CRUSH/CHAOS, and each of `Engine` / `Stack`:

- `{MODE}-{Engine|Stack}-HQ-OFF-Raw.wav` and `...-HQ-ON-Raw.wav`: real level, ten seconds each.
- `{MODE}-{Engine|Stack}-HQ-OFF-RMS-matched.wav` and `...-HQ-ON-RMS-matched.wav`: offline RMS0.1 gain, ten seconds each.
- `{MODE}-{Engine|Stack}-OFF-then-ON.wav`: twenty-second matched comparison, OFF then ON.

For example: [BURN engine comparison](Phase5/BURN-Engine-OFF-then-ON.wav), [CHAOS engine comparison](Phase5/CHAOS-Engine-OFF-then-ON.wav), [RIP full-stack comparison](Phase5/RIP-Stack-OFF-then-ON.wav), [CRUSH full-stack comparison](Phase5/CRUSH-Stack-OFF-then-ON.wav).

All derive from the same established synthetic musical source. Both HQ states include the same reported 60-sample delay, and identical offline fades prevent file-edge clicks. Export verification reads back float format, exact frame counts and preserved peaks; raw >0 dBFS peaks are not silently clipped. Per-file RMS, peak, parameter values and matching gain are in `listening-levels.csv`. No plugin loudness normalization is applied.

## 34. Exact current build

Windows x64 Release VST3, JUCE8.0.12, MSVC19.29 / Visual Studio2019 BuildTools, version0.1.0:

`C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3`

Inner binary `Contents/x86_64-win/DISRUPTION.vst3`, SHA256:

`BBAAAC2B58C9C5C9F9F9B40F08778E531DEF59BF5707E0E47D5A9D5B538530D6`

**dist still contains Phase 2**, SHA256 `5933B0D5942ED2E50D910F670435C40F544B32C9D49830ED874D23BB881F7699`; it was not refreshed or reported as Phase 5. `build.ps1` remains unchanged and was not used because it includes the dist-copy step. No installer, release packaging or production publication was performed.

```powershell
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release -V
python Tools/inspect-phase5.py
```

Full paths to the existing CMake/CTest tools were used; the existing JUCE checkout is cached locally, with no dependency download. Build/test logs are in the project root. Neutral archived DLLs must remain in their baseline folders for regression tests.

## 35. Concerns before Phase 6

HQ achieves measurable improvements while keeping common musical-source RMS close and preserving digital CRUSH exactly. The fixed 60-sample latency is also present with HQ OFF and bypass; this is an intentional compatibility timing change that hosts must compensate. HQ at 96 kHz can use approximately 10–12% of one core per active analog-mode instance in the measured 64-frame configuration. FIR ringing can increase peaks even when RMS stays close; OUTPUT headroom still matters. Very aggressive high-frequency CHAOS sine cases can lose substantial RMS as alias energy is removed, and 4× still leaves residual foldback. Human listening on real sessions remains the next evaluation step; generated files and objective metrics do not replace it.

No gain compensation, final UI, presets, installer or distribution update was added. Phase 6 has not started.
