# Issue
Issue #66 has been opened.

## To my comprehension
A sort of race condition on a global state of dealii thru class ParameterAcceptor

## How to reproduce (bad idea)
The OP of the issue reccomend `/home/matteo/Projects/dealII-X/repos/coral-editor/test_files/minimal_failing.json` under immersx.
immersx is a huge repo `/home/matteo/Projects/dealII-X/repos/immersx`.
The idea is bad because immersx is huge to build (and needs a huge container that compiles in 15 mins)

## How to reproduce (my idea)
In `minimal_failing.json` there are the following classes defined in immersx which are not defined in dealii plugin:
* `immersX::DomainParameters` which is defined in immersx in `include/immersx/core/domain.h`;
* `immersX::FiniteSpaceElementParameters` which is defined in immersx in `include/immersx/core/fe_space.h`;
They both inherit from `dealii::ParamenterAcceptor` class.

Thus my roadmap is to:
* add `DomainParamenter` and `FiniteSpaceElementParameter` in dealii coral, that should be immediate;
* rebuild the dealii registry with coral with these two nodes added;
* modify `minimal_failing.json` file to point to the correct name of these two nodes;
* try to understand the behavior.

## Possible solution
The OP suggest a complacate serialization mechanism.
If I understand correctly, it should be enough to put associate `dealii::ParameterAcceptor` to a static mutex member in some way (to discuss).
However firs the problem has to be reproduced.

## TODO
* check the feasibility of what I said;
* discuss interactively and if possible, implement the plan;
* discuss technical detail of the solution.
