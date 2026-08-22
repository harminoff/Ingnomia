@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 || exit /b 1
cmake -G Ninja -S spikes\rmlui-qt-gl -B build-wave5-inspector-spike-msvc -DCMAKE_MAKE_PROGRAM=C:\PROGRA~1\MICROS~1\18\COMMUN~1\Common7\IDE\COMMON~1\MICROS~1\CMake\Ninja\ninja.exe -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe -DQt6_DIR=C:\Programming\Repos\Ingnomia2\Ingnomia\.build-support\Qt\6.8.3\msvc2022_64\lib\cmake\Qt6 -DFETCHCONTENT_SOURCE_DIR_RMLUI=C:\Programming\Repos\Ingnomia2\Ingnomia\build-wave1-spike-msvc\_deps\rmlui-src -DFETCHCONTENT_SOURCE_DIR_FREETYPE=C:\Programming\Repos\Ingnomia2\Ingnomia\build-wave1-spike-msvc\_deps\freetype-src || exit /b 1
cmake --build build-wave5-inspector-spike-msvc --target ingnomia_rmlui_qt_spike || exit /b 1
cmake -E copy_directory content\rmlui build-wave5-inspector-spike-msvc\assets || exit /b 1
build-wave5-inspector-spike-msvc\ingnomia_rmlui_qt_spike.exe --inspector-self-test > build-wave5-inspector-spike-msvc\inspector-runner-output.log 2>&1 || exit /b 1
findstr /c:"Inspector vertical proof passed" /c:"Inspector click ownership passed" /c:"Inspector event listener teardown passed" build-wave5-inspector-spike-msvc\inspector-self-test.log || exit /b 1
exit /b 0
