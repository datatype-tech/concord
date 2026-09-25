@echo off
cd /d "%~dp0"
if exist "build-cli\game.exe" (cd build-cli) else (cd build)
start "" "game.exe"
