# Issue #73: Signature-only free functions

**Status:** Published in PR #75; CI follow-up in progress
**PR:** https://github.com/2listic/coral/pull/75
**Branch:** `fix/signature-only-functions`
**Issue:** https://github.com/2listic/coral/issues/73
**Base:** `dealiix-tutorial` at `08d0d1d`; integration into `main` belongs to Pablo

## Goal and scope

Remove plain-name function aliases, rather than retaining compatibility or adding
alias metadata. Keep signature-keyed registration and execution unchanged. This
patch does not merge the tutorial branch into main or change the inheritance API.

## Checklist

- [x] Remove alias registration, lookup and registry copies.
- [x] Update graph fixtures and C++ examples to signature keys.
- [x] Reproduce the odd/even overload bug with a failing regression on the base.
- [x] Verify all five overloads execute and only signature keys remain; cover void,
  explicit-output and SELF-output registration.
- [x] Debug and Release core CTest: 26 passed in each configuration.
- [x] Debug full CTest, sequential: 94 discovered, 92 passed, 2 MPI cases skipped.
- [x] CI aggregate targets in Debug and Release: core 26/26, backend 64/64,
  plugin 1/1 in each configuration.
- [x] Publish the scoped patch as PR #75.
- [ ] Remote CI after the shared-registry regression fix.
- [ ] Main-branch integration (Pablo's responsibility).

## Validation notes

`ctest --test-dir <build> -R FreeFunctionOverloadsHaveOnlySignatureKeys
--output-on-failure` fails on unmodified `08d0d1d` because plain-name copies
reappear at odd overload counts; it passes after removal.

`ctest --test-dir <debug-dealii-build> --output-on-failure` passes sequentially.
A `-j 4` attempt failed two VTK cases which expect `grid-1.vtk`; their sequential
rerun passed. Shared-file interference is a hypothesis, not a diagnosed defect,
and is not addressed here. The two skipped MPI tests are not MPI validation.

PR #75's first CI run (`5ab8b45`, run `44578904322`) failed because the new
five-overload test registered `float`, which `TrivialTypes.Unregistered` expects
to remain unregistered. CTest's per-test processes hid this shared-registry
interaction. The aggregate `run_coral_core_tests` target reproduces the failure.
Use `long long` for the fifth overload instead; keep the unregistered-type oracle
unchanged and validate the aggregate targets used by CI.

The independent SELF-output/main integration finding is tracked in #74 and is
not included in this patch. No remote merge was performed.
