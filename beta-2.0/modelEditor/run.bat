@echo off
setlocal
if exist .gemini_key (
    set /p GEMINI_API_KEY=<.gemini_key
)
if exist build\Release\modelEditor.exe (
    cd build\Release
    modelEditor.exe
    cd ..\..
) else (
    echo modelEditor.exe not found. Run build.bat first.
)
endlocal
