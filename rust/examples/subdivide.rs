use tinysubdiv::{subdivide, Mesh};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mesh = Mesh {
        positions: vec![
            [0., 0., 0.].into(),
            [1., 0., 0.].into(),
            [0., 1., 0.].into(),
        ],
        triangles: vec![[0, 1, 2]],
    };
    let smooth = subdivide(&mesh, 3)?;
    println!(
        "{} vertices, {} triangles",
        smooth.positions.len(),
        smooth.triangles.len()
    );
    Ok(())
}
