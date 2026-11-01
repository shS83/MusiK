from pathlib import Path
import struct
import tempfile
import time
import unittest
import wave

from PySide6.QtCore import QSettings, Qt, QThreadPool, QEvent, QPointF
from PySide6.QtGui import QColor, QImage, QSurfaceFormat, QMouseEvent
from PySide6.QtTest import QTest, QSignalSpy
from PySide6.QtWidgets import QApplication

from musik.app import MainWindow
from musik.metadata import MetadataDialog, folder_art
from musik.settings import EFFECTS


def make_track(path):
    with wave.open(str(path), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(16000)
        audio.writeframes(b"\0\0" * 32000)
    info = b"INFO"
    for key, value in ((b"INAM", b"Hover test"), (b"IART", b"MusiK artist"), (b"IPRD", b"Test album")):
        value += b"\0"
        info += key + struct.pack("<I", len(value)) + value + b"\0" * (len(value) % 2)
    data = path.read_bytes() + b"LIST" + struct.pack("<I", len(info)) + info
    path.write_bytes(data[:4] + struct.pack("<I", len(data) - 8) + data[8:])


def wait_for(predicate, timeout=4000):
    deadline = time.monotonic() + timeout / 1000
    while not predicate() and time.monotonic() < deadline:
        QTest.qWait(20)
    assert predicate(), "Timed out"


def move_over(widget, position):
    # Send Qt's real viewport event directly: native cursor warping is not
    # delivered as a mouse-move event in every Windows desktop test session.
    event = QMouseEvent(QEvent.Type.MouseMove, QPointF(position),
                        QPointF(widget.mapToGlobal(position)), Qt.MouseButton.NoButton,
                        Qt.MouseButton.NoButton, Qt.KeyboardModifier.NoModifier)
    QApplication.sendEvent(widget, event)


class MetadataPartyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        QSurfaceFormat.setDefaultFormat(fmt)
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.path = self.root / "test.wav"
        make_track(self.path)
        self.window = MainWindow(QSettings(str(self.root / "prefs.ini"), QSettings.Format.IniFormat))
        self.window.output.setMuted(True)
        self.window.add_paths([self.path])
        self.window.show()
        QTest.qWait(40)
        QTest.mouseMove(self.window.settings_button)

    def tearDown(self):
        self.window.close()
        self.window.deleteLater()
        QTest.qWait(50)
        QThreadPool.globalInstance().waitForDone(3000)
        self.temporary.cleanup()

    def test_hover_wait_cancel_modal_escape_and_x(self):
        playlist = self.window.playlist
        position = playlist.visualItemRect(playlist.item(0)).center()
        move_over(playlist.viewport(), position)
        QTest.qWait(1100)
        self.assertIsNone(self.window.metadata_dialog)
        QApplication.sendEvent(playlist.viewport(), QEvent(QEvent.Type.Leave))
        QTest.qWait(1100)
        self.assertIsNone(self.window.metadata_dialog)
        started = time.monotonic()
        move_over(playlist.viewport(), position)
        QTest.qWait(1700)
        self.assertIsNone(self.window.metadata_dialog)
        wait_for(lambda: self.window.metadata_dialog is not None)
        self.assertGreaterEqual(time.monotonic() - started, 1.95)
        dialog = self.window.metadata_dialog
        self.assertTrue(dialog.isModal())
        QTest.keyClick(dialog, Qt.Key.Key_Escape)
        self.assertIsNone(self.window.metadata_dialog)
        QTest.qWait(2100)
        self.assertIsNone(self.window.metadata_dialog)
        self.window.show_track_metadata(str(self.path))
        self.window.metadata_dialog.close_button.click()
        self.assertIsNone(self.window.metadata_dialog)

    def test_metadata_cover_and_playback_untouched(self):
        cover = QImage(320, 200, QImage.Format.Format_RGB32)
        cover.fill(QColor("#c464a5"))
        cover.save(str(self.root / "cover.jpg"))
        self.window.play()
        wait_for(lambda: self.window.player.position() > 100)
        current = self.window.player.source()
        self.window.show_track_metadata(str(self.path))
        dialog = self.window.metadata_dialog
        wait_for(lambda: dialog.reader.duration() > 0 and not dialog.folder_image.isNull())
        values = [dialog.form.itemAt(i).widget().text() for i in range(dialog.form.count())]
        self.assertIn("Hover test", values)
        self.assertIn("MusiK artist", values)
        self.assertIn("0:02", values)
        self.assertFalse(dialog.cover.pixmap().isNull())
        self.assertEqual(self.window.player.source(), current)
        self.assertEqual(dialog.reader.playbackState(), dialog.reader.PlaybackState.StoppedState)
        dialog.reject()
        self.window.player.stop()
        self.window.player.setSource(type(current)())

    def test_folder_cover_fallback_and_unreadable_track(self):
        (self.root / "cover.jpg").write_bytes(b"not an image")
        image = QImage(80, 80, QImage.Format.Format_RGB32)
        image.fill(QColor("blue"))
        image.save(str(self.root / "folder.png"))
        result, name = folder_art(self.path)
        self.assertFalse(result.isNull())
        self.assertEqual(name, "folder.png")
        self.window.show_track_metadata(str(self.root / "missing.mp3"))
        dialog = self.window.metadata_dialog
        wait_for(lambda: "unavailable" in dialog.status.text())
        dialog.close_button.click()

    def test_party_cycles_all_preserves_fullscreen_and_settings(self):
        original = dict(self.window.preferences)
        self.assertEqual(self.window.party_timer.interval(), 12000)
        self.window.party_button.click()
        self.assertTrue(self.window.party_active)
        self.assertTrue(self.window.party_timer.isActive())
        self.window.party_timer.stop()  # Drive boundaries deterministically.
        self.window.visualizer.borderless.set_active(True)
        QTest.qWait(40)
        seen = {self.window.visualizer_effect}
        for _ in range(len(EFFECTS)):
            self.window.advance_party()
            QTest.qWait(40)
            self.assertTrue(self.window.visualizer.borderless.active)
            self.assertFalse(self.window.visualizer.error)
            seen.add(self.window.visualizer_effect)
        self.assertEqual(seen, set(range(len(EFFECTS))))
        self.assertEqual(self.window.preferences, original)
        self.window.party_button.click()
        self.assertFalse(self.window.party_timer.isActive())
        self.window.party_button.click()
        self.window.visualizer.close()
        self.assertFalse(self.window.party_active)
        self.assertFalse(self.window.party_button.isChecked())

    def test_party_timer_and_modal_deferral(self):
        self.window.party_button.click()
        self.window.party_timer.setInterval(120)
        spy = QSignalSpy(self.window.party_timer.timeout)
        wait_for(lambda: spy.count() > 0)
        self.window.party_timer.stop()
        self.assertNotEqual(self.window.visualizer_effect, self.window.preferences["effect"])
        self.window.show_track_metadata(str(self.path))
        current = self.window.visualizer_effect
        self.window.advance_party()
        self.assertEqual(self.window.visualizer_effect, current)
        self.window.metadata_dialog.reject()
        self.window.advance_party()
        self.assertNotEqual(self.window.visualizer_effect, current)


if __name__ == "__main__":
    unittest.main()
