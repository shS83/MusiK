import tempfile
import unittest
import wave
from pathlib import Path

from PySide6.QtCore import QSettings, QUrl
from PySide6.QtMultimedia import QAudioFormat, QMediaDevices
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from musik.app import MainWindow
from musik.settings import SettingsDialog, load_settings


class OutputTests(unittest.TestCase):
    def test_formatted_playback(self):
        app = QApplication.instance() or QApplication([])
        if QMediaDevices.defaultAudioOutput().isNull():
            self.skipTest("A playback device is required")
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            store = QSettings(str(root/"prefs.ini"), QSettings.IniFormat)
            window = MainWindow(store)
            window.output.setMuted(True)
            warnings = []
            route = window.playback_output
            route.warning.connect(warnings.append)
            try:
                audio = root/"test.wav"
                with wave.open(str(audio),"wb") as out:
                    out.setparams((1, 2, 22050, 0, "NONE", ""))
                    out.writeframes(b"\x00\x10" * 22050 * 6)
                dialog = SettingsDialog(window.preferences, window)
                controls = dialog.audio_controls
                device = QMediaDevices.defaultAudioOutput()
                key = bytes(device.id()).hex()
                controls.device.setCurrentIndex(controls.device.findData(key))
                self.assertEqual(controls.values()["output_device"], key)
                for width in ("16", "24"):
                    idx = controls.width.findData(width)
                    if idx < 0:
                        self.skipTest("Device lacks the tested PCM format")
                    controls.width.setCurrentIndex(idx)
                    controls.rate.setCurrentIndex(controls.rate.findData(44100))
                    window.apply_settings(dialog.values())
                    self.assertEqual(load_settings(store), window.preferences)
                    if window.current_item is None:
                        window.add_paths([audio])
                        window.play()
                    for _ in range(100):
                        QTest.qWait(20)
                        if route.writer and route.sink.processedUSecs() > 0:
                            break
                    self.assertFalse(warnings, warnings)
                    self.assertIsNotNone(route.writer)
                    self.assertGreater(route.sink.processedUSecs(), 0)
                    self.assertEqual(route.sink.format().sampleRate(), 44100)
                    self.assertEqual(route.sink.format().sampleFormat(),
                                     QAudioFormat.Int16 if width=="16" else QAudioFormat.Int32)
                    self.assertEqual(route.sink.volume(), 0.)
                    self.assertEqual(route.active[1], key)
                    window.toggle_play()
                    QTest.qWait(40)
                    window.toggle_play()
                window.seek.setValue(2000)
                window.seek_to()
                QTest.qWait(150)
                self.assertGreaterEqual(window.player.position(), 2000)
                window.apply_settings(dict(window.preferences, output_rate=0, output_width="auto"))
                self.assertIsNone(route.sink)
                self.assertIs(window.player.audioOutput(), window.output)
                dialog.reject()
            finally:
                window.stop()
                window.player.setSource(QUrl())
                window.close()

