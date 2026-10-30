"""Qt adaptations of the five Lasers/laser_convergence*.py sketches.

Preserves captured/live targets, rocking/full glyph rotation and fading trails.
Original source copyright Santeri Saarinen; see licenses/lasers-MIT.txt.
"""
import math
import random

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QPainterPath, QPixmap, QTransform


def edge_position(distance, width, height):
    margin = 10
    w, h = max(1, width - 20), max(1, height - 20)
    d = distance % (2 * (w + h))
    if d < w:
        return margin + d, margin
    if d < w + h:
        return width - margin, margin + d - w
    if d < 2 * w + h:
        return width - margin - (d - w - h), height - margin
    return margin, height - margin - (d - 2 * w - h)


class LaserScene:
    def __init__(self, variant):
        self.variant = variant
        self.rng = random.Random(8100 + variant)
        self.rays = (5, 15, 14, 15, 28)[variant]
        self.per_ray = (32, 48, 48, 72, 64)[variant]
        self.glyph = ("•", "•", "?", "O", "´")[variant]
        self.font_size = (12, 12, 27, 18, 30)[variant]
        self.spin = self.spin_target = (1., 1., 2.3, 4.3, 2.3)[variant]
        self.phase = self.orbit = self.distance = self.elapsed = self.emission = 0.0
        self.color = QColor("#e355ff")
        self.next_color = QColor("#45ddff")
        self.color_mix = 0.0
        self.cache = []
        self.cache_key = None
        self.trails = None
        self.size = None
        self.particles = []
        self.changes = 0

    def change_phrase(self, number=0):
        self.changes += 1
        counts = (5, 8, 12, 16) if self.variant < 2 else (8, 12, 16, 20, 28)
        self.rays = self.rng.choice([count for count in counts if count != self.rays])
        self.per_ray = self.rng.choice([count for count in (24, 36, 48, 64, 80) if count != self.per_ray])
        self.spin_target = self.rng.choice((-1, 1)) * self.rng.uniform(.7, 4.8)
        if self.variant >= 2:
            self.glyph = self.rng.choice([char for char in "|S?OX+*<>0123§Ω" if char != self.glyph])
            self.font_size = self.rng.choice((16, 20, 24, 28))
        self.color = self.current_color()
        self.next_color = QColor.fromHsv(self.rng.randrange(360), 190, 255)
        self.color_mix = 0.0

    def current_color(self):
        t = self.color_mix * self.color_mix * (3 - 2 * self.color_mix)
        return QColor(*(round(a + (b - a) * t) for a, b in zip(self.color.getRgb()[:3], self.next_color.getRgb()[:3])))

    def reset(self):
        self.particles.clear()
        self.trails = None
        self.emission = 0.0

    def build_cache(self):
        key = (self.glyph, self.font_size)
        if key == self.cache_key:
            return
        font = QFont("DejaVu Sans Mono")
        font.setPixelSize(self.font_size)
        path = QPainterPath()
        path.addText(0, 0, font, self.glyph)
        bounds = path.boundingRect()
        transform = QTransform().translate(-bounds.center().x(), -bounds.center().y())
        path = transform.map(path)
        side = math.ceil(math.hypot(bounds.width(), bounds.height())) + 6
        self.cache = []
        for step in range(72):
            glyph = QPixmap(side, side)
            glyph.fill(Qt.GlobalColor.transparent)
            painter = QPainter(glyph)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.translate(side / 2, side / 2)
            painter.rotate(step * 5)
            painter.fillPath(path, Qt.GlobalColor.white)
            painter.end()
            self.cache.append(glyph)
        self.cache_key = key

    def render(self, width, height, dt, energy, mid, treble, pulse):
        # Bound the CPU painter workload even on a high-DPI fullscreen display.
        scale = min(1.0, 1280 / max(1, width), 800 / max(1, height))
        width, height = max(80, round(width * scale)), max(60, round(height * scale))
        if self.size != (width, height):
            self.reset()
            self.size = (width, height)
        self.elapsed += dt
        self.spin += (self.spin_target - self.spin) * (1 - math.exp(-dt / .4))
        drive = .08 + 1.1 * energy + .45 * mid + .30 * treble
        self.phase += dt * self.spin * drive
        self.orbit += dt * self.spin * (.025 + .55 * energy + .25 * mid)
        self.distance += dt * (8 + 180 * energy + 60 * pulse)
        self.color_mix = min(1, self.color_mix + dt * .5)
        radius = min(width, height) * (.23 if self.variant == 2 else .085)
        target = (width / 2 + radius * math.cos(self.orbit), height / 2 + radius * math.sin(self.orbit))
        perimeter = 2 * (width - 20 + height - 20)
        layer = QPixmap(width, height)
        layer.fill(Qt.GlobalColor.transparent)
        painter = QPainter(layer)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        if self.variant < 2:
            self.emission += dt * (8 + 24 * energy + 16 * pulse)
            while self.emission >= 1:
                self.emission -= 1
                for ray in range(self.rays):
                    start = edge_position(ray * perimeter / self.rays + self.distance, width, height)
                    self.particles.append([start, target, 0.0])
            alive = []
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(Qt.GlobalColor.white)
            for particle in self.particles:
                particle[2] += dt * (.25 + .85 * energy + .25 * treble)
                if particle[2] >= 1:
                    continue
                start, captured, progress = particle
                end = captured if self.variant == 0 else target
                x = start[0] + (end[0] - start[0]) * progress
                y = start[1] + (end[1] - start[1]) * progress
                painter.drawEllipse(QPointF(x, y), 2.1 + pulse, 2.1 + pulse)
                alive.append(particle)
            self.particles = alive[-2000:]
        else:
            self.build_cache()
            for ray in range(self.rays):
                sx, sy = edge_position(ray * perimeter / self.rays + self.distance, width, height)
                base = math.atan2(target[1] - sy, target[0] - sx)
                for j in range(self.per_ray):
                    t = (j + .25) / (self.per_ray + .7)
                    x, y = sx + (target[0] - sx) * t, sy + (target[1] - sy) * t
                    wave = self.phase - j * .32 + ray * .24
                    angle = base + (math.sin(wave) * 1.05 if self.variant == 2 else wave)
                    glyph = self.cache[round(angle / math.tau * 72) % 72]
                    painter.drawPixmap(round(x - glyph.width() / 2), round(y - glyph.height() / 2), glyph)
        painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceIn)
        painter.fillRect(layer.rect(), self.current_color())
        painter.end()
        if self.variant in (0, 1, 4):
            if self.trails is None:
                self.trails = QPixmap(width, height)
                self.trails.fill(Qt.GlobalColor.transparent)
            painter = QPainter(self.trails)
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_DestinationIn)
            painter.fillRect(self.trails.rect(), QColor(255, 255, 255, round(255 * .5 ** (dt / .15))))
            if self.variant < 2:
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceIn)
                painter.fillRect(self.trails.rect(), self.current_color())
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
            painter.drawPixmap(0, 0, layer)
            painter.end()
            return self.trails
        return layer

    def paint(self, widget, dt, energy, mid, treble, pulse):
        frame = self.render(widget.width(), widget.height(), dt, energy, mid, treble, pulse)
        painter = QPainter(widget)
        painter.fillRect(widget.rect(), Qt.GlobalColor.black)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
        painter.drawPixmap(QRectF(widget.rect()), frame, QRectF(frame.rect()))
        # Flash is a final overlay, never baked into the trail feedback image.
        if pulse > .001:
            painter.fillRect(widget.rect(), QColor(255, 255, 255, round(190 * min(1, pulse))))
        painter.end()
