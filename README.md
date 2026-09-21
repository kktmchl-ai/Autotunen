# PitchSnap – monophonic auto-tune VST3

Key/scale pitch correction for single voices. Retune Speed 0 ms = hard "robot" effect, higher = natural.
Parameters: Key, Scale, Retune Speed, Amount, Mix, Output. Reports ~28 ms latency to the host (FL compensates).

## Get the Windows plugin (no compiler needed): GitHub Actions
1. Create a new GitHub repository and upload the contents of this folder (keep `.github/`).
2. Open the **Actions** tab -> "Build Windows VST3" -> wait ~10 min for the green tick.
3. Open the run, download the artifact **PitchSnap-VST3-Windows** and unzip it.
4. Copy the **PitchSnap.vst3** folder to `C:\Program Files\Common Files\VST3\`.
5. FL Studio: Options > Manage plugins > **Find installed plugins** (verify). Add it as an effect on a mixer insert.

## Or build locally on Windows
Install Visual Studio 2022 (C++ desktop workload), CMake and Git, then run `build_windows.bat`.

## Tips
- Use it on a single, dry vocal. Not for chords or choirs.
- Set Key/Scale to your song. Retune 0-10 ms for the robot sound, 50-150 ms for subtle correction.
- Rename: edit the variables at the top of `CMakeLists.txt`.

## Layout
- `Source/AutoTuneCore.h` – DSP (YIN pitch detection + pitch-synchronous OLA shifter), no JUCE dependency
- `Source/Plugin*.{h,cpp}` – JUCE wrapper and GUI
- `tests/` – DSP tests (`runcore.cpp` + python) and a headless wrapper test

## License note
JUCE is AGPLv3 or commercial. Fine for personal use; if you distribute or sell the plugin, comply with JUCE's licence.
