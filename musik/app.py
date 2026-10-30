import os
from pathlib import Path
import sys

# AudioBufferOutput requires Qt's FFmpeg backend.
os.environ.setdefault("QT_MEDIA_BACKEND", "ffmpeg")

import numpy as np
from PySide6.QtCore import Qt, QRectF, QTimer, QUrl
from PySide6.QtGui import QColor, QFont, QKeySequence, QPainter, QPainterPath, QPen, QShortcut, QSurfaceFormat
from PySide6.QtMultimedia import QAudioBufferOutput, QAudioOutput, QMediaPlayer
from PySide6.QtWidgets import (
    QAbstractItemView, QApplication, QFileDialog, QFrame, QHBoxLayout, QLabel,
    QListWidget, QListWidgetItem, QMainWindow, QMessageBox, QPushButton,
    QSlider, QStyle, QVBoxLayout, QWidget,
)

from .audio import AudioAnalysis
from .library import AUDIO_FILE_FILTER, FolderScanner
from .playlist import load_playlist, save_playlist
from .visualizer import Visualizer


STYLE = """
QMainWindow { background: #24132c; }
QWidget#canvas {
 background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
 stop:0 #381754, stop:0.46 #632b60, stop:1 #cf713e);
}
QWidget { color: #fff4ed; font-family: 'Noto Sans', sans-serif; font-size: 13px; }
QLabel { background: transparent; }
QLabel#brand { font-size: 42px; font-weight: 800; letter-spacing: -2px; }
QLabel#eyebrow { color: #e8c1d6; font-size: 11px; letter-spacing: 2px; }
QLabel#track { font-size: 22px; font-weight: 600; }
QLabel#muted { color: #e4c7d6; }
QFrame#panel { background: rgba(23, 13, 35, 190); border: 1px solid rgba(255,255,255,28); border-radius: 18px; }
QPushButton { background: rgba(255,255,255,17); border: 1px solid rgba(255,255,255,35); border-radius: 9px; padding: 10px 14px; }
QPushButton:hover { background: rgba(255,255,255,38); border-color: #f7ae89; }
QPushButton:pressed { background: #7d436f; }
QPushButton:disabled { color: #947f92; background: rgba(255,255,255,6); border-color: transparent; }
QPushButton#primary { background: #f7ac81; color: #28152e; font-weight: 700; }
QPushButton#visualize { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #9a5de0,stop:1 #ef9a65); color: white; font-weight: 700; padding: 13px 22px; }
QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item { padding: 13px 10px; border-bottom: 1px solid rgba(255,255,255,12); border-radius: 5px; }
QListWidget::item:selected { background: rgba(196,131,224,70); }
QListWidget::item:hover { background: rgba(255,255,255,18); }
QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: rgba(255,255,255,35); }
QSlider::sub-page:horizontal { background: #f4ad88; border-radius: 2px; }
QSlider::handle:horizontal { background: #ffe1cc; width: 12px; margin: -4px 0; border-radius: 6px; }
QStatusBar { background: #25162e; color: #d9bcd1; font-size: 11px; }
QToolTip { color: #fff4ed; background: #38213e; border: 1px solid #8e5e91; }
"""


