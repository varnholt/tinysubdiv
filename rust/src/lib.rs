//! Loop subdivision for indexed triangle meshes, with no dependencies.
//!
//! ```
//! use tinysubdiv::{Mesh, subdivide, vertex_normals};
//! let cage = Mesh {
//!     positions: vec![[0., 0., 0.].into(), [4., 0., 0.].into(), [0., 4., 0.].into()],
//!     triangles: vec![[0, 1, 2]],
//! };
//! let smooth = subdivide(&cage, 2)?;
//! assert_eq!(smooth.triangles.len(), 16);
//! let normals = vertex_normals(&smooth)?;
//! # Ok::<(), tinysubdiv::Error>(())
//! ```

use std::collections::HashMap;
use std::fmt;
use std::ops::{Add, Mul, Sub};

/// A tightly packed position or direction. Convert to/from `[f32; 3]`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}
impl From<[f32; 3]> for Vec3 {
    fn from([x, y, z]: [f32; 3]) -> Self {
        Self { x, y, z }
    }
}
impl From<Vec3> for [f32; 3] {
    fn from(v: Vec3) -> Self {
        [v.x, v.y, v.z]
    }
}
impl Add for Vec3 {
    type Output = Self;
    fn add(self, b: Self) -> Self {
        [self.x + b.x, self.y + b.y, self.z + b.z].into()
    }
}
impl Sub for Vec3 {
    type Output = Self;
    fn sub(self, b: Self) -> Self {
        [self.x - b.x, self.y - b.y, self.z - b.z].into()
    }
}
impl Mul<f32> for Vec3 {
    type Output = Self;
    fn mul(self, s: f32) -> Self {
        [self.x * s, self.y * s, self.z * s].into()
    }
}
pub fn dot(a: Vec3, b: Vec3) -> f32 {
    a.x * b.x + a.y * b.y + a.z * b.z
}
pub fn cross(a: Vec3, b: Vec3) -> Vec3 {
    [
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    ]
    .into()
}
pub type Triangle = [u32; 3];

/// Positions and consistently wound triangle indices. Subdivision interpolates positions only.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Mesh {
    pub positions: Vec<Vec3>,
    pub triangles: Vec<Triangle>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    TooManyVertices,
    MissingVertex { triangle: usize },
    DegenerateTriangle { triangle: usize },
    NonFinitePosition { vertex: usize },
    InvalidEpsilon,
    CapacityExceeded,
}
impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{self:?}")
    }
}
impl std::error::Error for Error {}

/// Validate indices, distinct triangle vertices, and finite positions.
pub fn validate(mesh: &Mesh) -> Result<(), Error> {
    if mesh.positions.len() > u32::MAX as usize {
        return Err(Error::TooManyVertices);
    }
    for (vertex, p) in mesh.positions.iter().enumerate() {
        if !p.x.is_finite() || !p.y.is_finite() || !p.z.is_finite() {
            return Err(Error::NonFinitePosition { vertex });
        }
    }
    for (triangle, &[a, b, c]) in mesh.triangles.iter().enumerate() {
        if [a, b, c]
            .iter()
            .any(|&i| i as usize >= mesh.positions.len())
        {
            return Err(Error::MissingVertex { triangle });
        }
        if a == b || b == c || c == a {
            return Err(Error::DegenerateTriangle { triangle });
        }
    }
    Ok(())
}

struct Edge {
    a: usize,
    b: usize,
    opposite: [usize; 2],
    count: usize,
}
fn topology(mesh: &Mesh) -> (Vec<Edge>, Vec<[usize; 3]>) {
    let mut edges: Vec<Edge> = Vec::new();
    let mut lookup = HashMap::new();
    let mut triangles = Vec::with_capacity(mesh.triangles.len());
    for t in &mesh.triangles {
        let mut indices = [0; 3];
        for k in 0..3 {
            let a = t[k] as usize;
            let b = t[(k + 1) % 3] as usize;
            let apex = t[(k + 2) % 3] as usize;
            let index = *lookup.entry((a.min(b), a.max(b))).or_insert_with(|| {
                let index = edges.len();
                edges.push(Edge {
                    a,
                    b,
                    opposite: [apex; 2],
                    count: 0,
                });
                index
            });
            let e = &mut edges[index];
            if e.count == 1 {
                e.opposite[1] = apex;
            }
            e.count += 1;
            indices[k] = index;
        }
        triangles.push(indices);
    }
    (edges, triangles)
}

/// Apply Loop subdivision. Boundaries and non-manifold edges are creases.
/// Each level quadruples the triangle count. Zero levels returns a validated copy.
pub fn subdivide(mesh: &Mesh, levels: u32) -> Result<Mesh, Error> {
    validate(mesh)?;
    let mut result = mesh.clone();
    for _ in 0..levels {
        if result.triangles.is_empty() {
            break;
        }
        result = loop_once(&result)?;
    }
    Ok(result)
}
/// Alias matching the C++ operation's name.
pub fn r#loop(mesh: &Mesh, levels: u32) -> Result<Mesh, Error> {
    subdivide(mesh, levels)
}

