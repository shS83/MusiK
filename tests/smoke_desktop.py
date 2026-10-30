"""Run on a desktop: uv run python -m tests.smoke_desktop (muted audio)."""
from pathlib import Path
import tempfile
import time
import wave
from unittest.mock import patch

import numpy as np
from PySide6.QtCore import Qt, QSettings, QUrl
from PySide6.QtGui import QSurfaceFormat
from PySide6.QtMultimedia import QMediaPlayer
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from musik.app import MainWindow


def wait_until(predicate, timeout=5000):
    deadline = time.monotonic() + timeout / 1000
    while not predicate() and time.monotonic() < deadline:
        QTest.qWait(20)
    assert predicate(), "Timed out waiting for desktop state"


def main():
    fmt = QSurfaceFormat()
    fmt.setVersion(3, 3)
    fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
    QSurfaceFormat.setDefaultFormat(fmt)
    app = QApplication([])
    window = MainWindow(QSettings(str(Path.cwd() / "smoke-settings.ini"), QSettings.Format.IniFormat))
    window.output.setMuted(True)
    errors, beats, buffers = [], [], []
    window.player.errorOccurred.connect(lambda *args: errors.append(args))
    window.analysis.beat.connect(beats.append)
    window.analysis.waveform.connect(lambda samples: buffers.append(float(np.max(np.abs(samples)))))
    with tempfile.TemporaryDirectory() as folder:
        paths = []
        for name in ("Bass test", "Next track"):
            path = Path(folder) / (name + ".wav")
            t = np.arange(48000 * 8) / 48000
            phase = (t - 0.25) % 0.5
            samples = 0.7 * np.sin(2 * np.pi * 70 * t) * np.exp(-phase * 30)
            samples += 0.12 * np.sin(2 * np.pi * 700 * t) + 0.08 * np.sin(2 * np.pi * 6000 * t)
            with wave.open(str(path), "wb") as wav:
                wav.setnchannels(2)
                wav.setsampwidth(2)
                wav.setframerate(48000)
                wav.writeframes((np.column_stack((samples, -samples)) * 32767).astype('<i2').tobytes())
            paths.append(path)
        try:
            window.add_paths(paths)
            window.show()
            assert window.play_button.text() == "Play"
            window.play_button.click()
            wait_until(lambda: window.player.position() > 600)
            assert window.play_button.text() == "Pause"
            assert beats and max(buffers) > 0.1, (beats, buffers)
            assert max(window.scope.spectrum) > 0.1
            QTest.mouseClick(window.scope, Qt.MouseButton.LeftButton)
            assert window.scope.spectrum_mode
            QTest.mouseClick(window.scope, Qt.MouseButton.LeftButton)
            assert not window.scope.spectrum_mode
            window.show_visualizer()
            QTest.qWait(400)
            visual = window.visualizer
            assert visual.isValid() and not visual.error, visual.error
            assert min(visual.motion) > 0.1, visual.motion
            visual.grabFramebuffer().save(str(Path(folder) / 'musik-live-scene.png'))
            window.grab().save(str(Path(folder) / 'musik-live-window.png'))
            # Switch all audio-driven effects while the decoder is playing.
            for effect in (1, 2, 3, 0):
                window.apply_settings(dict(window.preferences, effect=effect))
                QTest.qWait(130)
                visual = window.visualizer
                assert visual.isValid() and not visual.error, visual.error
                if effect:
                    wait_until(lambda: visual.dynamics.energy > 0.05)
            window.play_button.click()
            QTest.qWait(100)
            assert window.play_button.text() == "Play"
            assert visual.motion == [0.0, 0.0]
            assert max(window.scope.spectrum) == 0
            paused = window.player.position()
            count = len(beats)
            QTest.qWait(150)
            assert window.player.position() == paused and len(beats) == count
            window.playlist.setCurrentRow(1)
            window.play_button.click()
            wait_until(lambda: window.player.position() > paused)
            assert window.play_button.text() == "Pause"
            assert window.current_item is window.playlist.item(0)
            window.next()
            wait_until(lambda: window.player.position() > 100)
            assert window.current_item is window.playlist.item(1)
            window.previous()
            assert window.current_item is window.playlist.item(0)
            # Wait for the new source to start before seeking near its end.
            wait_until(lambda: window.player.position() > 100)
            # Automatic next, then natural stop at the end of the list.
            wait_until(lambda: window.player.isSeekable())
            window.player.setPosition(7800)
            wait_until(lambda: window.current_item is window.playlist.item(1))
            wait_until(lambda: window.player.position() > 50)
            window.player.setPosition(7800)
            wait_until(lambda: window.player.playbackState() == QMediaPlayer.PlaybackState.StoppedState)
            assert window.play_button.text() == "Play"
            visual.resize(960, 540)
            QTest.qWait(100)
            QTest.keyClick(visual, Qt.Key.Key_F11)
            QTest.qWait(100)
            assert visual.borderless.active
            QTest.keyClick(visual, Qt.Key.Key_Escape)
            visual.close()
            assert not visual.timer.isActive()
            window.show_visualizer()
            QTest.qWait(100)
            assert visual.timer.isActive() and visual.isValid()
            window.play()
            window.remove_files()
            assert window.current_item is None
            assert window.player.source().isEmpty()
            # Import through the actual folder button, including nested/uppercase files.
            library = Path(folder) / "Music library"
            album = library / "Album"
            album.mkdir(parents=True)
            for name in ("01.MP3", "02.ogg", "03.flac"):
                (album / name).touch()
            (library / "cover.jpg").touch()
            before = window.playlist.count()
            with patch('musik.app.choose_folders', return_value=[str(library), str(album)]):
                window.add_folder_button.click()
            wait_until(lambda: window.folder_scanner is None)
            assert window.playlist.count() == before + 3
            assert window.add_folder_button.text() == "Add folders"
            with patch('musik.app.choose_folders', return_value=[]):
                window.add_folder_button.click()
            assert window.playlist.count() == before + 3
            assert not errors, errors
            print(f'Desktop smoke passed: playback, {len(beats)} bass beats, pause/resume, skip, auto-next/end, removal, folder import, OpenGL, resize, fullscreen, reopen.')
        finally:
            window.player.stop()
            window.player.setSource(QUrl())
            window.close()
            app.processEvents()


if __name__ == '__main__':
    main()
