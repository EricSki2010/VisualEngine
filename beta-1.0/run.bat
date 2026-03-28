@echo off
if "%1"=="" goto main
if "%1"=="--main" goto main
if "%1"=="--test_engine" goto test_engine
if "%1"=="--creator" goto creator
if "%1"=="--creator2" goto creator2

echo Unknown target: %1
echo Usage: run [--main] [--test_engine] [--creator] [--creator2]
goto end

:main
build_msvc\beta.exe
goto end

:test_engine
build_msvc\test_engine.exe
goto end

:creator
build_msvc\mapCreator.exe
goto end

:creator2
build_msvc\mapCreatorV2.exe
goto end

:end
