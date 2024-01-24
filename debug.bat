@echo off

if not exist 3d_model_viewer.rdbg (
	start "" remedybg build\3d_model_viewer.exe
	exit /b 0
)

start "" remedybg 3d_model_viewer.rdbg
