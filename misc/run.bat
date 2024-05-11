@echo off
call build.bat
if %errorlevel% neq 0 exit /b %errorlevel%
start ..\build\win32_ngame.exe