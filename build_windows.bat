@echo off
REM Local Windows build. Needs: Visual Studio 2022 (workload "Desktop development with C++"), CMake, Git.
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release --target PitchSnap_VST3 || exit /b 1
echo.
echo Done. Plugin folder:  build\PitchSnap_artefacts\Release\VST3\PitchSnap.vst3
