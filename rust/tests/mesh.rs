use tinysubdiv::*;
fn triangle() -> Mesh {
    Mesh {
        positions: vec![
            [0., 0., 0.].into(),
            [4., 0., 0.].into(),
            [0., 4., 0.].into(),
        ],
        triangles: vec![[0, 1, 2]],
    }
}
#[test]
fn boundary_weights_and_winding() {
    let m = subdivide(&triangle(), 1).unwrap();
    assert_eq!(m.positions.len(), 6);
    assert_eq!(m.triangles.len(), 4);
    assert_eq!(m.positions[0], [0.5, 0.5, 0.].into());
    assert_eq!(m.positions[3], [2., 0., 0.].into());
    for n in vertex_normals(&m).unwrap() {
        assert_eq!(n, [0., 0., 1.].into());
    }
    assert_eq!(subdivide(&triangle(), 0).unwrap(), triangle());
}
#[test]
fn closed_topology_and_symmetry() {
    let m = Mesh {
        positions: vec![
            [1., 1., 1.].into(),
            [1., -1., -1.].into(),
            [-1., 1., -1.].into(),
            [-1., -1., 1.].into(),
        ],
        triangles: vec![[0, 1, 2], [0, 3, 1], [0, 2, 3], [1, 3, 2]],
    };
    let s = subdivide(&m, 3).unwrap();
    assert_eq!(s.triangles.len(), 256);
    assert_eq!(
        s.positions.len() as isize - edges(&s).unwrap().len() as isize + s.triangles.len() as isize,
        2
    );
    let sum = s.positions.iter().fold(Vec3::default(), |a, &b| a + b);
    assert!(dot(sum, sum) < 1e-8);
}
#[test]
fn welding_seams_and_removing_collapsed_faces() {
    let mut m = triangle();
    m.positions
        .extend([Vec3::from([0., -0., 0.]), Vec3::from([0.0001, 0., 0.])]);
    m.triangles.extend([[3, 1, 2], [0, 4, 1]]);
    assert_eq!(weld(&m, 0.).unwrap().positions.len(), 4);
    let w = weld(&m, 0.001).unwrap();
    assert_eq!(w.positions.len(), 3);
    assert_eq!(w.triangles.len(), 2);
    assert_eq!(w.triangles[0], w.triangles[1]);
}
#[test]
fn invalid_input_returns_errors() {
    let mut m = triangle();
    m.triangles[0][0] = 99;
    assert!(matches!(subdivide(&m, 0), Err(Error::MissingVertex { .. })));
    m.triangles[0] = [0, 0, 1];
    assert!(matches!(
        validate(&m),
        Err(Error::DegenerateTriangle { .. })
    ));
    assert_eq!(weld(&triangle(), f32::NAN), Err(Error::InvalidEpsilon));
    let mut m = triangle();
    m.positions[0].x = f32::INFINITY;
    assert!(matches!(validate(&m), Err(Error::NonFinitePosition { .. })));
}
#[test]
fn nonmanifold_edge_stays_sharp() {
    let m = Mesh {
        positions: vec![
            [0., 0., 0.].into(),
            [2., 0., 0.].into(),
            [0., 2., 0.].into(),
            [0., -2., 0.].into(),
            [0., 0., 2.].into(),
        ],
        triangles: vec![[0, 1, 2], [1, 0, 3], [0, 1, 4]],
    };
    let out = subdivide(&m, 1).unwrap();
    assert_eq!(out.positions[5], [1., 0., 0.].into());
}
