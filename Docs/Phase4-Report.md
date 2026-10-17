# DISRUPTION 0.1.0 — Phase 4 completion

Completed 2026-10-05. TONE, BODY and global CRUSH are implemented; Windows x64 Release VST3 built. Phase 4 stops here. HQ remains inactive. No presets, final UI, installer or release packaging were introduced.

## 1. Files

New: `Source/DSP/ParallelFilter.h`, `Source/DSP/DigitalQuantizer.h`, `Tests/Phase4Tests.cpp`, `Tools/inspect-phase4.py`, this report and the artifacts in `Docs/Phase4/`.

Modified: `Source/DSP/ToneProcessor.h/.cpp`, `BodyProcessor.h/.cpp`, `CrushProcessor.h/.cpp`, `CrushEngine.cpp`, `DisruptionEngine.h/.cpp`, `Source/PluginEditor.cpp`, `Tests/DisruptionTests.cpp`, `Tests/Phase2Tests.cpp`, `Tests/Phase3Tests.cpp`, `CMakeLists.txt`, `README.md`.

The previous test suites regenerate their Phase 2/3 numerical and WAV artifacts; their historical reports remain unchanged. Their current CPU CSVs describe the current executable, so the Phase 4 CPU comparison instead loads the actual archived Phase 3 DLL. Development logs and temporary scripts live in `build/` and the project root. No Git repository exists here; no commit was made.

The processor parameter/state implementation was left unchanged. VoxxLab, VxLb, Dsrp, `com.voxxlab.disruption`, version 0.1.0, parameter IDs/ranges/defaults/version hints, VST3 identity and state format are preserved. The fixed temporary 840×460 editor retains its layout; its footer now identifies the functional stages and reserved HQ.

## 2–4. TONE design, frequency and gain

A prewarped bilinear first-order low-pass splits the input into complementary low and high paths at **1200 Hz**. For filter state z, g=tan(pi·1200/Fs), a=g/(1+g), v=a·(x−z), low=v+z, z=low+v. High=x−low. Histories are double precision and independent per channel.

For TONE percent t, lowGain=10^(−0.003t), highGain=10^(0.003t). Output is x+(lowGain−1)·low+(highGain−1)·high. This is a continuous shelving tilt with both spectral regions always present. It is not an LP/HP selector. At zero the exact input is returned while the split history continues updating.

1200 Hz separates warmth/fundamental regions from presence and edge across voices, guitars and synths without turning the knob into a sub-bass control. This is listening-oriented design reasoning; no claim of human audition or user listening approval is made.

Maximum asymptotic shelving gain is **+6/−6 dB**, a **12 dB low-to-high contrast**. Small-signal measurements over 10–18000 Hz across all four rates span −5.99555 to +5.99972 dB. The extreme tilt has approximately +3.26 dB at the 1200 Hz split, because the complementary components have phase; this is not a constant-gain pivot. At the center both gains are exactly one. No loudness normalization or limiter is applied.

## 5–8. BODY architecture, frequencies, saturation and phase

BODY adds a parallel path derived from the signal **after global CRUSH and TONE**:

x → first-order high-pass at **20 Hz** (x minus its low-pass) → first-order low-pass at **350 Hz** → 1.5·tanh(band/1.5) → BODY/100 gain → add to x.

Both filters use the same bilinear structure, prewarped at their own frequencies. This broad region covers 808 fundamentals, bass/kick weight, vocal lower harmonics and guitar/snare body. The 20 Hz rejection avoids steady DC/subsonic accumulation. BODY is level-dependent parallel reinforcement, not a static low shelf.

The saturation has unity slope at zero, compresses the added component as level rises and bounds it to ±1.5 at BODY=100. There is no envelope follower, makeup gain, compressor pumping or extra engine. At BODY 25/50/75/100 the branch gains are 0.25/0.50/0.75/1.00. BODY=0 returns the exact input; histories remain warm.

