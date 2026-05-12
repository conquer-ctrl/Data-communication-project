@echo off
REM Build local Win32 GUI client (MinGW g++). Requires g++ on PATH.
cd /d "%~dp0"
g++ -std=c++17 -O2 timetable_gui.cpp database.cpp -o timetable_gui.exe -mwindows -municode -luser32 -lgdi32 -lcomctl32
if errorlevel 1 exit /b 1
echo Built timetable_gui.exe. Place courses.csv and users.txt next to it, or run from this folder.
