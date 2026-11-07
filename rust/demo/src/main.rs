mod platform;
mod shapes;
mod viewer;
use eframe::egui;
use glam::Vec3;
use tinysubdiv::{edges, subdivide, vertex_normals, Mesh};

type PendingImport = std::sync::Arc<std::sync::Mutex<Option<Result<String, String>>>>;
struct App {
    pending: PendingImport,
    viewer: viewer::Viewer,
    cage: Mesh,
    smooth: Mesh,
    shape: usize,
    levels: u32,
    show_cage: bool,
    wireframe: bool,
    color: [f32; 3],
    elapsed_ms: f32,
    error: String,
    loaded: bool,
}
impl App {
    fn new(
        cc: &eframe::CreationContext<'_>,
    ) -> Result<Self, Box<dyn std::error::Error + Send + Sync>> {
        platform::theme(&cc.egui_ctx);
        let shape = platform::option("shape")
            .and_then(|v| {
                shapes::NAMES
                    .iter()
                    .position(|name| name.eq_ignore_ascii_case(&v))
            })
            .unwrap_or(3);
        let levels = platform::option("level")
            .and_then(|s| s.parse().ok())
            .unwrap_or(3)
            .min(5);
        let mut app = Self {
            pending: Default::default(),
            viewer: viewer::Viewer::new(cc)?,
            cage: shapes::builtin(shape),
            smooth: Mesh::default(),
            shape,
            levels,
            show_cage: true,
            wireframe: false,
            color: [0.21, 0.72, 0.68],
            elapsed_ms: 0.,
            error: String::new(),
            loaded: false,
        };
        app.rebuild();
        #[cfg(not(target_arch = "wasm32"))]
        if let Some(path) = platform::option("obj") {
            match std::fs::read_to_string(path) {
                Ok(text) => {
                    if app.import(&text) {
                        app.rebuild();
                    }
                }
                Err(e) => app.error = e.to_string(),
            }
        }
        Ok(app)
    }
    fn import(&mut self, text: &str) -> bool {
        match shapes::parse_obj(text) {
            Ok(mesh) => {
                self.cage = mesh;
                self.loaded = true;
                self.levels = self.levels.min(2);
                self.error.clear();
                true
            }
            Err(e) => {
                self.error = e;
                false
            }
        }
    }
    fn rebuild(&mut self) {
        if self
            .cage
            .triangles
            .len()
            .checked_mul(4usize.pow(self.levels))
            .is_none_or(|n| n > 400_000)
        {
            self.error = "Choose a lower level: this would exceed 400,000 triangles.".into();
            return;
        }
        let start = platform::now();
        // Use the browser performance clock or a native monotonic clock.
        match subdivide(&self.cage, self.levels) {
            Ok(mesh) => self.smooth = mesh,
            Err(e) => {
                self.error = e.to_string();
                return;
            }
        }
        self.elapsed_ms = (platform::now() - start) as f32;
        self.error.clear();
        self.geometry();
    }
    fn geometry(&mut self) {
        let mut g = viewer::Geometry::default();
        g.grid(8, -1.5, 0.5);
        let normals = vertex_normals(&self.smooth).unwrap_or_default();
        let p: Vec<_> = self
            .smooth
            .positions
            .iter()
            .map(|v| Vec3::from_array((*v).into()))
            .collect();
        for &[a, b, c] in &self.smooth.triangles {
            let ids = [a as usize, b as usize, c as usize];
            g.smooth_triangle(
                ids.map(|i| p[i]),
                ids.map(|i| Vec3::from_array(normals[i].into())),
                self.color,
            );
        }
        if self.wireframe {
            for [a, b] in edges(&self.smooth).unwrap_or_default() {
                g.line(
                    p[a as usize] * 1.001,
                    p[b as usize] * 1.001,
                    [0.06, 0.17, 0.20],
                );
            }
        }
        if self.show_cage {
            for [a, b] in edges(&self.cage).unwrap_or_default() {
                let a = Vec3::from_array(self.cage.positions[a as usize].into());
                let b = Vec3::from_array(self.cage.positions[b as usize].into());
                g.line(a, b, [1., 0.68, 0.31]);
            }
        }
        self.viewer.set_geometry(g);
    }
}
impl eframe::App for App {
    fn ui(&mut self, ui: &mut egui::Ui, _: &mut eframe::Frame) {
        let ctx = ui.ctx().clone();
        let mut rebuild = false;
        let mut redraw = false;
        let dropped = ctx.input(|i| i.raw.dropped_files.clone());
        for file in dropped {
            #[cfg(not(target_arch = "wasm32"))]
            {
                *self.pending.lock().unwrap() = Some(file.bytes().and_then(obj_text));
            }
            #[cfg(target_arch = "wasm32")]
            {
                let pending = self.pending.clone();
                let ctx = ctx.clone();
                wasm_bindgen_futures::spawn_local(async move {
                    let result = file.bytes_async().await.and_then(obj_text);
                    *pending.lock().unwrap() = Some(result);
                    ctx.request_repaint();
                });
            }
        }
        let imported = self.pending.lock().unwrap().take();
        if let Some(result) = imported {
            match result {
                Ok(text) => {
                    rebuild = self.import(&text);
                }
                Err(e) => self.error = e,
            }
        }
        platform::controls(ui, |ui| {
            platform::heading(ui, "tinysubdiv", "Loop subdivision for triangle meshes");
            ui.label("CONTROL CAGE");
            let before = self.shape;
            let mut chosen = false;
            egui::ComboBox::from_id_salt("shape")
                .selected_text(if self.loaded {
                    "Imported OBJ"
                } else {
                    shapes::NAMES[self.shape]
                })
                .show_ui(ui, |ui| {
                    for (i, name) in shapes::NAMES.iter().enumerate() {
                        chosen |= ui.selectable_value(&mut self.shape, i, *name).clicked();
                    }
                });
            if self.shape != before || (self.loaded && chosen) {
                self.cage = shapes::builtin(self.shape);
                self.loaded = false;
                self.error.clear();
                rebuild = true;
            }
            rebuild |= ui
                .add(egui::Slider::new(&mut self.levels, 0..=5).text("Level"))
                .changed();
            ui.separator();
            ui.label("DISPLAY");
            redraw |= ui.checkbox(&mut self.show_cage, "Control cage").changed();
            redraw |= ui
                .checkbox(&mut self.wireframe, "Surface wireframe")
                .changed();
            ui.checkbox(&mut self.viewer.orbit, "Auto orbit");
            redraw |= ui.color_edit_button_rgb(&mut self.color).changed();
            if ui.button("Reset camera").clicked() {
                self.viewer.camera = viewer::Camera::default();
            }
            ui.separator();
            ui.label(format!("{} vertices", self.smooth.positions.len()));
            ui.label(format!("{} triangles", self.smooth.triangles.len()));
            ui.label(format!("Subdivision: {:.2} ms", self.elapsed_ms));
            ui.separator();
            ui.small(
                "Drop an OBJ file to load your own mesh. UV seams are welded before subdivision.",
            );
            if ui.button("Export OBJ").clicked() {
                if let Err(e) = export_obj(&shapes::obj(&self.smooth)) {
                    self.error = e;
                }
            }
            ui.small("Drag: orbit  |  Right drag: pan\nScroll: zoom");
            if !self.error.is_empty() {
                ui.colored_label(egui::Color32::LIGHT_RED, &self.error);
            }
            ui.hyperlink_to(
                "Library & source",
                "https://github.com/varnholt/tinysubdiv/tree/master/rust",
            );
        });
        if rebuild {
            self.rebuild();
        } else if redraw {
            self.geometry();
        }
        egui::CentralPanel::default().show(ui, |ui| {
            self.viewer.show(ui);
        });
    }
    fn on_exit(&mut self, gl: Option<&eframe::glow::Context>) {
        if let Some(gl) = gl {
            self.viewer.destroy(gl);
        }
    }
}
#[cfg(not(target_arch = "wasm32"))]
fn export_obj(text: &str) -> Result<(), String> {
    std::fs::write("subdivided.obj", text).map_err(|e| e.to_string())
}
#[cfg(target_arch = "wasm32")]
fn export_obj(text: &str) -> Result<(), String> {
    use wasm_bindgen::JsCast;
    let parts = js_sys::Array::new();
    parts.push(&wasm_bindgen::JsValue::from_str(text));
    let blob = web_sys::Blob::new_with_str_sequence(&parts).map_err(|e| format!("{e:?}"))?;
    let url = web_sys::Url::create_object_url_with_blob(&blob).map_err(|e| format!("{e:?}"))?;
    let document = web_sys::window().unwrap().document().unwrap();
    let anchor = document
        .create_element("a")
        .unwrap()
        .dyn_into::<web_sys::HtmlAnchorElement>()
        .unwrap();
    anchor.set_href(&url);
    anchor.set_download("subdivided.obj");
    anchor.click();
    let _ = web_sys::Url::revoke_object_url(&url);
    Ok(())
}
#[cfg(not(target_arch = "wasm32"))]
fn main() -> eframe::Result {
    platform::run(
        "tinysubdiv  |  Rust demo",
        Box::new(|cc| Ok(Box::new(App::new(cc)?))),
    )
}
#[cfg(target_arch = "wasm32")]
fn main() {
    platform::run("tinysubdiv", Box::new(|cc| Ok(Box::new(App::new(cc)?))));
}

fn obj_text(bytes: Vec<u8>) -> Result<String, String> {
    if bytes.len() > 20_000_000 {
        return Err("OBJ exceeds 20 MB.".into());
    }
    String::from_utf8(bytes).map_err(|e| e.to_string())
}