fn loop_once(mesh: &Mesh) -> Result<Mesh, Error> {
    let n = mesh.positions.len();
    let (edges, triangle_edges) = topology(mesh);
    let size = n
        .checked_add(edges.len())
        .filter(|&s| s <= u32::MAX as usize)
        .ok_or(Error::TooManyVertices)?;
    let mut sums = vec![Vec3::default(); n];
    let mut sharp_sums = sums.clone();
    let mut valence = vec![0usize; n];
    let mut sharp = valence.clone();
    for e in &edges {
        for (a, b) in [(e.a, e.b), (e.b, e.a)] {
            sums[a] = sums[a] + mesh.positions[b];
            valence[a] += 1;
            if e.count != 2 {
                sharp_sums[a] = sharp_sums[a] + mesh.positions[b];
                sharp[a] += 1;
            }
        }
    }
    let mut out = Mesh::default();
    out.positions
        .try_reserve_exact(size)
        .map_err(|_| Error::CapacityExceeded)?;
    let faces = mesh
        .triangles
        .len()
        .checked_mul(4)
        .ok_or(Error::CapacityExceeded)?;
    out.triangles
        .try_reserve_exact(faces)
        .map_err(|_| Error::CapacityExceeded)?;
    for (i, &p) in mesh.positions.iter().enumerate() {
        let p = if valence[i] == 0 || (sharp[i] != 0 && sharp[i] != 2) {
            p
        } else if sharp[i] == 2 {
            p * 0.75 + sharp_sums[i] * 0.125
        } else {
            let v = valence[i] as f64;
            let c = 0.375 + 0.25 * (2.0 * std::f64::consts::PI / v).cos();
            let beta = ((0.625 - c * c) / v) as f32;
            p * (1.0 - valence[i] as f32 * beta) + sums[i] * beta
        };
        out.positions.push(p);
    }
    for e in edges {
        let ab = mesh.positions[e.a] + mesh.positions[e.b];
        out.positions.push(if e.count != 2 {
            ab * 0.5
        } else {
            ab * 0.375 + (mesh.positions[e.opposite[0]] + mesh.positions[e.opposite[1]]) * 0.125
        });
    }
    for (&[a, b, c], &[x, y, z]) in mesh.triangles.iter().zip(&triangle_edges) {
        let [x, y, z] = [x, y, z].map(|e| (n + e) as u32);
        out.triangles
            .extend([[a, x, z], [b, y, x], [c, z, y], [x, y, z]]);
    }
    Ok(out)
}

/// Merge positions within `epsilon`, drop collapsed faces and unused vertices.
/// Zero uses exact equality, treating signed zero as equal. Negative/nonfinite epsilon is rejected.
pub fn weld(mesh: &Mesh, epsilon: f32) -> Result<Mesh, Error> {
    validate(mesh)?;
    if !epsilon.is_finite() || epsilon < 0.0 {
        return Err(Error::InvalidEpsilon);
    }
    let mut unique: Vec<Vec3> = Vec::new();
    let mut remap = Vec::with_capacity(mesh.positions.len());
    let mut exact = HashMap::new();
    // f64 cells avoid overflow when dividing finite f32 positions by tiny tolerances.
    let mut grid: HashMap<[i64; 3], Vec<usize>> = HashMap::new();
    for &p in &mesh.positions {
        let id = if epsilon == 0.0 {
            let key = [p.x, p.y, p.z].map(|v| if v == 0.0 { 0 } else { v.to_bits() });
            *exact.entry(key).or_insert_with(|| {
                unique.push(p);
                unique.len() - 1
            })
        } else {
            let components = [p.x, p.y, p.z];
            let cell = components.map(|v| (f64::from(v) / f64::from(epsilon)).floor() as i64);
            let mut found = None;
            'search: for dx in -1i64..=1 {
                for dy in -1i64..=1 {
                    for dz in -1i64..=1 {
                        let key = [
                            cell[0].saturating_add(dx),
                            cell[1].saturating_add(dy),
                            cell[2].saturating_add(dz),
                        ];
                        if let Some(ids) = grid.get(&key) {
                            for &i in ids {
                                let a: [f32; 3] = unique[i].into();
                                let distance: f64 = (0..3)
                                    .map(|k| (f64::from(a[k]) - f64::from(components[k])).powi(2))
                                    .sum();
                                if distance <= f64::from(epsilon).powi(2) {
                                    found = Some(i);
                                    break 'search;
                                }
                            }
                        }
                    }
                }
            }
            found.unwrap_or_else(|| {
                let i = unique.len();
                unique.push(p);
                grid.entry(cell).or_default().push(i);
                i
            })
        };
        remap.push(id);
    }
    let mut out = Mesh::default();
    let mut compact = vec![None; unique.len()];
    for t in &mesh.triangles {
        let mapped = t.map(|i| remap[i as usize]);
        if mapped[0] == mapped[1] || mapped[1] == mapped[2] || mapped[2] == mapped[0] {
            continue;
        }
        let triangle = mapped.map(|i| {
            *compact[i].get_or_insert_with(|| {
                let id = out.positions.len() as u32;
                out.positions.push(unique[i]);
                id
            })
        });
        out.triangles.push(triangle);
    }
    Ok(out)
}
/// Area-weighted unit normals; zero for vertices without area.
pub fn vertex_normals(mesh: &Mesh) -> Result<Vec<Vec3>, Error> {
    validate(mesh)?;
    let mut normals = vec![Vec3::default(); mesh.positions.len()];
    for &[a, b, c] in &mesh.triangles {
        let p = mesh.positions[a as usize];
        let face = cross(
            mesh.positions[b as usize] - p,
            mesh.positions[c as usize] - p,
        );
        for i in [a, b, c] {
            normals[i as usize] = normals[i as usize] + face;
        }
    }
    for n in &mut normals {
        let length = dot(*n, *n).sqrt();
        if length > 0.0 {
            *n = *n * (1.0 / length);
        }
    }
    Ok(normals)
}
/// Unique edges in first-encounter order, suitable for wireframe rendering.
pub fn edges(mesh: &Mesh) -> Result<Vec<[u32; 2]>, Error> {
    validate(mesh)?;
    Ok(topology(mesh)
        .0
        .into_iter()
        .map(|e| [e.a as u32, e.b as u32])
        .collect())
}
