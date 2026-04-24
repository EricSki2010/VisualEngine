@echo off
if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
cd ..
xcopy /E /I /Y ..\assets build\Release\assets
copy /Y src\mechanics\AiHandling\agent.py build\Release\agent.py
copy /Y dist_run.bat build\Release\run.bat
