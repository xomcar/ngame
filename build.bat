@echo off
IF NOT EXIST build mkdir build
pushd build
cl -nologo -FAsc -Zi -W4 -DDEVEL ..\source\win32_ngame.cpp user32.lib Gdi32.lib Dwmapi.lib Xinput.lib
popd
