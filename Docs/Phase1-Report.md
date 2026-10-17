# DISRUPTION 0.1.0 — Phase 1 handoff

VoxxLab's Windows x64 Release VST3 audio effect foundation. Target product version: 1.0.0. Build uses JUCE 8.0.12, MSVC 19.29.30148, Visual Studio 2019 Build Tools and Windows SDK 10.0.19041.0. The official supplied emblem is preserved in Assets; the editor deliberately uses simple charcoal, metal-gray and orange branding.

## Permanent parameters

All IDs are lowercase, stable APVTS IDs, version hint 1. Float steps: 0.01 in displayed units. Never rename IDs or change choice order in a compatible release.

| ID | Range / choices | Default |
|---|---|---|
| mode | 0 BURN, 1 BREAK, 2 RIP, 3 CRUSH, 4 CHAOS | 0 BURN |
| drive | 0 to +36 dB | 0 dB |
| damage | 0 to 100% | 0% |
| tone | -100 to +100% (dark / neutral / bright) | 0% |
| body | 0 to 100% | 0% |
| crush | 0 to 100% | 0% |
| mix | 0 to 100% | 100% |
| output | -24 to +12 dB | 0 dB |
| hq | Boolean | Off |
| bypass | Boolean | Off |

## Permanent plugin identity

- Product: DISRUPTION; manufacturer: VoxxLab.
- Bundle identifier: com.voxxlab.disruption.
- JUCE manufacturer code: VxLb (0x56784C62).
- JUCE plugin code: Dsrp (0x44737270).
- VST3 processor class ID: ABCDEF019182FAEB56784C6244737270.
- VST3 controller class ID: ABCDEF011234ABCD56784C6244737270.
- VST3 categories: Fx, Distortion; no MIDI, not an instrument.
- Identity codes and VST3 class IDs must remain unchanged across future versions.

## DSP and state architecture

PluginProcessor owns APVTS and caches atomic parameter pointers. It reads and validates parameter snapshots once per block. DisruptionEngine handles safety, gain smoothing, engine selection and stage coordination. Each mode has a separate engine class and implementation file, with prepare/reset/process hooks and DAMAGE routed as an argument. ToneProcessor, BodyProcessor and CrushProcessor each expose independent replaceable processing stages.

Signal flow: finite input safety -> preserve clean input sample -> smoothed DRIVE -> reserved HQ/oversampling location -> selected engine with DAMAGE -> CRUSH -> TONE -> BODY -> linear dry/wet MIX -> smoothed OUTPUT -> finite protection and 5 Hz DC blocker -> smoothed bypass crossfade to clean input. The dry sample is preserved in a local variable before wet processing; there is no scratch-buffer allocation. DRIVE, OUTPUT, MIX and bypass use 20 ms ramps. HQ is read but currently inactive. No oversampling or algorithmic latency (reported latency: 0 samples).

Mono and stereo with matching input/output channel counts are supported. Mismatched layouts and surround are rejected. Unexpected processing buffers are safely cleared. Input/output non-finite values become zero; denormals are suppressed. Emergency sample bounds are +/-1,000,000 (+120 dBFS), not a normal audio limiter. High DRIVE/OUTPUT settings can exceed 0 dBFS; keep monitoring levels low when testing maximum gain.

For exact clean dry behavior, MIX=0 skips DC filtering of the output; OUTPUT still applies. Fully engaged bypass returns clean sanitized input, independent of DRIVE/MIX/OUTPUT. DC state is maintained during bypass for a smooth return. Nonzero wet settings use the final DC blocker, including default identity mode. Bypass transitions ramp over 20 ms for both the parameter and host processBlockBypassed path. No added tail is advertised for the inaudible DC filter decay.

State root: DISRUPTION_STATE; schemaVersion: 1. JUCE binary XML encodes all ten parameters. Restore merges only known IDs into the complete schema, rejects nonnumeric/non-finite values, clamps and snaps legal values, and ignores malformed/foreign XML. Unknown fields are ignored for future migration. APVTS copy/replace is confined to state operations, not processBlock. This provides a shared basis for future presets.

## Validation

Release build succeeded. CTest passed: 311,653 assertions; zero tracked ordinary C++ new/new[] allocations during processor callbacks. This instrumentation does not claim to intercept every OS allocation API.

Automated coverage: initialization/editor creation; mono/stereo; 44.1/48/88.2/96 kHz; silence; parameter minima/maxima; rapid automation across all modes; exact dry behavior; wet gain; partial mix and OUTPUT placement; channel independence; DRIVE and OUTPUT ramps; plugin and host bypass; variable/oversized buffers; NaN/infinity/extreme input; invalid parameter snapshots; state round trip for all ten parameters; invalid state/range rejection; unsupported layouts. The harness also discovers and loads the actual compiled VST3, runs mono/stereo audio, restores changed host parameter state and creates the hosted editor.

