# NYCU Embedded Real-Time Systems - Coursework Repository

This repository tracks assignment work for Prof. Li-Ping Chang's Embedded Real-Time Systems course.

The goal is to keep each lab reproducible, reviewable, and suitable as a portfolio project for job applications.

## Repository Goals

1. Preserve complete implementation records.
2. Keep lab artifacts organized by phase (DOSBox verification -> board migration).
3. Maintain frequent, meaningful commits for traceability.

## Current Scope

- Lab 1: RMS scheduling on uC/OS-II (x86 DOSBox verification and Nios II migration preparation)

## Key Files

- `Lab1_Requirements.md`: task specification and acceptance criteria.
- `Implementation_Log.md`: development log and debugging history.
- `REPORT.md`: final operation/verification report and board migration mapping.
- `Board_Migration_Log.md`: board-side adaptation record.
- `NIOS2_Board_Migration_Package/`: board migration assets and references.

## Suggested Commit Rhythm

Use small commits that each answer one question:

1. What changed?
2. Why was it changed?
3. How was it verified?

Example commit sequence for a lab:

1. `docs: add lab requirements and baseline notes`
2. `feat(lab1): add periodic task timing fields to TCB`
3. `feat(lab1): instrument preempt and complete event logging`
4. `fix(lab1): reset OSTime after task creation to remove spurious tick offset`
5. `docs(report): add set1/set2 verification outputs`

## How to Keep This Repo Updated

1. Implement one focused change.
2. Update `Implementation_Log.md` with reason and verification steps.
3. Update `REPORT.md` only when behavior or operation steps change.
4. Commit immediately after local verification.
5. Push to GitHub.

## Portfolio Note

When writing a project description on GitHub or resume, focus on:

- Kernel instrumentation in scheduler/tick paths.
- Deadline detection and deterministic event tracing.
- Migration thinking from x86 DOS simulation to Nios II board environment.