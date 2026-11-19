# Issue #73: Signature-only free functions

**Status:** Local implementation verified; publication and integration pending
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
- [ ] Remote CI and publication (not authorized).
- [ ] Main-branch integration (Pablo's responsibility).

## Validation notes

`ctest --test-dir <build> -R FreeFunctionOverloadsHaveOnlySignatureKeys
--output-on-failure` fails on unmodified `08d0d1d` because plain-name copies
reappear at odd overload counts; it passes after removal.

`ctest --test-dir <debug-dealii-build> --output-on-failure` passes sequentially.
A `-j 4` attempt failed two VTK cases which expect `grid-1.vtk`; their sequential
rerun passed. Shared-file interference is a hypothesis, not a diagnosed defect,
and is not addressed here. The two skipped MPI tests are not MPI validation.

The independent SELF-output/main integration finding is tracked in #74 and is
not included in this patch. No push, PR creation or remote merge was performed.
