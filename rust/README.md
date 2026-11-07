# tinysubdiv

**[Interactive Rust demo](https://varnholt.github.io/tinysubdiv/rust/)** — [run it natively or build for the browser](https://github.com/varnholt/tinysubdiv/blob/rust-v0.1.1/rust/demo/README.md).

Native Rust Loop subdivision for indexed triangle meshes. No dependencies, no C++ toolchain, and no unsafe code. MIT licensed. Requires Rust 1.85 or newer.

```rust
use tinysubdiv::{subdivide, weld, Mesh};

let mesh = Mesh {
    positions: vec![[0., 0., 0.].into(), [4., 0., 0.].into(), [0., 4., 0.].into()],
    triangles: vec![[0, 1, 2]],
};
let welded = weld(&mesh, 0.0)?;
let smooth = subdivide(&welded, 3)?;
# Ok::<(), tinysubdiv::Error>(())
```

## Use

```toml
[dependencies]
tinysubdiv = "0.1"
```

The crate lives alongside the C++ implementation in `rust/`. To use a local checkout, replace the version with `{ path = "/path/to/tinysubdiv/rust" }`.

[crates.io](https://crates.io/crates/tinysubdiv) | [API documentation](https://docs.rs/tinysubdiv)

## API

| Operation | Behavior |
|---|---|
| `subdivide(&mesh, levels)` / `r#loop(&mesh, levels)` | Loop subdivision; each level quadruples the triangle count |
| `weld(&mesh, epsilon)` | Merge duplicate or nearby positions; remove collapsed faces and unused vertices |
| `vertex_normals(&mesh)` | Area-weighted unit normals |
| `edges(&mesh)` | Unique edges in encounter order |
| `validate(&mesh)` | Check indices, repeated triangle vertices, finite positions and index capacity |

All operations return `Result`. `Mesh` owns `Vec<Vec3>` positions and `Vec<[u32; 3]>` triangles. `Vec3` converts to and from `[f32; 3]`. Zero subdivision levels returns a validated copy.

The subdivision rules and output ordering follow the C++ header. Open boundaries and non-manifold edges are creases. UVs, normals and other per-corner attributes are not interpolated. Negative or nonfinite welding tolerances and nonfinite positions are rejected. Subdivision grows exponentially; use modest levels for large meshes.

## Check and run

Run from `rust/`:

```sh
cargo test
cargo run --example subdivide
cargo clippy --all-targets -- -D warnings
cargo package --allow-dirty
```

Tests include boundary weights, closed topology, non-manifold creases, welding and C++ reference positions/indices. To regenerate the reference samples from the current header, run `python tools/generate_reference.py` with a C++23 compiler (`CXX` selects it). `--check` verifies the stored samples on the same toolchain. Normal Cargo builds do not need C++.

CI checks native platforms, Rust 1.85, WebAssembly, packaging and freshly generated C++ reference samples.
