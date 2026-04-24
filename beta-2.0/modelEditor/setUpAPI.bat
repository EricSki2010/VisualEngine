@echo off
if "%~1"=="" (
    echo Usage: setUpAPI YOUR_API_KEY
    exit /b 1
)
<NUL set /p="%~1">.gemini_key
echo API key saved to .gemini_key (gitignored). Next: .\run