Phase management: in the corresponding bilinear analog variable, the unsaturated branch has H(s)=B·s/((s+A)(s+B)) for positive prewarped A/B. Its real part at imaginary s is nonnegative. Consequently the linear parallel sum 1+amount·H does not create a cancellation notch. The branch peaks near sqrt(20·350), approximately 84 Hz, and spans low mids instead of forming a narrow resonance. Small-signal BODY sine measurements at twelve frequencies from 10–18000 Hz, five amounts, four rates showed no attenuation below the neutral reference; maximum gain **5.78134 dB**. Additional amplitude sweeps (.001/.1/1/10/1000 at 80 Hz) verify the saturation contribution bound, and a ten-second DC test verifies unity steady DC output from this stage. Full-scale/overload/sweep tests exercise the entire nonlinear path.

All filters are causal without buffers, lookahead, oversampling or host-reported latency. A causal frequency-dependent phase shift is present; zero reported latency does not mean zero phase shift. Saturation produces modest extra harmonics and is intentionally distinct from the linear TONE control.

## 9–13. Global CRUSH architecture and mapping

One host-rate-aware fractional phase accumulator is advanced once per frame before either channel. It captures the two channel values independently at synchronized instants. Fractional phase continues across parameter changes; it is never reset by automation. First frame captures immediately. No randomness, channel averaging or stereo collapse is used.

Let c=clamp(CRUSH/100,0,1):

- Effective bits b=16−11·c^0.85, ending at 5 bits.
- Requested capture rate=96000·exp(log(2200/96000)·c^1.7) Hz.
- Actual capture rate=min(host rate,requested rate).
- n=floor(b), f=b−n; lower grid levels=2^(n−1)−0.5; upper=2^n−0.5 unless n=16.
- Quantize held/2 on both adjacent integer grids, interpolate their results with f, then multiply by 2.
- Blend original x and degraded q as x·(1−c)+q·c.

Thus low values introduce only a small amount of fine quantization, middle values add increasing grit and mild hold reduction, and high values become a fully held coarse digital layer. At zero an exact input return bypasses quantization and any rail while retaining warm clock/input histories. The nominal ±2 ADC input headroom protects post-engine peaks; rounding on the half-level grid can reach ±2.06452 at the coarsest setting. This is part of the digital stage, not a limiter on the plugin output.

| CRUSH % | Bits | Requested Hz | Actual Hz at 48 kHz |
|---|---:|---:|---:|
| 0 | 16 | 96000.00 | 48000 |
| 25 | 12.6144 | 67131.23 | 48000 |
| 50 | 9.89737 | 30029.54 | 30029.5 |
| 75 | 7.3862 | 9477.89 | 9477.89 |
| 100 | 5 | 2200.00 | 2200 |

`DigitalQuantizer.h` shares the proven adjacent-grid rounding/interpolation operation with Phase 3 CRUSH. The dedicated engine keeps its original coefficients, ±1 ADC range and 16→3-bit, host→600 Hz mapping. Only the common quantizer operation was factored; its arithmetic remains equivalent and the archived DLL comparison confirms unchanged outputs. The small phase accumulator is independently maintained for each stage.

Global CRUSH instead uses ±2 headroom, half-level grids, a 16→5-bit mapping, 2200 Hz endpoint and an amount blend. Its endpoint clock is not a multiple of the dedicated 600 Hz endpoint, and its grid is not aligned with the dedicated power-of-two grid. This ensures that CRUSH mode + global CRUSH changes capture timing and values rather than merely applying another identical quantizer. Tests at dedicated DAMAGE 25/50/100 verify nonzero differences and a nonzero residual after fitting the best scalar gain. The combination remains deterministic, finite and stable; deliberate quantization/hold discontinuities remain audible.

## 14. Smoothing and signal flow

TONE, BODY and global CRUSH each receive a **20 ms linear macro smoother** initialized directly to restored/current parameter values in prepare. TONE feed-forward gains are recomputed from the smoothed value; its feedback coefficients/poles stay fixed. BODY blend is smoothed. Global CRUSH wet blend, effective bit grids and capture-rate increments derive from its smoothed macro; adjacent grids interpolate continuously, and there is no phase reset. Coefficient calculation is cached when controls settle. Deliberate sampled-audio steps are not blurred by an output smoothing filter.