def timestamp(ms):
    seconds = max(0, ms // 1000)
    return f"{seconds // 60}:{seconds % 60:02d}"


class Oscilloscope(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(115)
        self.spectrum_mode = False
        self.spectrum = np.zeros(48)
        self.spectrum_max = 20000.0
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setToolTip("Click to switch between oscilloscope and spectrum analyzer")
        self.samples = np.zeros(512)
        self.pending = self.samples
        self.timer = QTimer(self)
        self.timer.setInterval(33)
        self.timer.timeout.connect(self.tick)
        self.timer.start()

    def set_samples(self, samples):
        self.pending = samples

    def set_spectrum(self, levels, maximum):
        self.spectrum = levels
        self.spectrum_max = maximum

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self.spectrum_mode = not self.spectrum_mode
            self.update()
            event.accept()
        else:
            super().mousePressEvent(event)

    def tick(self):
        self.samples = self.pending
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), QColor("#20152e"))
        painter.setPen(QPen(QColor(255, 220, 240, 14), 1))
        for x in range(0, self.width(), 36):
            painter.drawLine(x, 0, x, self.height())
        for y in range(0, self.height(), 28):
            painter.drawLine(0, y, self.width(), y)
        painter.setPen(QColor("#e4c7d6"))
        painter.drawText(10, 16, "SPECTRUM · click for waveform" if self.spectrum_mode
                         else "OSCILLOSCOPE · click for spectrum")
        if self.spectrum_mode:
            left, top, bottom = 10.0, 26.0, self.height() - 22.0
            width = max(1.0, self.width() - 20.0)
            step = width / len(self.spectrum)
            for i, level in enumerate(self.spectrum):
                height = float(np.clip(level, 0, 1)) * max(1, bottom - top)
                painter.fillRect(QRectF(left + i * step, bottom - height,
                                        max(1, step - 2), height), QColor("#ffc3a3"))
            painter.setPen(QColor("#e4c7d6"))
            for frequency in (30, 100, 1000, 10000):
                if frequency >= self.spectrum_max:
                    continue
                x = left + width * np.log(frequency / 30) / np.log(self.spectrum_max / 30)
                label = f"{frequency // 1000}k" if frequency >= 1000 else str(frequency)
                painter.drawText(int(x), self.height() - 5, label)
            painter.drawText(self.width() - 62, 16, "−72–0 dB")
            painter.end()
            return
        count = min(len(self.samples), max(2, self.width()))
        points = self.samples[np.linspace(0, len(self.samples) - 1, count).astype(int)]
        path = QPainterPath()
        for i, sample in enumerate(points):
            x = i * self.width() / max(1, count - 1)
            y = self.height() / 2 - float(np.clip(sample, -1, 1)) * self.height() * 0.43
            path.moveTo(x, y) if i == 0 else path.lineTo(x, y)
        painter.setPen(QPen(QColor(242, 154, 197, 40), 7))
        painter.drawPath(path)
        painter.setPen(QPen(QColor("#ffc3a3"), 1.6))
        painter.drawPath(path)
        painter.end()


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("MusiK")
        self.resize(900, 820)
        self.setMinimumSize(720, 670)
        self.setStyleSheet(STYLE)
        self.current_item = None
        self.visualizer = None
        self.folder_scanner = None
        self.closing = False
        self.player = QMediaPlayer(self)
        self.output = QAudioOutput(self)
        self.output.setVolume(0.7)
        self.player.setAudioOutput(self.output)
        self.buffer_output = QAudioBufferOutput(self)
        self.player.setAudioBufferOutput(self.buffer_output)
        self.analysis = AudioAnalysis(self)
        self.buffer_output.audioBufferReceived.connect(self.analysis.receive)
        self.build_ui()
        self.analysis.waveform.connect(self.scope.set_samples)
        self.analysis.spectrum.connect(self.scope.set_spectrum)
        self.player.positionChanged.connect(self.position_changed)
        self.player.durationChanged.connect(self.duration_changed)
        self.player.seekableChanged.connect(self.update_controls)
        self.player.playbackStateChanged.connect(self.state_changed)
        self.player.mediaStatusChanged.connect(self.media_status_changed)
        self.player.errorOccurred.connect(self.player_error)
        self.playlist.itemDoubleClicked.connect(self.play_item)
        self.playlist.itemSelectionChanged.connect(self.update_controls)
        self.seek.sliderReleased.connect(self.seek_to)
        QShortcut(QKeySequence("Space"), self, activated=self.toggle_play)
        QShortcut(QKeySequence("Delete"), self.playlist, activated=self.remove_files)
        QShortcut(QKeySequence("Ctrl+O"), self, activated=self.add_files)
        self.statusBar().showMessage("Add your music to begin · Space: play / pause · Ctrl+O: add files")
        self.update_controls()

    def build_ui(self):
        canvas = QWidget(objectName="canvas")
        self.setCentralWidget(canvas)
        layout = QVBoxLayout(canvas)
        layout.setContentsMargins(30, 22, 30, 24)
        layout.setSpacing(18)
        header = QHBoxLayout()
        title = QVBoxLayout()
        title.setSpacing(0)
        title.addWidget(QLabel("MusiK", objectName="brand"))
        title.addWidget(QLabel("SOUND INTO COLOUR", objectName="eyebrow"))
        header.addLayout(title)
        header.addStretch()
        self.visualize_button = QPushButton("✦  Visualization", objectName="visualize")
        self.visualize_button.clicked.connect(self.show_visualizer)
        header.addWidget(self.visualize_button)
        layout.addLayout(header)

        panel = QFrame(objectName="panel")
        box = QVBoxLayout(panel)
        box.setContentsMargins(22, 18, 22, 18)
        box.setSpacing(12)
        row = QHBoxLayout()
        row.addWidget(QLabel("NOW PLAYING", objectName="eyebrow"))
        row.addStretch()
        self.state_label = QLabel("STOPPED", objectName="eyebrow")
        row.addWidget(self.state_label)
        box.addLayout(row)
        self.track_label = QLabel("Your next favourite sound", objectName="track")
        self.track_label.setWordWrap(True)
        box.addWidget(self.track_label)
        self.scope = Oscilloscope()
        box.addWidget(self.scope)
        timeline = QHBoxLayout()
        self.elapsed = QLabel("0:00", objectName="muted")
        self.duration = QLabel("0:00", objectName="muted")
        self.seek = QSlider(Qt.Orientation.Horizontal)
        self.seek.setRange(0, 0)
        self.seek.setAccessibleName("Playback position")
        timeline.addWidget(self.elapsed)
        timeline.addWidget(self.seek, 1)
        timeline.addWidget(self.duration)
        box.addLayout(timeline)
        controls = QHBoxLayout()
        self.previous_button = self.transport_button("Previous", QStyle.StandardPixmap.SP_MediaSkipBackward, self.previous)
        self.play_button = self.transport_button("Play", QStyle.StandardPixmap.SP_MediaPlay, self.toggle_play)
        self.play_button.setObjectName("primary")
        self.next_button = self.transport_button("Next", QStyle.StandardPixmap.SP_MediaSkipForward, self.next)
        self.stop_button = self.transport_button("Stop", QStyle.StandardPixmap.SP_MediaStop, self.stop)
        for button in (self.previous_button, self.play_button, self.next_button, self.stop_button):
            controls.addWidget(button)
        controls.addStretch()
        controls.addWidget(QLabel("Volume", objectName="muted"))
        volume = QSlider(Qt.Orientation.Horizontal)
        volume.setRange(0, 100)
        volume.setValue(70)
        volume.setFixedWidth(85)
        volume.setAccessibleName("Volume")
        volume.valueChanged.connect(lambda value: self.output.setVolume(value / 100))
        controls.addWidget(volume)
        box.addLayout(controls)
        layout.addWidget(panel)

        playlist_panel = QFrame(objectName="panel")
        box = QVBoxLayout(playlist_panel)
        box.setContentsMargins(20, 18, 20, 16)
        row = QHBoxLayout()
        row.addWidget(QLabel("PLAYLIST", objectName="eyebrow"))
        row.addStretch()
        self.count_label = QLabel("0 tracks", objectName="muted")
        row.addWidget(self.count_label)
        box.addLayout(row)
        self.playlist = QListWidget()
        self.playlist.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.playlist.setDragDropMode(QAbstractItemView.DragDropMode.InternalMove)
        self.playlist.setDefaultDropAction(Qt.DropAction.MoveAction)
        self.playlist.setAccessibleName("Playlist — double-click a track to play")
        self.playlist.setMinimumHeight(130)
        box.addWidget(self.playlist, 1)
        self.empty_label = QLabel("A little silence before the music.\nAdd audio files with + to build your playlist.", objectName="muted")
        self.empty_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        box.addWidget(self.empty_label)
        row = QHBoxLayout()
        for text, callback in (("+  Add files", self.add_files), ("Add folder", self.add_folder),
                               ("−  Remove files", self.remove_files),
                               ("Load playlist", self.load), ("Save playlist", self.save)):
            button = QPushButton(text)
            button.clicked.connect(callback)
            row.addWidget(button)
            if text == "Add folder":
                self.add_folder_button = button
                button.setToolTip("Add audio files from a folder and its subfolders")
            if text.startswith("−"):
                self.remove_button = button
            if text.startswith("Save"):
                self.save_button = button
        box.addLayout(row)
        layout.addWidget(playlist_panel, 1)

    def transport_button(self, text, icon, callback):
        button = QPushButton(self.style().standardIcon(icon), text)
        button.clicked.connect(callback)
        return button

    def add_paths(self, paths, report_missing=True):
        missing = []
        added = 0
        for value in paths:
            path = Path(value).expanduser().resolve()
            if not path.is_file():
                missing.append(str(value))
                continue
            item = QListWidgetItem(path.stem)
            item.setData(Qt.ItemDataRole.UserRole, str(path))
            item.setToolTip(str(path))
            self.playlist.addItem(item)
            added += 1
        if self.playlist.currentRow() < 0 and self.playlist.count():
            self.playlist.setCurrentRow(0)
        self.update_controls()
        if missing and report_missing:
            QMessageBox.warning(self, "Files unavailable", "Could not find:\n" + "\n".join(missing[:10]))
        return added

    def add_files(self):
        paths, _ = QFileDialog.getOpenFileNames(self, "Add music", "", AUDIO_FILE_FILTER)
        self.add_paths(paths)

    def add_folder(self):
        if self.folder_scanner is not None:
            self.folder_scanner.requestInterruption()
            self.add_folder_button.setEnabled(False)
            return
        folder = QFileDialog.getExistingDirectory(self, "Add music folder (including subfolders)")
        if not folder:
            return
        self.folder_added = 0
        self.folder_errors = 0
        self.folder_scanner = FolderScanner(folder, self)
        self.folder_scanner.files_found.connect(self.folder_files_found)
        self.folder_scanner.scan_error.connect(self.folder_scan_error)
        self.folder_scanner.finished.connect(self.folder_scan_finished)
        self.add_folder_button.setText("Cancel scan")
        self.statusBar().showMessage("Scanning music folder and subfolders…")
        self.folder_scanner.start()

    def folder_files_found(self, paths):
        if self.closing:
            return
        self.folder_added += self.add_paths(paths, report_missing=False)
        self.statusBar().showMessage(f"Scanning music folder… {self.folder_added} tracks added")

    def folder_scan_error(self, message):
        self.folder_errors += 1

    def folder_scan_finished(self):
        cancelled = self.folder_scanner.isInterruptionRequested()
        self.folder_scanner.deleteLater()
        self.folder_scanner = None
        self.add_folder_button.setText("Add folder")
        self.add_folder_button.setEnabled(True)
        result = "Scan cancelled" if cancelled else "Folder scan complete"
        detail = f" · {self.folder_errors} unreadable folders/files skipped" if self.folder_errors else ""
        self.statusBar().showMessage(f"{result} · {self.folder_added} tracks added{detail}")
        if self.closing:
            self.close()

    def remove_files(self):
        selected = self.playlist.selectedItems()
        if self.current_item in selected:
            self.stop()
            self.player.setSource(QUrl())
            self.current_item = None
            self.track_label.setText("Your next favourite sound")
        for item in selected:
            self.playlist.takeItem(self.playlist.row(item))
        self.update_controls()

    def load(self):
        filename, _ = QFileDialog.getOpenFileName(self, "Load playlist", "", "Playlists (*.m3u8 *.m3u)")
        if not filename:
            return
        try:
            tracks, missing = load_playlist(filename)
        except (OSError, UnicodeError) as exc:
            QMessageBox.warning(self, "Cannot load playlist", str(exc))
            return
        self.stop()
        self.player.setSource(QUrl())
        self.current_item = None
        self.playlist.clear()
        self.track_label.setText("Your next favourite sound")
        self.add_paths(tracks)
        if missing:
            QMessageBox.warning(self, "Skipped playlist entries", "Missing files or unsupported network URLs:\n" + "\n".join(missing[:10]))

    def save(self):
        filename, _ = QFileDialog.getSaveFileName(self, "Save playlist", "MusiK.m3u8", "UTF-8 playlist (*.m3u8);;M3U playlist (*.m3u)")
        if filename:
            if Path(filename).suffix.lower() not in (".m3u8", ".m3u"):
                filename += ".m3u8"
            try:
                save_playlist(filename, [self.playlist.item(i).data(Qt.ItemDataRole.UserRole) for i in range(self.playlist.count())])
            except (OSError, ValueError) as exc:
                QMessageBox.warning(self, "Cannot save playlist", str(exc))

    def play_item(self, item):
        if item is None:
            return
        if item is not self.current_item:
            if self.current_item is not None:
                self.current_item.setFont(QFont())
            self.player.stop()
            self.analysis.reset()
            self.current_item = item
            font = QFont()
            font.setBold(True)
            item.setFont(font)
            self.player.setSource(QUrl.fromLocalFile(item.data(Qt.ItemDataRole.UserRole)))
            self.track_label.setText(item.text())
            self.track_label.setToolTip(item.data(Qt.ItemDataRole.UserRole))
        self.playlist.setCurrentItem(item)
        self.player.play()

    def play(self):
        selected = self.playlist.currentItem()
        self.play_item(selected if selected is not None else self.current_item)

    def toggle_play(self):
        if self.player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            self.player.pause()
        elif self.player.playbackState() == QMediaPlayer.PlaybackState.PausedState:
            self.player.play()
        else:
            self.play()

    def stop(self):
        self.player.stop()
        self.player.setPosition(0)
        self.analysis.reset()
        if self.visualizer:
            self.visualizer.reset()

    def next(self):
        if not self.playlist.count():
            return
        row = self.playlist.row(self.current_item) if self.current_item is not None else -1
        self.play_item(self.playlist.item((row + 1) % self.playlist.count()))

    def previous(self):
        if self.player.position() > 3000:
            self.player.setPosition(0)
            self.analysis.reset()
        elif self.playlist.count():
            row = self.playlist.row(self.current_item) if self.current_item is not None else 1
            self.play_item(self.playlist.item((row - 1) % self.playlist.count()))

    def media_status_changed(self, status):
        if status == QMediaPlayer.MediaStatus.EndOfMedia:
            row = self.playlist.row(self.current_item) if self.current_item is not None else -1
            if 0 <= row < self.playlist.count() - 1:
                self.play_item(self.playlist.item(row + 1))
            else:
                self.stop()
        self.update_controls()

    def player_error(self, error, message):
        if error != QMediaPlayer.Error.NoError:
            self.statusBar().showMessage(f"Playback error: {message}")
            self.analysis.reset()

    def state_changed(self, state):
        self.analysis.active = state == QMediaPlayer.PlaybackState.PlayingState
        self.state_label.setText({QMediaPlayer.PlaybackState.PlayingState: "PLAYING",
                                  QMediaPlayer.PlaybackState.PausedState: "PAUSED",
                                  QMediaPlayer.PlaybackState.StoppedState: "STOPPED"}[state])
        if not self.analysis.active:
            self.analysis.reset()
            if self.visualizer:
                self.visualizer.reset()
        self.update_controls()

    def update_controls(self):
        count = self.playlist.count()
        self.count_label.setText(f"{count} track{'s' if count != 1 else ''}")
        self.empty_label.setVisible(count == 0)
        self.playlist.setVisible(count > 0)
        self.play_button.setEnabled(count > 0)
        self.previous_button.setEnabled(count > 0)
        self.next_button.setEnabled(count > 0)
        self.save_button.setEnabled(count > 0)
        self.remove_button.setEnabled(bool(self.playlist.selectedItems()))
        playing = self.player.playbackState() == QMediaPlayer.PlaybackState.PlayingState
        self.play_button.setText("Pause" if playing else "Play")
        self.play_button.setIcon(self.style().standardIcon(
            QStyle.StandardPixmap.SP_MediaPause if playing else QStyle.StandardPixmap.SP_MediaPlay
        ))
        self.stop_button.setEnabled(self.player.playbackState() != QMediaPlayer.PlaybackState.StoppedState)
        self.seek.setEnabled(self.player.isSeekable())

    def position_changed(self, position):
        self.elapsed.setText(timestamp(position))
        if not self.seek.isSliderDown():
            self.seek.setValue(position)

    def duration_changed(self, duration):
        self.seek.setRange(0, duration)
        self.duration.setText(timestamp(duration))

    def seek_to(self):
        self.analysis.reset()
        if self.visualizer:
            self.visualizer.reset()
        self.player.setPosition(self.seek.value())

    def show_visualizer(self):
        if self.visualizer is None:
            self.visualizer = Visualizer(self)
            self.analysis.beat.connect(self.visualizer.on_beat)
            self.analysis.bands.connect(self.visualizer.on_bands)
            self.visualizer.failed.connect(lambda message: self.statusBar().showMessage(message))
        self.visualizer.show()
        self.visualizer.raise_()
        self.visualizer.activateWindow()
        self.statusBar().showMessage("Visualization · F11: fullscreen · Esc: back / close · ↑↓: fog · ←→: speed · R: reset")

    def closeEvent(self, event):
        self.player.stop()
        if self.visualizer:
            self.visualizer.close()
        if self.folder_scanner is not None:
            self.closing = True
            self.folder_scanner.requestInterruption()
            self.hide()
            event.ignore()
            return
        super().closeEvent(event)


def main():
    fmt = QSurfaceFormat()
    fmt.setVersion(3, 3)
    fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
    fmt.setSwapInterval(1)
    QSurfaceFormat.setDefaultFormat(fmt)
    app = QApplication(sys.argv)
    app.setApplicationName("MusiK")
    app.setOrganizationName("MusiK")
    window = MainWindow()
    window.add_paths(sys.argv[1:])
    window.show()
    sys.exit(app.exec())
