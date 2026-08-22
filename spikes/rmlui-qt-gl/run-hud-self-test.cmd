@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 || exit /b 1
cmake -G Ninja -S spikes\rmlui-qt-gl -B build-wave4-hud-spike-msvc -DCMAKE_MAKE_PROGRAM=C:\PROGRA~1\MICROS~1\18\COMMUN~1\Common7\IDE\COMMON~1\MICROS~1\CMake\Ninja\ninja.exe -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe -DQt6_DIR=C:\Programming\Repos\Ingnomia2\Ingnomia\.build-support\Qt\6.8.3\msvc2022_64\lib\cmake\Qt6 -DFETCHCONTENT_SOURCE_DIR_RMLUI=C:\Programming\Repos\Ingnomia2\Ingnomia\build-wave1-spike-msvc\_deps\rmlui-src -DFETCHCONTENT_SOURCE_DIR_FREETYPE=C:\Programming\Repos\Ingnomia2\Ingnomia\build-wave1-spike-msvc\_deps\freetype-src || exit /b 1
cmake --build build-wave4-hud-spike-msvc --target ingnomia_rmlui_qt_spike || exit /b 1
if "%1"=="--build-only" exit /b 0
set QT_LOGGING_RULES=*.debug=false
build-wave4-hud-spike-msvc\ingnomia_rmlui_qt_spike.exe --hud-self-test > build-wave4-hud-spike-msvc\hud-self-test.log 2>&1 || exit /b 1
