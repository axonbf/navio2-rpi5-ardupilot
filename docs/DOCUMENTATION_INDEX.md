# Documentation Index

## Document Status

`current`

## Purpose

Inventory of all documentation in this folder. Check here before creating a new file.

---

## Start here

### `QUICK_START.md`
Step-by-step guide to reproduce the full Navio2 + Pi 5 setup from scratch:
OS config → RCIO overlays → drive-strength service → kernel module build → ArduPilot build → run.
**Read this first if you are setting up a new Pi 5.**

---

## Reference documents

### `PROJECT_OBJECTIVE.md`
What this project achieves, scope, constraints, and current status.

### `TECHNICAL_SETUP.md`
Minimum environment requirements, build commands, ArduPilot configuration, RCIO deployment steps, and next steps for ArduRover integration.

### `TECHNICAL_ARCHITECTURE.md`
System architecture: SPI/I2C/GPIO layout, RCIO kernel module structure, DW SPI controller details, sensor mapping, and the two root-cause fixes.

### `TECHNICAL_DESIGN.md`
Design decisions: why libgpiod over pigpio, why native build, ArduPilot board strategy, constraints.

### `IMPLEMENTATION_PLAN.md`
Phase-by-phase work plan with current completion status and detailed next steps for Phase 5 (ArduRover integration, I2C+RCIO bus contention, boat parameters).

### `NAVIO2_PINOUT_REFERENCE.md`
Official Navio2 40-pin header mapping: RCIO SPI pins (GPIO16/19/20/21), RCIO SWD pins (GPIO12/13), sensor SPI buses.

### `SESSION_HISTORY.md`
Full session log. The 2026-05-14 entry documents the RCIO root causes, diagnostic path, and fix in detail.

---

## Operational

### `README.md`
Documentation rules, Mermaid diagram rules, assistant operational rules. Read before modifying docs.

### `DOCUMENTATION_INDEX.md`
This file.

### `current_opencode_session.md`
Query template for identifying the active OpenCode session ID.

### `USAGE_README.md`
How to reuse this documentation template in a new project.

---

## Statistics

- Total active documents: 10
- Deleted (2026-05-14 clean-up, logged in SESSION_HISTORY): `SOLUTION_4_PLAN.md`, `SOLUTION_C_PLAN.md`, `RCIO_PI5_HANDOFF.md`, `RCIO_PI5_REALISTIC_PLAN.md`, `HARDWARE_DEBUG_CHECKLIST.md`
