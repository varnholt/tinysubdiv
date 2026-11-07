use tinysubdiv::{Mesh, Vec3};
pub const NAMES: [&str; 5] = ["Cube", "Open box", "Tetrahedron", "Torus", "Letter T"];
fn quad(mesh: &mut Mesh, [a, b, c, d]: [u32; 4]) {
    mesh.triangles.extend([[a, b, c], [a, c, d]]);
}
pub fn builtin(index: usize) -> Mesh {
    match index {
        2 => Mesh {
            positions: vec![
                [1., 1., 1.].into(),
                [1., -1., -1.].into(),
                [-1., 1., -1.].into(),
                [-1., -1., 1.].into(),
            ],
            triangles: vec![[0, 1, 2], [0, 3, 1], [0, 2, 3], [1, 3, 2]],
        },
        3 => {
            let mut mesh = Mesh::default();
            let rings = 8u32;
            let sides = 4u32;
            for r in 0..rings {
                for s in 0..sides {
                    let u = std::f32::consts::TAU * r as f32 / rings as f32;
                    let v = std::f32::consts::TAU * (s as f32 + 0.5) / sides as f32;
                    let radius = 1.2 + 0.5 * v.cos();
                    mesh.positions
                        .push([radius * u.cos(), 0.5 * v.sin(), radius * u.sin()].into());
                }
            }
            for r in 0..rings {
                for s in 0..sides {
                    let at = |r, s| (r % rings) * sides + s % sides;
                    quad(
                        &mut mesh,
                        [at(r, s), at(r, s + 1), at(r + 1, s + 1), at(r + 1, s)],
                    );
                }
            }
            mesh
        }
        4 => {
            let outline = [
                [-1.2, 1.],
                [-0.3, 1.],
                [0.3, 1.],
                [1.2, 1.],
                [1.2, 0.4],
                [0.3, 0.4],
                [0.3, -1.2],
                [-0.3, -1.2],
                [-0.3, 0.4],
                [-1.2, 0.4],
            ];
            let mut mesh = Mesh::default();
            for z in [0.3, -0.3] {
                for [x, y] in outline {
                    mesh.positions.push([x, y, z].into());
                }
            }
            for [a, b, c, d] in [[9, 8, 1, 0], [8, 5, 2, 1], [5, 4, 3, 2], [7, 6, 5, 8]] {
                quad(&mut mesh, [a, b, c, d]);
                quad(&mut mesh, [d + 10, c + 10, b + 10, a + 10]);
            }
            for i in 0..10 {
                let j = (i + 1) % 10;
                quad(&mut mesh, [i, j, j + 10, i + 10]);
            }
            mesh
        }
        _ => {
            let mut mesh = Mesh {
                positions: vec![
                    [-1., -1., -1.].into(),
                    [1., -1., -1.].into(),
                    [1., -1., 1.].into(),
                    [-1., -1., 1.].into(),
                    [-1., 1., -1.].into(),
                    [1., 1., -1.].into(),
                    [1., 1., 1.].into(),
                    [-1., 1., 1.].into(),
                ],
                triangles: Vec::new(),
            };
            for face in [
                [0, 1, 2, 3],
                [3, 2, 6, 7],
                [1, 0, 4, 5],
                [0, 3, 7, 4],
                [2, 1, 5, 6],
            ] {
                quad(&mut mesh, face);
            }
            if index == 0 {
                quad(&mut mesh, [4, 7, 6, 5]);
            }
            mesh
        }
    }
}
pub fn parse_obj(text: &str) -> Result<Mesh, String> {
    let mut mesh = Mesh::default();
    for (line_no, line) in text.lines().enumerate() {
        let mut words = line.split('#').next().unwrap_or("").split_whitespace();
        match words.next() {
            Some("v") => {
                let mut p = [0.; 3];
                for v in &mut p {
                    *v = words
                        .next()
                        .ok_or("Incomplete vertex")?
                        .parse::<f32>()
                        .map_err(|_| format!("Invalid vertex on line {}", line_no + 1))?;
                }
                mesh.positions.push(p.into());
                if mesh.positions.len() > 200_000 {
                    return Err("OBJ exceeds 200,000 vertices".into());
                }
            }
            Some("f") => {
                let mut ids = Vec::new();
                for w in words {
                    let id = w
                        .split('/')
                        .next()
                        .unwrap_or("")
                        .parse::<i64>()
                        .map_err(|_| format!("Invalid face on line {}", line_no + 1))?;
                    let index = if id < 0 {
                        mesh.positions.len() as i64 + id
                    } else {
                        id - 1
                    };
                    if index < 0 || index >= mesh.positions.len() as i64 {
                        return Err("Face references a missing vertex".into());
                    }
                    ids.push(index as u32);
                }
                if ids.len() < 3 {
                    return Err("A face needs at least three vertices".into());
                }
                for k in 1..ids.len() - 1 {
                    mesh.triangles.push([ids[0], ids[k], ids[k + 1]]);
                }
                if mesh.triangles.len() > 200_000 {
                    return Err("OBJ exceeds 200,000 triangles".into());
                }
            }
            _ => {}
        }
    }
    if mesh.triangles.is_empty() {
        return Err("OBJ has no faces".into());
    }
    let mut mesh = tinysubdiv::weld(&mesh, 0.).map_err(|e| e.to_string())?;
    if mesh.positions.is_empty() {
        return Err("OBJ has no nondegenerate faces".into());
    }
    let (mut min, mut max) = ([f32::INFINITY; 3], [f32::NEG_INFINITY; 3]);
    for p in &mesh.positions {
        for (i, v) in <[f32; 3]>::from(*p).into_iter().enumerate() {
            min[i] = min[i].min(v);
            max[i] = max[i].max(v);
        }
    }
    let extent = (0..3)
        .map(|i| max[i] as f64 - min[i] as f64)
        .fold(0_f64, f64::max);
    let scale = if extent > 0. { 2.8 / extent } else { 1. };
    for p in &mut mesh.positions {
        let a: [f32; 3] = (*p).into();
        *p = Vec3::from(std::array::from_fn(|i| {
            ((a[i] as f64 - (min[i] as f64 + max[i] as f64) * 0.5) * scale) as f32
        }));
    }
    Ok(mesh)
}
pub fn obj(mesh: &Mesh) -> String {
    use std::fmt::Write;
    let mut out = String::from("# tinysubdiv mesh\n");
    for p in &mesh.positions {
        writeln!(&mut out, "v {} {} {}", p.x, p.y, p.z).unwrap();
    }
    for [a, b, c] in &mesh.triangles {
        writeln!(&mut out, "f {} {} {}", a + 1, b + 1, c + 1).unwrap();
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn all_shapes_subdivide() {
        for i in 0..5 {
            let m = builtin(i);
            tinysubdiv::validate(&m).unwrap();
            assert_eq!(
                tinysubdiv::subdivide(&m, 2).unwrap().triangles.len(),
                m.triangles.len() * 16
            );
        }
    }
    #[test]
    fn obj_handles_quads_negative_indices_and_errors() {
        let m = parse_obj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf -4/1 -3/2 -2/3 -1/4").unwrap();
        assert_eq!(m.triangles.len(), 2);
        assert!(parse_obj("v 0 0 0\nf 1 2 3").is_err());
        assert!(parse_obj("v NaN 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3").is_err());
    }
}
