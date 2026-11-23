# Issue #81: Package the deal.II backend in the Coral image

## Description

The Coral CLI and shared core are available in the published image. The built-in
deal.II backend is also needed to run deal.II graphs using just this container.

## Implementation plan

- Enable and require the deal.II plugin target when building the image.
- Install the module with the matching SDK in Debug and Release.
- Load the installed plugin and check its registry on AMD64 and ARM64.
- Replace the initial v1.3.0 container with the tested image.
