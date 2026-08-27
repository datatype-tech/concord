# Contributing to Concord Flash

Thank you for your interest in improving Concord Flash. This guide keeps
contributions consistent and reviewable.

## Before You Start

Clarify the scope of the change: does it affect the public API, engine
internals, build scripts, or examples and documentation? When a change spans
multiple areas, describe the dependencies and expected behavior first. Do
not bundle unrelated changes into a single patch.

Read [`AGENTS.md`](../AGENTS.md) at the repository root before writing any
code — it defines the naming, file layout, and architectural rules every
change must follow (facade re-export pattern, ECS conventions, the 150-line
file limit, the two-DLL split, and more).

## Comment and Commit Language

Comments, documentation, commit subjects, and commit bodies may be written
in Simplified Chinese, Traditional Chinese, or English. Terminology, type
names, paths, commands, and external API names may be kept in their
original form. Do not use mixed-encoding text that cannot be consistently
displayed or read by other contributors.

## Implementation

1. Make changes on a dedicated branch. Do not modify files unrelated to the
   current goal.
2. When adding a public capability, determine its facade header, module,
   and lifecycle first; keep implementation details under the corresponding
   `engine/` sub-area (see `AGENTS.md` §3–§4).
3. When touching rendering, windowing, ECS, or the render-backend factory,
   clarify ownership, which DLL the code belongs to, failure behavior, and
   compatibility impact.
4. Comments should only explain constraints, decisions, or non-obvious
   behavior — never restate what the code already expresses.
5. New source files under `concord/` carry the MPL-2.0 notice above the
   include guard (see `AGENTS.md` §14).

## Verification

Before committing, complete verification appropriate to the scope of the
change:

1. Build the affected targets (`cmake --build build`).
2. Run the engine (or the relevant demo) and confirm the behavior you
   changed.
3. Check that new or changed public headers, Desc fields, and documentation
   stay consistent with each other.
4. Confirm no new errors, warnings, or Vulkan validation messages appear.

## Commit and Review

1. Split work into commits that can be understood and verified
   independently.
2. Commit subjects describe the behavioral change with a concrete subsystem
   (e.g. `render: add clustered light culling`), not generic wording like
   `update` or `fix`.
3. Commit bodies explain the rationale, compatibility impact, and
   verification performed.
4. Pull request descriptions should list the scope of changes, verified
   items, and remaining risks.
5. After receiving feedback, update the implementation, tests, or
   description while keeping the commit history readable.
