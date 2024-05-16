@echo off
IF NOT EXIST build mkdir build
pushd build
cl -nologo -FAsc -Zi -W3 ..\source\win32_ngame.cpp user32.lib Gdi32.lib Dwmapi.lib Xinput.lib
popd