The editor snapshot in Docs/editor-preview.png was visually inspected. Final incremental build log contains no warnings/errors. An initial test-host build used a removed JUCE helper API; it was corrected to explicitly register VST3PluginFormat. FL Studio itself has not been exercised; the real VST3 is verified through JUCE's host harness.

## Intentional placeholders

BURN, BREAK, RIP, CRUSH and CHAOS are identity engines. DAMAGE, TONE, BODY and CRUSH have no audible processing yet; HQ has no oversampling yet. No final distortion algorithms, distressed artwork, waveform displays, particles or preset browser were implemented. The UI is a fixed-size temporary editor. The binary is an unsigned development build.

## Build and packaged artifact

Installable bundle (copy the entire directory):

C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\dist\DISRUPTION.vst3

Original compiled bundle:

C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3

Binary inside bundle:

C:\Users\Admin\Desktop\Coding Projects\DISRUPTION\dist\DISRUPTION.vst3\Contents\x86_64-win\DISRUPTION.vst3

Build from PowerShell with CMake 3.22+ and Visual Studio C++ x64 tools:

```powershell
.\build.ps1 -CMakePath 'path\to\cmake.exe' -JucePath 'path\to\JUCE-8.0.12'
```

Omit -JucePath to fetch the pinned JUCE 8.0.12 tag from GitHub. This build used the existing sibling checkout at C:\Users\Admin\Desktop\Coding Projects\GHOST (Phasmora)\build\_deps\juce-src and CMake at C:\Users\Admin\Desktop\Coding Projects\GHOST (Phasmora)\.tools\cmake-package\cmake\data\bin\cmake.exe. Neither sibling project was modified.

## FL Studio installation and test

1. Close FL Studio. Copy the entire dist\DISRUPTION.vst3 folder into C:\Program Files\Common Files\VST3\DISRUPTION.vst3. Windows may request administrator permission for that copy. Do not copy only the inner binary.
2. Open FL Studio, go to Options -> Manage plugins, enable Verify plugins, and run Find installed plugins / Start scan (label depends on version).
3. Find DISRUPTION from VoxxLab under effects, optionally favorite it, then add it to a Mixer effect slot.
4. Start at default values with a quiet test signal. Confirm DRIVE/OUTPUT gain, MIX=0 clean dry audio, and bypass. DAMAGE/TONE/BODY/CRUSH/HQ and mode changes are intentionally inaudible in this phase.
5. Change all controls, save the FL Studio project, close/reopen it and verify every control restores. Try both mono source material and a stereo source. Keep this development build on a test project.

Official Image-Line instructions: https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm

## Files delivered

The inventory below lists authored source, assets, supporting documents and the shipped bundle. Generated CMake/MSVC intermediates are contained under build/ and are not hand-maintained source.
- `.gitignore`
- `Assets/Disruption_Molten_Tech_Emblem.png`
- `build.log`
- `build.ps1`
- `CMakeLists.txt`
- `configure.log`
- `dist/DISRUPTION.vst3/Contents/Resources/moduleinfo.json`
- `dist/DISRUPTION.vst3/Contents/x86_64-win/DISRUPTION.vst3`
- `Docs/editor-preview.png`
- `Docs/Phase1-Report.md`
- `README.md`
- `Source/DSP/BodyProcessor.cpp`
- `Source/DSP/BodyProcessor.h`
- `Source/DSP/BreakEngine.cpp`
- `Source/DSP/BreakEngine.h`
- `Source/DSP/BurnEngine.cpp`
- `Source/DSP/BurnEngine.h`
- `Source/DSP/ChaosEngine.cpp`
- `Source/DSP/ChaosEngine.h`
- `Source/DSP/CrushEngine.cpp`
- `Source/DSP/CrushEngine.h`
- `Source/DSP/CrushProcessor.cpp`
- `Source/DSP/CrushProcessor.h`
- `Source/DSP/DisruptionEngine.cpp`
- `Source/DSP/DisruptionEngine.h`
- `Source/DSP/ModeEngine.h`
- `Source/DSP/RipEngine.cpp`
- `Source/DSP/RipEngine.h`
- `Source/DSP/ToneProcessor.cpp`
- `Source/DSP/ToneProcessor.h`
- `Source/PluginEditor.cpp`
- `Source/PluginEditor.h`
- `Source/PluginProcessor.cpp`
- `Source/PluginProcessor.h`
- `tests.log`
- `Tests/DisruptionTests.cpp`
- `ThirdPartyLicenses/JUCE-LICENSE.md`

Packaged dist bundle independently passed the same 311,653 checks. Binary SHA-256: 8A801F8109265CB958C6CB5EFEABC1A45DD44E65488E9A6135B18223FA214C42.
