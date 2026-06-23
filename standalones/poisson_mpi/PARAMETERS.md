# `poisson_mpi` — JSON parameters

Run with: `mpirun -n <N> ./poisson_mpi parameters.json`

The parameter file is a single JSON object, read with deal.II's
`ParameterAcceptor` / `ParameterHandler` (no third-party JSON library).
**All keys are optional**; the defaults below are used when a key is missing.
Two things follow from using `ParameterHandler`:

- **Unknown keys are rejected** (a typo'd key aborts the run with a clear
  message), rather than being silently ignored.
- If the file **does not exist**, a ready-to-run example (identical to the
  shipped `parameters_generator.json`) is written to that path (by rank 0) and
  the program exits gracefully with a message — so a missing file is
  self-documenting. With no argument at all, the default path is
  `parameters_generator.json`.

Boundary-id lists are a **comma-separated string** (e.g. `"0, 1, 2, 3"`), not a
JSON array; an empty list is `""`. The schema is identical to the serial
`poisson`, so the same file works for both programs (only the solver defaults
differ).

| key | type | default | meaning |
|---|---|---|---|
| `output_file_name` | string | `"solution.vtu"` | output VTU file (single file, written in parallel) |
| `finite_element_degree` | int (≥1) | `1` | `FE_Q` polynomial degree |
| `n_global_refinements` | int | `0` | global mesh refinements |
| `rhs_expression` | string | `"1"` | right-hand side `f` (muparser) |
| `dirichlet_boundary_ids` | int list (string) | `"0"` | boundary ids with Dirichlet BC |
| `dirichlet_expression` | string | `"0"` | Dirichlet value (muparser) |
| `neumann_boundary_ids` | int list (string) | `""` | boundary ids with Neumann BC |
| `neumann_expression` | string | `"0"` | Neumann flux (muparser) |
| `mesh` | object | generator | mesh source, see below |
| `linear_solver` | object | iterative | solver settings, see below |

Output is written with `write_vtu_in_parallel`: a single `.vtu` file written
collectively by all ranks, identical in name and format to the serial program.
(For very large rank counts, a one-file-per-rank `.pvtu` record scales better.)

## `mesh` (mutually exclusive sources)

`mesh.source` selects exactly one path; only its keys are read.

```jsonc
// generator
"mesh": { "source": "generator",
          "grid_generator_function": "hyper_cube",      // default
          "grid_generator_arguments": "0 : 1 : true" }  // default

// file (file_name required)
"mesh": { "source": "file", "file_name": "mesh.msh" }
```

The mesh is built directly as a `parallel::distributed::Triangulation`. File
meshes are read by `GridIn` as a coarse mesh and then partitioned across ranks
(it does not ingest an already-distributed/refined mesh).

## `linear_solver`

```jsonc
"linear_solver": { "type": "iterative",   // default for poisson_mpi; or "direct"
                   "tolerance": 1e-8,      // iterative only
                   "max_iterations": 0 }   // iterative only; 0 = n_dofs
```

- `"iterative"` → CG + algebraic multigrid (scales to many ranks).
- `"direct"` → parallel direct solver (PETSc MUMPS / Trilinos Amesos);
  exact up to round-off, so `tolerance`/`max_iterations` are ignored.

> `poisson_mpi` defaults to `iterative`, `poisson` defaults to `direct`, but both
> honor an explicit `type`, so a file is interchangeable between the two.

## Example

This is the shipped `parameters_generator.json`; it is identical to the one in
`poisson/`. With `type` omitted, `poisson_mpi` solves it iteratively and
`poisson` directly.

```json
{
  "output_file_name": "solution.vtu",
  "finite_element_degree": 2,
  "n_global_refinements": 8,
  "mesh": { "source": "generator",
            "grid_generator_function": "hyper_cube",
            "grid_generator_arguments": "0 : 1 : true" },
  "rhs_expression": "1",
  "dirichlet_boundary_ids": "0, 1, 2, 3",
  "dirichlet_expression": "0",
  "neumann_boundary_ids": "",
  "neumann_expression": "0",
  "linear_solver": { "tolerance": 1e-8, "max_iterations": 0 }
}
```
