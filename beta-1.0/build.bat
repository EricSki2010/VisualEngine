@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
cmake --preset default 2>&1
if "%1"=="" (
    cmake --build --preset default 2>&1
) else (
    cmake --build --preset default --target %1 2>&1
)
