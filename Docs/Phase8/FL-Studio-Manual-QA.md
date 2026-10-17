# FL Studio manual QA — 1.0.0 RC1

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
