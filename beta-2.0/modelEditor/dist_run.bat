@echo off
setlocal
if exist .gemini_key (
    set /p GEMINI_API_KEY=<.gemini_key
)
if exist modelEditor.exe (
    modelEditor.exe
) else (
    echo modelEditor.exe not found in this folder.
    pause
)
endlocal
