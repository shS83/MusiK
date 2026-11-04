@echo off
setlocal
set "PATH=C:\Qt\6.12.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%"
start "" "%~dp0build-win-mingw\MusiK.exe" %*

