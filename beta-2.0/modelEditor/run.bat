@echo off
if exist build\Release\modelEditor.exe (
    cd build\Release
    modelEditor.exe
    cd ..\..
) else (
    echo modelEditor.exe not found. Run build.bat first.
)
