@echo off
setlocal
if "%~1"=="" (
  echo Usage: run-management-6a-self-test.cmd path-to-rmlui_qt_gl_spike.exe
  exit /b 2
)
"%~1" --management-6a-self-test
