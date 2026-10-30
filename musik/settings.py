"""Persistent player preferences and a fixed, seeded set of gradient themes."""
import colorsys
import random

from PySide6.QtCore import QSettings, QSize
from PySide6.QtGui import QColor, QIcon, QLinearGradient, QPainter, QPixmap
from PySide6.QtWidgets import (
    QComboBox, QDialog, QDialogButtonBox, QFormLayout, QLabel, QSpinBox, QVBoxLayout,
)


def make_themes():
    rng = random.Random(20261030)
    themes = [("MusiK", ("#381754", "#632b60", "#cf713e", "#ffc3a3"))]
    names = ("Aurora", "Ember", "Lagoon", "Velvet", "Neon", "Dusk", "Ocean",
             "Orchid", "Copper", "Forest", "Midnight", "Rose", "Glacier",
             "Solar", "Plum", "Jade", "Cosmic", "Amber", "Electric")
    for name in names:
        hue = rng.random()
        shift = rng.uniform(0.08, 0.30)
        def color(h, s, v):
            return "#" + "".join(f"{round(c * 255):02x}" for c in colorsys.hsv_to_rgb(h % 1, s, v))
        themes.append((name, (color(hue, .72, .25), color(hue + shift, .68, .42),
                              color(hue + shift * 2, .65, .64), color(hue + shift, .30, 1))))
    return tuple(themes)


THEMES = make_themes()
ORDERS = ("Manual / import order", "Title A–Z", "Title Z–A", "Folder / filename", "Shuffled")
EFFECTS = ("Chrome sphere", "PsyTunnel", "Hiss", "Matrixia",
           "Laser Nexus", "Orbit Streams", "Glyph Waves", "Glyph Spin", "Glyph Trails")
DEFAULTS = {"scope_points": 512, "spectrum_bands": 48, "order": 0, "theme": 0, "effect": 0, "laser_bars": 4}
LIMITS = {"scope_points": (64, 2048), "spectrum_bands": (16, 128), "order": (0, 4),
          "theme": (0, 19), "effect": (0, len(EFFECTS) - 1), "laser_bars": (4, 8)}


def load_settings(store):
    values = {}
    for key, default in DEFAULTS.items():
        try:
            value = int(store.value(key, default))
        except (ValueError, TypeError):
            value = default
        low, high = LIMITS[key]
        values[key] = max(low, min(high, value))
    values["laser_bars"] = 8 if values["laser_bars"] == 8 else 4
    return values


def default_store():
    return QSettings("MusiK", "MusiK")


class SettingsDialog(QDialog):
    def __init__(self, values, parent=None):
        super().__init__(parent)
        self.setWindowTitle("MusiK · Settings")
        self.setMinimumWidth(470)
        layout = QVBoxLayout(self)
        form = QFormLayout()
        self.scope_points = QSpinBox()
        self.scope_points.setRange(*LIMITS["scope_points"])
        self.scope_points.setSingleStep(64)
        self.scope_points.setValue(values["scope_points"])
        self.scope_points.setSuffix(" points")
        self.scope_points.setToolTip("Number of samples drawn across the oscilloscope")
        form.addRow("Oscilloscope density", self.scope_points)
        self.spectrum_bands = QSpinBox()
        self.spectrum_bands.setRange(*LIMITS["spectrum_bands"])
        self.spectrum_bands.setSingleStep(8)
        self.spectrum_bands.setValue(values["spectrum_bands"])
        self.spectrum_bands.setSuffix(" bands")
        form.addRow("Spectrum density", self.spectrum_bands)
        self.order = QComboBox()
        self.order.addItems(ORDERS)
        self.order.setCurrentIndex(values["order"])
        form.addRow("Playlist order", self.order)
        self.theme = QComboBox()
        self.theme.setIconSize(QSize(100, 22))
        for name, colors in THEMES:
            preview = QPixmap(100, 22)
            painter = QPainter(preview)
            gradient = QLinearGradient(0, 0, 100, 22)
            for stop, color in zip((0, .46, 1), colors):
                gradient.setColorAt(stop, QColor(color))
            painter.fillRect(preview.rect(), gradient)
            painter.end()
            self.theme.addItem(QIcon(preview), name)
        self.theme.setCurrentIndex(values["theme"])
        form.addRow("Player theme", self.theme)
        self.effect = QComboBox()
        self.effect.addItems(EFFECTS)
        self.effect.setCurrentIndex(values["effect"])
        form.addRow("Visualization", self.effect)
        self.laser_bars = QComboBox()
        self.laser_bars.addItem("Every 4 bars", 4)
        self.laser_bars.addItem("Every 8 bars", 8)
        self.laser_bars.setCurrentIndex(1 if values["laser_bars"] == 8 else 0)
        self.laser_bars.setToolTip("Estimated 4/4 phrases from bass onsets; timing depends on the music")
        form.addRow("Laser pattern changes", self.laser_bars)
        layout.addLayout(form)
        hint = QLabel("Settings are remembered. In Matrixia, click the visualization to change colour.\n"
                      "PsyTunnel: bass flashes white, quiet passages can reverse the motion.")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def values(self):
        return {"scope_points": self.scope_points.value(), "spectrum_bands": self.spectrum_bands.value(),
                "order": self.order.currentIndex(), "theme": self.theme.currentIndex(),
                "effect": self.effect.currentIndex(), "laser_bars": self.laser_bars.currentData()}
