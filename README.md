# ResiliX – Distributed System Failure Detection and Recovery Simulator  
**Project Type:** Object-Oriented Programming (OOP) Course Project  
**Technologies:** C++17, HTML, CSS, JavaScript, REST API

## Project Overview

ResiliX is a distributed system reliability simulator designed to demonstrate how service failures can be detected, monitored, and recovered using object-oriented programming concepts.

The project simulates a network of interconnected services and provides a web-based dashboard for observing system health, incidents, dependencies, recovery operations, and reliability metrics.

## Key Features

- Real-time service health monitoring
- Simulated service failure injection
- Service dependency visualization
- Incident detection and tracking
- Automated and manual recovery simulation
- Reliability metrics and reports
- Persistent incident and metrics history
- Interactive web dashboard

## Simulated Services

The system includes seven interconnected services:

1. API Gateway
2. Authentication Service
3. Order Service
4. Payment Service
5. Inventory Service
6. Database
7. Cache

## Technology Stack

| Component | Technology |
|---|---|
| Backend | C++17 |
| Frontend | HTML, CSS, JavaScript |
| API | HTTP REST API |
| Data Storage | CSV files |
| Configuration | JSON |
| Platform | Windows |

## Project Structure

```text
OOPS_CP/
├── ResiliX_Reliability_Backend/
├── ResiliX_Frontend/
├── START_RESiLIX.bat
├── RUN_BACKEND_WINDOWS.bat
├── README.md
└── .gitignore
```

## How to Run

1. Clone or download the repository.
2. Open the project folder in VS Code on Windows.
3. Ensure a compatible C++17 compiler is installed.
4. Run `START_RESiLIX.bat`.
5. Open `ResiliX_Frontend/index.html` in your browser.
6. Keep the backend terminal running while using the dashboard.

The backend API runs locally on port `8080` by default.

## Testing

The backend includes automated tests.

Run `RUN_TESTS_WINDOWS.bat` from the `ResiliX_Reliability_Backend` folder.

## Important Note

ResiliX is an educational simulation. Recovery operations and reliability metrics demonstrate distributed-system concepts; they do not represent actual production infrastructure recovery or measured production uptime.

## Project Objective

To demonstrate object-oriented programming principles through the simulation of distributed-system failures, dependency management, incident handling, and recovery strategies.
