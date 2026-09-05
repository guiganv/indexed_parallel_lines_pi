@echo off
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86
set "wxWidgets_ROOT_DIR=D:\Dev\OpenCPN\cache\wxWidgets-3.2.9"
set "wxWidgets_LIB_DIR=D:\Dev\OpenCPN\cache\wxWidgets-3.2.9\lib\vc14x_dll"
cd build
cmake -T v143 -A Win32 -G "Visual Studio 17 2022" ^
  --debug-find-pkg=wxWidgets ^
  -DOCPN_TARGET=MSVC ^
  -DwxWidgets_ROOT_DIR=%wxWidgets_ROOT_DIR% ^
  -DwxWidgets_LIB_DIR=%wxWidgets_LIB_DIR% ^
  -DwxWidgets_CONFIGURATION=mswu ^
  -DCMAKE_BUILD_TYPE=Release ^
  ..
