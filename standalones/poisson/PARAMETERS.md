# `poisson` — JSON parameters

Run with: `./poisson parameters.json`

The parameter file is a single JSON object, read with deal.II's
`ParameterAcceptor` / `ParameterHandler` (no third-party JSON library).
**All keys are optional**; the defaults below are used when a key is missing.
Two things follow from using `ParameterHandler`:

- **Unknown keys are rejected** (a typo'd key aborts the run with a clear
  message), rather than being silently ignored.
- If the file **does not exist**, a ready-to-run example (identical to the
  shipped `parameters_generator.json`) is written to that path and the program
  exits gracefully with a message — so a missing file is self-documenting. With
  no argument at all, the default path is `parameters_generator.json`.

Boundary-id lists are a **comma-separated string** (e.g. `"0, 1, 2, 3"`), not a
JSON array; an empty list is `""`. The schema is shared with `poisson_mpi`, so
the same file works for both programs.

| key | type | default | meaning |
|---|---|---|---|
| `output_file_name` | string | `"solution.vtu"` | output VTU file |
| `finite_element_degree` | int (≥1) | `1` | `FE_Q` polynomial degree |
| `n_global_refinements` | int | `0` | global mesh refinements |
| `rhs_expression` | string | `"1"` | right-hand side `f` (muparser) |
| `dirichlet_boundary_ids` | int list (string) | `"0"` | boundary ids with Dirichlet BC |
| `dirichlet_expression` | string | `"0"` | Dirichlet value (muparser) |
| `neumann_boundary_ids` | int list (string) | `""` | boundary ids with Neumann BC |
| `neumann_expression` | string | `"0"` | Neumann flux (muparser) |
| `mesh` | object | generator | mesh source, see below |
| `linear_solver` | object | direct | solver settings, see below |

## `mesh` (mutually exclusive sources)

`mesh.source` selects exactly one path; only its keys are read.

```jsonc
// generator
"mesh": { "source": "generator",
          "grid_generator_function": "hyper_cube",      // default
          "grid_generator_arguments": "0 : 1 : true" }  // default

// file (file_name required)
"mesh": { "source": "file", "file_name": "mesh.vtu" }
```

The generator forwards to `GridGenerator::generate_from_name_and_arguments`;
file meshes are read by `GridIn` (format from extension: `.msh`, `.vtu`, …).

## `linear_solver`

```jsonc
"linear_solver": { "type": "direct",      // default for poisson; or "iterative"
                   "tolerance": 1e-8,      // iterative only
                   "max_iterations": 0 }   // iterative only; 0 = n_dofs
```

- `"direct"` → UMFPACK (exact up to round-off; `tolerance`/`max_iterations` ignored).
- `"iterative"` → CG + SSOR.

> `poisson` defaults to `direct`, `poisson_mpi` defaults to `iterative`, but both
> honor an explicit `type`, so a file is interchangeable between the two.

## Example

This is the shipped `parameters_generator.json`; it is identical to the one in
`poisson_mpi/`. With `type` omitted, `poisson` solves it directly and
`poisson_mpi` iteratively.

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
