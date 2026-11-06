"""Run: QT_QPA_PLATFORM=xcb uv run python -m tools.benchmark_attractor_pack.

Measures 250k particles at 1280x720 (1100x619 internal density). CPU reference
excludes glow/upload, whereas GPU timing includes every render pass and a final
completion fence. This intentionally gives the old CPU implementation an advantage.
"""
import argparse
import time

import moderngl
from PySide6.QtGui import QGuiApplication, QSurfaceFormat, QOpenGLContext, QOffscreenSurface

from musik.attractor_pack import AttractorPackScene, NAMES
from musik.attractor_pack_renderer import AttractorPackRenderer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=int, default=90)
    args = parser.parse_args()
    app = QGuiApplication([])
    fmt = QSurfaceFormat()
    fmt.setVersion(3, 3)
    fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
    context = QOpenGLContext()
    context.setFormat(fmt)
    if not context.create():
        raise RuntimeError('OpenGL context creation failed')
    surface = QOffscreenSurface()
    surface.setFormat(context.format())
    surface.create()
    if not context.makeCurrent(surface):
        raise RuntimeError('Cannot make benchmark context current')
    ctx = moderngl.create_context(require=330)
    target = ctx.simple_framebuffer((1280, 720))
    frames = max(1, args.frames)
    print('GPU:', ctx.info['GL_RENDERER'], flush=True)
    print('mode | CPU reference wall ms/frame | GPU wall ms/frame | GPU process CPU ms/frame', flush=True)
    try:
        for mode, name in enumerate(NAMES):
            cpu = AttractorPackScene()
            cpu.select(mode)
            cpu.auto_switch = False
            for _ in range(10):
                cpu.render(1/60, 1280, 720, .8, .6, .5)
            start = time.perf_counter()
            for _ in range(frames):
                cpu.render(1/60, 1280, 720, .8, .6, .5)
            cpu_ms = (time.perf_counter()-start)*1000/frames
            scene = AttractorPackScene(gpu=True)
            scene.select(mode)
            scene.auto_switch = False
            renderer = AttractorPackRenderer(ctx)
            try:
                for _ in range(30):
                    renderer.draw(target, (1280,720), scene, 1/60, .8, .6, .5)
                ctx.finish()
                start, cpu_start = time.perf_counter(), time.process_time()
                for _ in range(frames):
                    renderer.draw(target, (1280,720), scene, 1/60, .8, .6, .5)
                ctx.finish()  # benchmark only; the interactive renderer never waits
                gpu_ms = (time.perf_counter()-start)*1000/frames
                submit_ms = (time.process_time()-cpu_start)*1000/frames
                print(f'{name}: {cpu_ms:.2f} | {gpu_ms:.2f} | {submit_ms:.2f}', flush=True)
            finally:
                renderer.release()
    finally:
        target.release()
        ctx.release()
        context.doneCurrent()


if __name__ == '__main__':
    main()
