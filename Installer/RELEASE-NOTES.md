# VoxxLab DISRUPTION 1.0.0

Windows x64 VST3 audio destruction effect with BURN, BREAK, RIP, CRUSH and CHAOS engines; Tone, Body and global Crush; HQ oversampling; INIT + 30 factory presets; user presets and independent project recall.

![DISRUPTION interface](https://raw.githubusercontent.com/scryingorb/DISRUPTION/v1.0.0/Docs/Images/DISRUPTION.png)

## Downloads

- **DISRUPTION-1.0.0-Windows-x64-Setup.exe** — installer and uninstaller; installs the complete VST3 bundle into `C:\Program Files\Common Files\VST3`.
- **DISRUPTION-1.0.0-Windows-x64-VST3.zip** — the complete bundle and documentation for manual installation.
- **SHA256SUMS.txt** — checksums for the installer and portable ZIP.

Close the DAW before installing, then rescan VST3 effects. In FL Studio use Manage plugins > Find installed plugins with Verify plugins enabled. User presets are preserved when uninstalling.

The supplied screenshot shows the RC1 preview of the same interface; the release displays version 1.0.0. Manual QA completion and release publication were confirmed by the user. The installer and portable artifact are generated from this release's source commit; the automated workflow verifies hosted processing, state and editor lifecycles before publishing, plus installed DLL hash and uninstall behavior.

Windows x64 only; a VST3-capable x64 host is required. Fixed latency: 60 samples in both HQ states and bypass. No standalone application or other platform binaries are included.
