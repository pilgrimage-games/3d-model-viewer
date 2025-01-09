@echo off

setlocal enabledelayedexpansion

call %~dp0\vars.bat

pushd !project_dir!\build\
start "" !project_name!.exe
popd

endlocal
