@echo off
cd /d "%~dp0"
g++ -std=c++17 -O2 -Wall -Wextra tests.cpp service.cpp incident.cpp dependency_manager.cpp recovery_strategy.cpp persistence.cpp metrics.cpp simulator.cpp -o ResiliXTests.exe
if errorlevel 1 pause & exit /b 1
ResiliXTests.exe
pause