Existing DAMAGE 10 ms, mode crossfade 5 ms, and DRIVE/MIX/OUTPUT/BYPASS 20 ms remain unchanged. HQ remains preserved and inactive.

Flow remains input → DRIVE → selected/crossfaded engine → global CRUSH → TONE → BODY → MIX against original input → OUTPUT → existing final DC/safety protection → smoothed bypass. Zero/off stages return exact inputs while maintaining independent warm histories.

## 15. Level management

`interaction-levels.csv` records all **2640** cases: five modes, four rates, mono/stereo, six stage combinations and eleven signal types. Worst observed output peak at 0 dB OUTPUT is **4.14789** (approximately +12.36 dBFS), including overload, square/saw and extreme Drive36/Damage100/Tone±100/Body100/Crush100. This is not a clipping or limiter threshold.

`level-increases.csv` compares identical reference inputs/engine settings with new stages disabled versus enabled. It includes fixed sine, bass-heavy and vocal-like signals, all modes, useful settings and both extreme TONE directions. Ordinary rows use Drive12/Damage65; rows with Tone±100/Body100/Crush100 use Drive36/Damage100 for both reference and processed paths. Worst measured **RMS increase is 10.533 dB**, and worst **peak increase is 10.9846 dB**. The extreme RMS case is RIP with bass-heavy input and dark TONE; the peak-increase case is BURN with bass-heavy input and dark TONE.

On the common ten-second musical source, dark TONE100 adds approximately 5.93 dB RMS and BODY100 approximately 5.31 dB RMS separately. Global CRUSH100 preserves roughly the reference RMS while changing texture and peak timing. No automatic gain compensation or brick-wall limiter was added. Users can reduce OUTPUT when using maximum dark tilt and BODY together; OUTPUT+12 intentionally adds another 12 dB. Floating WAV exports preserve peaks above full scale.

## 16. Archived Phase 3 regression and state

The pre-change compiled bundle was copied to `build/Phase3Baseline/DISRUPTION.vst3` before edits. The test loads both real VST3 DLLs and compares all five modes × four rates × mono/stereo × DRIVE0/18/36 × DAMAGE0/50/100: **360 cases**, 8192 frames each. TONE/BODY/CRUSH are explicitly zero. **Maximum sample difference: 0** (required tolerance <1e−6). Original BURN/BREAK/RIP also match the archived Phase 2 DLL exactly in their existing comparison.

Existing Phase 1 and Phase 2 binary session fixtures restore all ten parameter values unchanged, including nonneutral formerly reserved controls. Those stored controls now intentionally affect sound when enabled; this phase guarantees Phase 3 sonic compatibility with the new stages neutral, not with previously unused nonzero values. Host state, mono/stereo VST3 loading and identity checks remain in the original suites.

Old placeholder-null assertions were technically obsolete. The Phase 3 null test now checks only reserved HQ; Phase 4 tests separately verify meaningful control differences. Engine-only Phase 1/2/3 checks explicitly zero the new stages after parameter/state torture. In the Phase 2 baseline comparison, normalized TONE is explicitly set to 0.5 because normalized zero means −100%. No previous suite was removed.

## 17. Automation

All four rates and five modes receive rapid simultaneous automation of TONE/BODY/global CRUSH plus DRIVE/DAMAGE/MODE/MIX/OUTPUT/BYPASS. Full-stack output stayed finite and bounded. Largest observed full-stack adjacent step was **3.68915**, during aggressive CHAOS/drive/output automation; largest block-boundary step **0.460962**. Those raw jumps include deliberate fold/hold behavior and cannot by themselves identify a control click.

A separate changed-versus-unchanged render isolates first-sample and residual-step effects for each new control, both directions, all modes and rates. BODY uses a continuous 130 Hz sine; TONE/global CRUSH use a settled constant input so their control residual is separated from fold edges. Worst first residual **0.000786543**, worst residual step **0.00738572**; all pass <0.005/<0.01 thresholds respectively. Existing mode-transition and old-control automation tests also pass unchanged. These checks verify the tested conditions, not arbitrary musical automation at every possible host scheduling interval.

