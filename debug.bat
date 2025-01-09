@echo off

setlocal enabledelayedexpansion

call %~dp0\vars.bat

if not exist !project_dir!\!project_name!.rdbg (
	start "" remedybg !project_dir!\build\!project_name!.exe
	exit /b 0
)

start "" remedybg !project_dir!\!project_name!.rdbg

endlocal
