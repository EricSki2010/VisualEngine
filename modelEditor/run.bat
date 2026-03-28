@echo off
if exist build\Release\modelEditor.exe (
    build\Release\modelEditor.exe
) else (
    echo modelEditor.exe not found. Run build.bat first.
)
