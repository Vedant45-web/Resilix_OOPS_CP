RESILIX — START HERE

1. Open this folder in VS Code.
2. Ensure MinGW-w64 g++ is installed and `g++ --version` works.
3. Double-click START_RESiLIX.bat. Keep its terminal window open.
4. Open ResiliX_Frontend/index.html in your browser (or use VS Code Live Server).
5. Check that the dashboard says API connected.
6. Read PRESENTATION_CHECKLIST.md for the demo order and honest scope notes.

Backend tests: open ResiliX_Reliability_Backend and run RUN_TESTS_WINDOWS.bat. The regression suite uses isolated test CSV files and covers restart/retry/failover/fallback, circuit-breaker handling, metric-triggered incidents, and persistence.
Data files (incidents.csv, metrics.csv, metrics.csv.evaluations.csv) are persisted beside the backend executable/source. Back them up before clearing demo history.

ResiliX is a simulator. Recovery actions change simulated service states; they do not restart real production services.
