# Windows installation

1. Close FL Studio and other audio hosts.
2. Run DISRUPTION-1.0.0-Windows-x64-Setup.exe. Administrator approval is required for the standard shared VST3 location.
3. The complete bundle is installed to `C:\Program Files\Common Files\VST3\DISRUPTION.vst3`.
4. Open FL Studio: Options > File settings > Manage plugins, enable Verify plugins and Find installed plugins. When replacing a prior copy, enable Rescan previously verified plugins.
5. Load VoxxLab DISRUPTION as an effect on an audio mixer track.

The portable ZIP contains the same whole bundle for manual copying. Do not copy only its internal DLL. This is a Windows x64 VST3, not a standalone application. Supports mono and stereo in a VST3-capable x64 host. Other operating systems and architectures are not provided.

Uninstall via Windows Installed apps > VoxxLab DISRUPTION. User presets under `%APPDATA%\VoxxLab\DISRUPTION\Presets` are left intact; back them up separately. Close the DAW before upgrading or uninstalling. Documentation and uninstall files are stored in Program Files\VoxxLab\DISRUPTION.

[FL Studio official plugin scanning documentation](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm).
