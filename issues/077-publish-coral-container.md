# Issue #77: Publish the Coral CLI and SDK container

**Branch** `77-publish-coral-container`

Install shared-core Coral in separate Debug and Release prefixes on the same
deal.II base image used by ImmersX and MetricFlowX. Build each architecture on
a native runner and smoke-test the installed CLI by loading a plugin compiled
against the corresponding installed SDK. Publish the multi-architecture tags
from main after the checks pass.

The ImmersX repository owns the tutorial backend with its plugins and Slurm.
