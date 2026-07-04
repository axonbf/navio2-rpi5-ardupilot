# Documentation README

## Document Status

`current`

Base guide to maintain the `docs/` folder clean, consistent and reusable as a base for a new project.

## Current Project

- Name: [pending]
- Initial responsible: [pending]
- General status: [pending]

## Objective

- Centralize project documentation in a simple and maintainable structure.
- Avoid dragging context from previous projects.
- Keep documents brief, current and easy to locate.

## Work Rules

1. All functional documentation of the project goes in this folder.
2. Before creating a new file, check `DOCUMENTATION_INDEX.md`.
3. If a document is no longer valid, update or delete it.
4. If a session leaves important decisions or context, register it in `SESSION_HISTORY.md`.
5. Use clear file names oriented to content.
6. When closing documentation changes, perform cross-consistency review between `README.md`, `DOCUMENTATION_INDEX.md` and affected files.
7. If the change affects flow or architecture, also verify consistency with `TECHNICAL_ARCHITECTURE.md`, `IMPLEMENTATION_PLAN.md` and `TECHNICAL_SETUP.md`.
8. `README.md` must remain as a generic reusable base; avoid including specific technical content of a concrete implementation.
9. If a proposal could break the genericity of `README.md`, confirm first before applying changes.

## Mermaid Diagram Rules

1. The source of truth for architecture is `TECHNICAL_ARCHITECTURE.md`.
2. The `Technical Pipeline`, `Technical Component Diagram` and `Functional Blocks` must always remain synchronized.
3. Stage and block names must be exactly consistent between diagrams and textual description.
4. Avoid redundancies: do not duplicate the same explanation in several sections; leave a main description and clear references.
5. Any flow change must update the three sections in the same document change.

## Advanced Consistency Rules

1. Canonical terminology: define and use a canonical glossary of the technical project; treat variants or historical names as explicitly documented aliases.
2. Status by document: each technical or organizational document must indicate its status (`current`, `in review` or `historical`) when applicable.
3. Real pending items: if a point becomes confirmed in a task, it must leave pending sections in the same change.
4. Mermaid visual consistency: reuse the same `init` block in all diagrams.
5. Explicit organizational closure: when closing a task, update current status in `PROJECT_OBJECTIVE.md` and register the corresponding traceability in `SESSION_HISTORY.md`.

## Assistant Operational Rules

1. Do not revert user changes or execute destructive actions without explicit request.
2. Do not create commits or push without explicit request.
3. Prioritize minimal, traceable changes consistent with the current project status.
4. Maintain synchronization between code, documentation and diagrams when a change affects more than one layer.
5. Avoid redundancies and maintain consistent names and terminology in all updated artifacts.
6. Execute non-destructive verifications first and report key results before proposing major changes.
7. If an instruction is ambiguous and could change the outcome, ask for a single concrete clarification before executing.
8. Direct and practical work: try to solve without blocking with unnecessary questions.
9. Keep responses concise and focused, proposing next steps when it makes sense.

## Priority and Application Consistency

1. First apply the project rules in `README.md` and folder documents.
2. Then apply the specific Mermaid rules.
3. Then apply the assistant operational rules.
4. If conflict appears between rules, prioritize the higher-level rule and maintain change traceability.
5. Every change must preserve consistency between code, documentation and diagrams in names, flow and scope.

## Documentary Status Guide

Use these statuses to maintain traceability and consistency in project documents:

- `current`: updated content aligned with the real project status.
- `in review`: temporarily misaligned content or in update process.
- `historical`: content preserved as reference, not operational for current decisions.

### When to Use Each Status

1. Change to `in review` when:
   - a change is started that impacts the document content.
   - the document becomes partially outdated during the task.

2. Return to `current` when:
   - the task is closed and the document is synchronized.
   - cross-consistency is confirmed with the corresponding axes.

3. Change to `historical` when:
   - the document is no longer operational but worth preserving.
   - a new version exists that replaces its current use.

### Closing Rule

When closing a task:

- do not leave documents in `in review` without explicit reason.
- justify any document in `historical`.
- confirm consistency using the checklists in Definition of Done section.

## Minimum Recommended Structure

- `README.md`: documentation rules.
- `DOCUMENTATION_INDEX.md`: inventory and usage guide.
- `SESSION_HISTORY.md`: session tracking.
- `USAGE_README.md`: instructions for reusing the template.
- `PROJECT_OBJECTIVE.md`: objective and scope.
- `TECHNICAL_DESIGN.md`: design decisions and criteria.
- `TECHNICAL_SETUP.md`: environment, build and execution.
- `TECHNICAL_ARCHITECTURE.md`: architecture and diagrams.
- `IMPLEMENTATION_PLAN.md`: plan and change log.
- `current_opencode_session.md`: operational reference of session format, if applicable.

## Suggested Convention for New Files

- Format: `TOPIC.md` or `specific_topic.md`
- Recommended start:

```md
# Document Title

## Document Status

`current`

## Objective

[Brief description]
```

## Maintenance

- Update the index after creating, renaming or deleting documents.
- Review dates, responsible parties and decisions when the project changes phase.

Closing note per change:
- Use the Definition of Done checklist to verify task completion.
- Reference the four technical axes (`DESIGN`, `SETUP`, `ARCHITECTURE`, `PLAN`) and four organizational axes (`PROJECT_OBJECTIVE`, `SESSION_HISTORY`, `README`, `DOCUMENTATION_INDEX`) for consistency checks.