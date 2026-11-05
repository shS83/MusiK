"""CPU-only fog_shS resource and waveform sanity checks."""
import ast
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def _resample():
    tree = ast.parse((ROOT / "musik" / "fog_renderer.py").read_text())
    node = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "resample_waveform")
    scope = {"np": np, "AUDIO_SAMPLES": 512}
    exec(compile(ast.Module(body=[node], type_ignores=[]), "<fog_waveform>", "exec"), scope)
    return scope["resample_waveform"]


def test_fog_waveform_bounds():
    resample = _resample()
    for input_samples in ([], [0.8], [1., -1.], np.linspace(-2, 2, 8000), [float('nan'), float('inf')]):
        output = resample(input_samples)
        assert output.shape == (512,)
        assert output.dtype == np.float32
        assert np.isfinite(output).all()
        assert np.max(np.abs(output)) <= 1


def test_fog_mesh_is_prebaked_and_finite():
    with np.load(ROOT / "musik" / "fog_mesh.npz", allow_pickle=False) as saved:
        mesh = saved["mesh"]
    assert mesh.shape == (19068, 10)
    assert len(mesh) % 3 == 0
    assert np.isfinite(mesh).all()


def test_fog_effect_appended_without_changing_prior_indices():
    settings = (ROOT / "musik" / "settings.py").read_text()
    app = (ROOT / "musik" / "app.py").read_text()
    assert '"Fireworks", "Circler Tetra", "fog_shS"' in settings
    assert 'FogShSVisualizer(self)' in app
    assert 'waveform.connect(self.visualizer.on_waveform)' in app
