@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 || exit /b 1
cmake -G Ninja -S tests\ui-hud -B build-wave4-hud-tests-vs3 -DCMAKE_MAKE_PROGRAM=C:\PROGRA~1\MICROS~1\18\COMMUN~1\Common7\IDE\COMMON~1\MICROS~1\CMake\Ninja\ninja.exe || exit /b 1
cmake --build build-wave4-hud-tests-vs3 || exit /b 1
ctest --test-dir build-wave4-hud-tests-vs3 --output-on-failure || exit /b 1
cl /nologo /std:c++20 /EHsc /W4 /WX /c src\gui\ui\screens\hud\HudRmlBinding.cpp /Isrc /Ibuild-wave1-spike-msvc\_deps\rmlui-src\Include /Fobuild-wave4-hud-tests-vs3\HudRmlBinding.compile-check.obj || exit /b 1
