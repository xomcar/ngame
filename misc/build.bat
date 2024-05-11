@echo off
mkdir ..\build
pushd ..\build
cl -nologo -Zi ..\ngame\source\win32_ngame.cpp user32.lib Gdi32.lib Dwmapi.lib Xinput.lib
popd
