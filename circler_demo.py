"""Test Circler v1 separately before integrating with MusiK.

SPACE/B: simulate a bass hit; A: toggle a 125-BPM test beat; ESC: quit.
"""
from __future__ import annotations
import pygame as pg
import moderngl
from musik.circler_reactive import CirclerScene, CirclerRenderer


def main():
    pg.init()
    ctx = None
    renderer = None
    try:
        pg.display.gl_set_attribute(pg.GL_CONTEXT_MAJOR_VERSION, 3)
        pg.display.gl_set_attribute(pg.GL_CONTEXT_MINOR_VERSION, 3)
        pg.display.gl_set_attribute(pg.GL_CONTEXT_PROFILE_MASK, pg.GL_CONTEXT_PROFILE_CORE)
        pg.display.gl_set_attribute(pg.GL_DEPTH_SIZE, 24)
        pg.display.set_mode((1280, 720), pg.OPENGL | pg.DOUBLEBUF | pg.RESIZABLE)
        pg.display.set_caption("MusiK · Circler Tetra prototype | Space=bass | A=auto beat")
        ctx = moderngl.create_context(require=330)
        renderer = CirclerRenderer(ctx)
        scene = CirclerScene()
        clock = pg.time.Clock()
        auto_beat = False
        auto_timer = 0.0
        running = True
        while running:
            dt = min(clock.tick(60) / 1000, .10)
            for e in pg.event.get():
                if e.type == pg.QUIT or (e.type == pg.KEYDOWN and e.key == pg.K_ESCAPE):
                    running = False
                elif e.type == pg.KEYDOWN and e.key in (pg.K_SPACE, pg.K_b):
                    scene.beat()
                elif e.type == pg.KEYDOWN and e.key == pg.K_a:
                    auto_beat = not auto_beat
            pg.display.set_caption(f"MusiK · Circler Tetra v3 | beat {scene.total_beats} | next palette in {16 - scene.total_beats % 16} hits | A=auto")
            w, h = pg.display.get_window_size()
            if w < 1 or h < 1:
                continue
            scene.update(dt, w, h)
            if auto_beat:
                auto_timer += dt
                if auto_timer >= 60 / 125:
                    auto_timer -= 60 / 125
                    scene.beat()
            else:
                auto_timer = 0.0
            renderer.draw(scene, w, h)
            pg.display.flip()
    finally:
        if renderer:
            renderer.release()
        if ctx:
            ctx.release()
        pg.quit()


if __name__ == "__main__":
    main()