## 18–19. Sample rates, mono/stereo and MIX

44.1/48/88.2/96 kHz pass filter response, hold-rate counts, interaction and automation tests. Capture counts over one second differ by at most one event from the requested effective physical rate. At global100 the rate is 2200 Hz at every tested host rate. At small amounts, rate reduction is clamped to the host rate. Prewarping preserves TONE/BODY corner frequencies at the tested rates.

Mono and stereo pass with zero reported latency. Each stereo channel equals its independently rendered mono counterpart for active full-stack settings in every mode. Opposite polarity is explicitly tested where engine symmetry applies; intentionally asymmetric engines are checked through the mono comparisons instead of demanding antisymmetry. No crosstalk or summation is introduced.

MIX0 is exactly the original source (with OUTPUT0) even with all new stages maximized. Independent construction of the first wet impulse, including engine/global CRUSH/TONE/BODY, verifies the complete wet-chain blend at MIX0/25/50/100 for all five modes. Existing DC-endpoint handling and smoothed bypass behavior remain unchanged.

## 20–21. Checks and real-time safety

Final Release CTest: **116,986,280 assertions passed; 0 failed**. Phase 1: 311,653; Phase 2: 8,178,577; Phase 3: 10,842,186; Phase 4: 97,653,864. CTest runs one aggregate executable and passes 1/1 in 19.71 seconds. Large counts include per-sample response/interaction/compatibility assertions; they are not independent test scenarios.

**Tracked callback new/new[] allocations: 0**, including native active-stage matrix, automation and listening rendering. The existing ordinary new/new[] instrumentation covers the native test executable; it does not intercept every DLL CRT allocation or all malloc/aligned allocation APIs. No allocation is required by the new algorithms: fixed two-channel arrays, scalar state, stack locals and coefficient arithmetic only. No file access, logging, locks, throwing operations or dynamic construction appear in their audio processing paths. All file/CSV/WAV operations are outside tracked callbacks. Existing finite-output protection, ScopedNoDenormals and small-state flushing remain; reusable filter state flushes below 1e−24.

Signals include silence, near-silence, fixed sines, chirp sweep, square, saw, impulse, DC, opposing stereo, bass/vocal-like synths, full scale and 1000× deliberate overload. The earlier suites retain their NaN/infinite/maximum-float safety checks and state sanitation checks.

## 22. CPU

Actual archived Phase 3 and new DLLs are timed in the same executable at 48 kHz, stereo, 64 frames. Each configuration warms for 200 blocks then measures 1000 blocks; DRIVE18/DAMAGE75, TONE50/BODY60/CRUSH60 where active. Median microseconds per callback:

| Mode | Phase 3 | Neutral | TONE | BODY | Global CRUSH | All stages | All / core |
|---|---:|---:|---:|---:|---:|---:|---:|
| BURN | 13.7 | 14.9 | 15.3 | 17.4 | 19.6 | 22.1 | 1.6575% |
| BREAK | 12.7 | 13.4 | 14 | 15.9 | 17.7 | 20.7 | 1.5525% |
| RIP | 11.8 | 12.6 | 13.1 | 15.1 | 17.9 | 20.4 | 1.53% |
| CRUSH | 11.9 | 12.8 | 13.3 | 15.1 | 17.1 | 19.9 | 1.4925% |
| CHAOS | 11.9 | 12.7 | 13.3 | 15.2 | 18.2 | 20.5 | 1.5375% |

`cpu-benchmark.csv` also records p95 values. Relative to new neutral processing, TONE adds roughly 0.4–0.6 µs, BODY 2.3–2.7 µs, and global CRUSH 4.3–5.4 µs. Warm inactive filters/clock add roughly 0.6–1.2 µs compared with Phase 3. All active stages add approximately 8–9 µs vs Phase 3, a substantial relative increase but a low absolute cost (about 1.5–1.7% of one core). Global CRUSH is the largest contributor; flag this for later profiling before adding HQ. This benchmark measures settled controls, not continuous coefficient recomputation under automation. Windows scheduling and timer noise affect p95/outliers; these local numbers are not a guaranteed DAW budget. No premature micro-optimization was applied.

