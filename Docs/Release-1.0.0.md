# DISRUPTION 1.0.0 release validation

The user confirmed manual QA completion and requested a normal release on 2026-10-17. This supersedes the manual-QA packaging gates recorded in the historical Phase 8 candidate report. No new license terms were invented; the existing JUCE notice is retained.

The final editor displays 1.0.0; numeric plugin and Windows file metadata are 1.0.0. Permanent identity, parameter IDs, ranges/defaults, DSP, preset values, state format and 60-sample latency remain unchanged. The exact user-supplied RC1 screenshot is used in the README and release notes; only the version label differs in the released editor.

Final local build: 350,132,333 assertions passed, zero tracked callback allocations, 264.27 seconds wall time. This includes the 240-case exact Phase 7 DLL comparison, 9,600 extreme cases, 180,000-block deterministic soak, 520 editor cycles and mono/stereo/rate/block/state/preset/HQ/bypass tests. See Tests and the historical Phase 8 report for allocator coverage and methodology limits.

The local Inno Setup installer compiled successfully. Its payload is the complete Windows x64 VST3 bundle plus README, changelog, installation guide, screenshot and JUCE notice. The uninstaller preserves user presets. Portable ZIP contents and DLL bytes were checked. Local artifacts are in the ignored release directory; stale dist was not used or overwritten.

The publication workflow rebuilds from source on Windows Server 2022, runs actual VST3 host validation, compiles an installer, silently installs it on the ephemeral runner, compares installed/built DLL hashes, loads the installed plugin, verifies uninstall removes the DLL, generates checksums, then publishes the normal v1.0.0 release with installer, ZIP and screenshot.

[Source commit](https://github.com/scryingorb/DISRUPTION/commit/4963e944b80b2d926459da4a8a92d5746b45ff8f) | [Publication workflow](https://github.com/scryingorb/DISRUPTION/actions/runs/39012819378) | [Release](https://github.com/scryingorb/DISRUPTION/releases/tag/v1.0.0)

Locally compiled artifacts and GitHub-built artifacts can differ in bytes because compiler, SDK and build paths differ. The downloaded release checksums identify the published assets; local release/validation.json identifies the local validation build. No local DAW installation was performed by the assistant.
