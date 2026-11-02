@echo off
title DSO152 SWD Viewer
python "%~dp0swd_viewer.py"
if errorlevel 1 pause
