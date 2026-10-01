# v0.90.31 — Undo/create temporal identity branch fix

## Failure

After creating an object and undoing the creation, later Cube, +4 Cubes, +8 Cubes, AI authoring, or delete/recreate operations could fail with status code 5 (`duplicate_identity`).

## Cause

Undo correctly removed the object from the active scene but retained its inactive temporal history slot so exact Redo could restore the original handle/state. Creation incorrectly treated that inactive tombstone as an active identity reservation.

## Contract

- Semantic object identity is unique among **active** scene objects.
- Inactive temporal-history slots do not block a new branch from creating the same semantic ID.
- A new branch receives a new slot/handle rather than recycling the old historical slot.
- Creating on a new branch invalidates the obsolete redo branch in the normal temporal-document way.

Regression sequence:

`create -> undo -> recreate same identity -> undo -> redo`

This rule is shared by single creation, batch creation, AI/authoring transactions, and delete/recreate workflows.
