# ResiliX — Reliability Simulation Backend

ResiliX is a C++17 backend project that models a small microservice dependency graph, failure impact, incident prioritization, recovery policies, persistence, metrics, and an HTTP API for a future dashboard.

## What is implemented

- **Service health model:** latency, error rate, CPU and memory metrics; configurable thresholds; healthy/degraded/failed states.
- **Dependency analysis:** upstream → dependent graph, cycle prevention, direct versus indirect downstream impact.
- **Risk scoring:** metric pressure plus blast-radius weighting, mapped to Low/Medium/High/Critical severity.
- **Recovery engine:** bounded attempts, exponential backoff capped by configuration, configurable cooldown, per-service recovery limits, circuit-breaker guard/reset policy, simulated restart/retry/failover/fallback outcomes, and post-recovery health verification. Extremely severe metric signals can make restart/retry attempts fail in the simulation, allowing the circuit-breaker path to be exercised.
- **Persistence:** configuration in `config.json` (thresholds, recovery policy, API port, file paths, dependency graph); incident history in `incidents.csv` (loaded at startup and rewritten after state changes); metric samples in `metrics.csv`.
- **Evaluation:** service sample counts, incident/resolution counts, mean recovery time, availability estimate, and precision/recall for controlled expected-vs-detected evaluations; evaluation cases are persisted in `metrics.csv.evaluations.csv`. Metric-triggered FAILED transitions create an incident automatically when one is not already open for that service.
- **HTTP API:** JSON endpoints with CORS headers; Windows Winsock and POSIX socket implementations; no third-party library required.
- **Regression tests:** isolated test CSV files; dependency cycle and downstream traversal; all four recovery strategy labels; retry attempts; severe-signal recovery failure; circuit-breaker open/block/failover; recovery limits; automatic incident creation; metric validation; evaluation counters; and incident/evaluation persistence across simulator reinitialization.

## Honest scope

This is a **simulator**, not a production monitoring or orchestration agent. Recovery actions change simulated in-memory service states; they do not restart real operating-system processes, move traffic, or provision replicas. Availability is a sample-based estimate, not a contractual uptime/SLO calculation. Precision/recall counters are persisted in a companion CSV file. Historical recovery entries with a recorded duration of zero are excluded from the mean because their true elapsed time cannot be reconstructed. The circuit breaker prevents normal restart/retry after the configured consecutive-failure threshold; severe metric signals can trigger simulated restart/retry failures, while failover/fallback are modeled recovery choices, not real traffic shifts. `cooldownMs` is loaded from `config.json` and applied before a recovery operation; `circuitResetAfterSuccesses` controls how many successful failover/fallback recoveries clear an open-circuit failure streak. HTTP is intentionally a small educational server; do not expose it to an untrusted network without authentication, request limits, TLS, and hardened parsing.

## Build on Windows (VS Code terminal, g++ / MinGW)

Run from the folder containing all `.cpp` files:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra main.cpp service.cpp incident.cpp dependency_manager.cpp recovery_strategy.cpp persistence.cpp metrics.cpp simulator.cpp api_server.cpp -o ResiliX.exe -lws2_32
```

Run API server:
```powershell
.\ResiliX.exe api
```
Keep the terminal open while using the dashboard. Open `ResiliX_Frontend/index.html` in your browser (or use VS Code Live Server).
Then open `http://localhost:8080/api/health`.

Run console:
```powershell
.\ResiliX.exe console
```

Run regression tests:
```powershell
g++ -std=c++17 -O2 -Wall -Wextra tests.cpp service.cpp incident.cpp dependency_manager.cpp recovery_strategy.cpp persistence.cpp metrics.cpp simulator.cpp -o ResiliXTests.exe
.\ResiliXTests.exe
```

## API reference

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/health` | Liveness / mode |
| GET | `/api/services` | Service status and current metrics |
| GET | `/api/dependencies` | Dependency edges |
| GET | `/api/incidents` | Persisted incident history |
| GET | `/api/metrics` | Aggregate metrics |
| POST | `/api/failure` | Body: `{"service":"Payment","description":"payment timeout"}` |
| POST | `/api/recovery` | Body: `{"service":"Payment","strategy":"restart"}` |
| POST | `/api/metrics` | Body: `{"service":"Payment","latencyMs":620,"errorRatePct":8,"cpuPct":91,"memoryPct":60}` |
| POST | `/api/evaluate` | Re-evaluate all services |

Allowed recovery strategy labels: `restart`, `retry`, `failover`, `fallback`. All are simulations.

## Suggested frontend integration

Use `fetch("http://localhost:8080/api/services")` to load the service list. Use POST with `Content-Type: application/json` for action endpoints. The current API enables CORS for local dashboard development.

## Project files

- `service.*` — service state and metrics
- `dependency_manager.*` — dependency graph and blast radius
- `incident.*` — incident model, severity and timestamps
- `recovery_strategy.*` — recovery policy and simulated strategies
- `persistence.*` — config + CSV history persistence
- `metrics.*` — sample collection and aggregate evaluation metrics
- `simulator.*` — orchestration, risk scoring and persistence
- `api_server.*` — HTTP API
- `main.cpp` — API/console entry point
- `tests.cpp` — regression tests
