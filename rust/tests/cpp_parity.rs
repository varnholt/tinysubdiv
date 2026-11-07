use tinysubdiv::{subdivide, Mesh};
#[test]
fn matches_cpp_positions_and_indices() {
    let mesh = Mesh {
        positions: vec![
            [1., 1., 1.].into(),
            [1., -1., -1.].into(),
            [-1., 1., -1.].into(),
            [-1., -1., 1.].into(),
        ],
        triangles: vec![[0, 1, 2], [0, 3, 1], [0, 2, 3], [1, 3, 2]],
    };
    let output = subdivide(&mesh, 2).unwrap();
    let mut words = include_str!("data/cpp_reference.txt").split_whitespace();
    let vertices: usize = words.next().unwrap().parse().unwrap();
    let triangles: usize = words.next().unwrap().parse().unwrap();
    assert_eq!(output.positions.len(), vertices);
    assert_eq!(output.triangles.len(), triangles);
    for (i, p) in output.positions.iter().enumerate() {
        for actual in <[f32; 3]>::from(*p) {
            let expected: f32 = words.next().unwrap().parse().unwrap();
            assert!(
                (actual - expected).abs() < 1e-6,
                "vertex {i}: {actual} != {expected}"
            );
        }
    }
    for triangle in &output.triangles {
        for actual in triangle {
            let expected: u32 = words.next().unwrap().parse().unwrap();
            assert_eq!(*actual, expected);
        }
    }
    assert!(words.next().is_none());
}
