@echo off
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86
set "PATH=%PATH%;C:\Program Files (x86)\NSIS"
set "WXWIN=D:\Dev\ocpn_wxWidgets"
set "wxWidgets_ROOT_DIR=D:\Dev\ocpn_wxWidgets"
set "wxWidgets_LIB_DIR=D:\Dev\ocpn_wxWidgets\lib\vc_dll"
set "WX_VER=32"
cd build
cmake -T v143 -A Win32 -G "Visual Studio 17 2022" ^
  -DOCPN_TARGET=MSVC ^
  -DwxWidgets_ROOT_DIR=%wxWidgets_ROOT_DIR% ^
  -DwxWidgets_LIB_DIR=%wxWidgets_LIB_DIR% ^
  -DwxWidgets_CONFIGURATION=mswu ^
  -DCMAKE_BUILD_TYPE=Release ^
  ..
if errorlevel 1 goto :eof
cmake --build . --target package --config Release -- -maxCpuCount:2 -property:UseMultiToolTask=false -property:EnableClServerMode=false
