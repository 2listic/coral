# Test Output Directory Management

## Overview

Tests automatically create output directories under `./test_output/` to isolate test artifacts (status files, etc.). These directories are managed by the `ScopedTestOutputDir` RAII class.

## Default Behavior

- **Test passes** ✅: Output directory is automatically deleted
- **Test fails** ❌: Output directory is kept for debugging

## Environment Variables

You can customize the cleanup behavior using environment variables:

### `CORAL_KEEP_SUCCEEDED_OUTPUT`

Keep output directories even for tests that pass.

**Usage:**
```bash
CORAL_KEEP_SUCCEEDED_OUTPUT=1 ./build/gtests/coral_gtests
```

**Use case:** Inspect output files from successful tests for verification or analysis.

### `CORAL_DELETE_FAILED_OUTPUT`

Delete output directories even for tests that fail.

**Usage:**
```bash
CORAL_DELETE_FAILED_OUTPUT=1 ./build/gtests/coral_gtests
```

**Use case:** Keep the test environment clean during CI/CD runs.

### Combining Both

You can combine both flags:
```bash
# Keep all output (never delete)
CORAL_KEEP_SUCCEEDED_OUTPUT=1 ./build/gtests/coral_gtests

# Delete all output (always delete)
CORAL_DELETE_FAILED_OUTPUT=1 ./build/gtests/coral_gtests

# Invert behavior: keep failed, delete succeeded (default is opposite)
# This doesn't make sense, so don't use both together
```

## Examples

### Debug a failing test
```bash
# Run tests normally - failed test output is preserved
./build/gtests/coral_gtests --gtest_filter=Modules.VtkGen3

# Check the output directory
ls -la ./test_output/Modules_VtkGen3/
```

### Keep all test output for inspection
```bash
# Keep output from all tests
CORAL_KEEP_SUCCEEDED_OUTPUT=1 ./build/gtests/coral_gtests

# Inspect any test's output
ls -la ./test_output/
```

### Clean CI/CD runs
```bash
# Delete everything, even failed tests (for clean CI builds)
CORAL_DELETE_FAILED_OUTPUT=1 ./build/gtests/coral_gtests
```

## Directory Structure

Test output directories follow this pattern:
```
./test_output/
├── Modules_HyperCubeNetworkConnectedRun/
├── Modules_VtkGen1/
├── Network_BareMinimal/
└── dealiiExamples_NetworkStep00/
```

Each directory contains:
- `.running` files - Created when a node starts running
- `.succeeded` files - Created when a node completes successfully
- `.failed` files - Created when a node fails
- Other test-specific output files

## Implementation

See `gtests/test_utils.h` for the `ScopedTestOutputDir` class implementation.