## 23. Inspector

[Offline DSP inspector](Phase4/DSP-Inspector.html), generated by `python Tools/inspect-phase4.py`. It displays measured TONE/BODY responses at the requested five settings and global CRUSH hold traces at 0/25/50/75/100. Frequency/phase data: `frequency-response.csv`; hold data: `global-crush-hold.csv`; rates/bits/counts: `global-crush-mapping.csv`. The inspector is developer-only HTML with embedded data, no external dependencies and no production UI integration. JavaScript syntax is checked with Node; browser interaction has not been manually inspected.

Other numerical artifacts: `interaction-levels.csv`, `level-increases.csv`, `automation.csv`, `stage-transition-residual.csv`, `cpu-benchmark.csv`, `listening-levels.csv`. Editor screenshot: `Phase4/editor-preview.png`.

## 24. Listening renders

**38 WAV files** in `Docs/Phase4/`, each 48 kHz stereo, 32-bit floating point, ten seconds. The source is the same Phase 2 synthetic vocal/bass/percussion/saw/chord sequence; no external source recordings or subjective listening claims are used. Each export is read back to verify float format, frame count and preserved peak. Raw files retain real DSP levels; `RMS-matched` files apply offline gain to RMS 0.1 after identical 10 ms edge fades, with no normalization in the plugin.

- `Tone--100`, `Tone--50`, `Tone-0`, `Tone-50`, `Tone-100`: BURN Drive12/Damage65, remaining stages off.
- `Body-0`, `Body-25`, `Body-50`, `Body-75`, `Body-100`: same engine, TONE/global CRUSH off.
- `GlobalCrush-0`, `GlobalCrush-25`, `GlobalCrush-50`, `GlobalCrush-75`, `GlobalCrush-100`: same engine, TONE/BODY off.
- `Stack-0`: BURN, BODY60, global CRUSH20, TONE0.
- `Stack-1`: BREAK, TONE−35, BODY60, global CRUSH0.
- `Stack-2`: RIP, TONE35, BODY0, global CRUSH55.
- `Stack-4`: CHAOS, TONE−35, BODY60, global CRUSH0.

Every listed stem has `-Raw.wav` and `-RMS-matched.wav` variants. All full-stack files use Drive12/Damage65. Levels and offline matching gain are recorded in `listening-levels.csv`.

## 25. Exact build

Windows x64 Release VST3, JUCE8.0.12, MSVC19.29 / Visual Studio2019 BuildTools, version0.1.0:

`C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3`

Inner binary: `Contents/x86_64-win/DISRUPTION.vst3`.

SHA256: `25DB1EC953021662B79B67F3BD0B3667255F7A3F5FD29665D63A26DDE65E3FE9`.

The archived Phase 3 inner DLL SHA256 is `C1C0B055355869581CD115F0967335C6188F54DB6254B9D45BA265F1054FC13F`.

**dist remains Phase 2**, SHA256 `5933B0D5942ED2E50D910F670435C40F544B32C9D49830ED874D23BB881F7699`. It was neither refreshed nor presented as Phase 4. `build.ps1` was not invoked because it includes the packaging-copy step. Build/test commands used existing cached JUCE and CMake, with no dependency downloads:

```powershell
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release -V
python Tools/inspect-phase4.py
```

Full tool paths were used because CMake is not on PATH. Build/test logs: `phase4-build.log`, `phase4-tests.log`.

## 26. Before Phase 5

The new stages are usable and bounded under the measured conditions. Extreme dark TONE + BODY can add about 10.5 dB RMS and needs OUTPUT headroom. Continuous global CRUSH automation recomputes nonlinear mappings at sample cadence; its automation CPU cost deserves profiling together with eventual HQ. Deliberate digital aliasing and fold harmonics remain; oversampling was not added and cannot eliminate intentional hold/quantization character. BODY is intentionally level dependent and should be auditioned on real sessions before final preset decisions. Formerly reserved nonzero state values now become audible as expected. Human listening evaluation, final graphics, oversampling, presets and release packaging remain future work. Phase 5 was not started.
