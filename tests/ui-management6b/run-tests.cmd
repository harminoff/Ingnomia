@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
for %%I in ("%~dp0.") do set "SOURCE=%%~fI"
cmake -S "%SOURCE%" -B "%ROOT%\build-ui-management6b" || exit /b 1
cmake --build "%ROOT%\build-ui-management6b" --config Release || exit /b 1
ctest --test-dir "%ROOT%\build-ui-management6b" -C Release --output-on-failure
