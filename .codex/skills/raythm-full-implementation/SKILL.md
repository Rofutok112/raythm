---
name: raythm-full-implementation
description: Complete-feature implementation guidance for the raythm rhythm game. Use when the user wants raythm work done beyond MVP, minimal-change, or proof-of-concept scope, especially requests for full implementation, production-quality behavior, broad feature completion, or work that should include architecture, UI polish, edge cases, integration, and verification.
---

# Raythm Full Implementation

Use this skill to raise the completion bar for raythm feature work. Treat the request as a request for a complete usable feature, not a narrow proof of concept or a smallest possible patch.

## Operating Mode

- Start from the intended finished experience, then work backward into implementable pieces.
- Read enough surrounding code to understand the existing feature shape, state flow, rendering flow, persistence, and tests.
- Prefer vertical completion over tiny isolated slices: wire the feature through the user-facing path, state updates, rendering, input, persistence or services, and build targets when relevant.
- Avoid TODO-driven delivery, placeholder behavior, dead controls, fake data, and partial wiring unless a dependency is genuinely unavailable.
- Do not expand into unrelated refactors. Keep the larger implementation bounded by the user's requested feature and nearby ownership boundaries.
- When the work touches structure, also apply `raythm-architecture`.
- When the work touches UI, also apply `raythm-ui-layout`.
- When verifying compiled raythm code, also apply `raythm-build-codex`.

## Completion Bar

Before finishing, check the feature from a user's point of view:

- The primary happy path works end to end.
- Expected empty, loading, failure, cancel, back/close, disabled, and selection states are handled when relevant.
- Input and hit regions match the rendered controls.
- State changes are not hidden inside drawing code.
- Data ownership, service boundaries, and scene responsibilities match nearby architecture.
- New or changed behavior has focused test or smoke coverage when practical.
- The app or relevant targets build through the Codex raythm build workflow when compiled code changed.
- Any intentionally deferred work is explicit and justified by a real blocker, not by MVP scope.

## Planning Style

When the user asks for a plan, produce a human-readable implementation plan that is ambitious but concrete:

- Describe the finished behavior first.
- Group work by feature surface or subsystem, not by tiny mechanical edits.
- Include integration and verification as first-class steps.
- Call out meaningful risks and unknowns.
- Avoid framing the plan as MVP unless the user explicitly asks for an MVP.

When the user asks to implement, do not stop at the plan. Execute the work, iterate through build or test failures, and leave the repository in a coherent state.
