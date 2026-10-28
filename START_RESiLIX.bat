@echo off
setlocal
cd /d "%~dp0ResiliX_Reliability_Backend"
echo Building ResiliX backend...
g++ -std=c++17 -O2 -Wall -Wextra main.cpp service.cpp incident.cpp dependency_manager.cpp recovery_strategy.cpp persistence.cpp metrics.cpp simulator.cpp api_server.cpp -o ResiliX.exe -lws2_32
if errorlevel 1 (
  echo.
  echo Build failed. Install MinGW-w64 g++ and ensure it is on PATH.
  pause
  exit /b 1
)
echo.
echo Starting ResiliX API at http://localhost:8080
ResiliX.exe api
pause
