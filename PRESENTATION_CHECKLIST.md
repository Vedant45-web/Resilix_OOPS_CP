# ResiliX — last-mile presentation checklist

## Start-up
1. Install MinGW-w64 g++ and make sure `g++ --version` works in VS Code terminal.
2. Double-click `START_RESiLIX.bat` (or run it from a terminal). Keep the terminal open.
3. Open `ResiliX_Frontend/index.html` in a browser, or use VS Code Live Server.
4. Confirm the top bar says **API connected** and `http://localhost:8080/api/health` returns `ok: true`.

## Demo sequence (about 3 minutes)
1. **Overview:** explain seven simulated services and current health.
2. **Dependencies:** show upstream → dependent arrows; explain that the graph represents configured relationships, not live network traffic.
3. **Inject failure:** choose Payment and inject a failure. Show incident severity/risk and the downstream impact (Order and API Gateway).
4. **Recovery center:** choose Payment, select Restart simulation, run recovery, and show post-recovery health verification. The backend also models retry with bounded exponential backoff, failover/fallback paths, per-service recovery limits, and circuit-breaker protection under severe signals; all are simulations, not real process restarts or traffic shifts.
5. **Metrics:** show sample availability, recovery outcomes and controlled detector evaluation. Record examples for all four cases: TP (expected failure + detected), FP (no expected failure + detected), TN (no expected failure + not detected), FN (expected failure + not detected).
6. **Persistence:** if time allows, restart the backend and show incident/evaluation history remains saved.

## Important interpretation notes
- This is a controlled simulator. It does not restart real OS processes or shift live traffic.
- Availability is a sample-based estimate, not production uptime/SLO.
- Historical incident rows with a recovery duration of zero do not contain enough information to reconstruct elapsed time; they are excluded from the mean recovery-time calculation.
- Precision/recall are only meaningful with a representative number of labelled test cases. Do not claim high real-world accuracy based on a few manual tests.

## Final checks
- [ ] Backend builds without errors.
- [ ] API connected indicator is green.
- [ ] Failure injection creates an incident.
- [ ] Metric thresholds can automatically create a failure incident.
- [ ] Recovery resolves the incident and returns the service to healthy.
- [ ] TP, FP, TN, FN counts each update.
- [ ] Backend regression suite passes, including retry, failover/fallback and circuit-breaker cases.
- [ ] Evaluation counts persist after backend restart.
- [ ] Search, filters, refresh and navigation work.
