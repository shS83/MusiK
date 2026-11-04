import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from PySide6.QtCore import QSettings, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
from musik.app import MainWindow


class PlaylistSearchTest(unittest.TestCase):
    def test_filter_in_player(self):
        app = QApplication.instance() or QApplication([])
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            window = MainWindow(QSettings(str(root/"settings.ini"), QSettings.IniFormat))
            self.addCleanup(window.deleteLater)
            miss = root/"Stars At The Old House Shine Inside.wav"
            hit = root/"Satoshi Tomiie"/"Disc 1"/"Numb.wav"
            hit.parent.mkdir(parents=True)
            hit.touch()
            miss.touch()
            window.add_paths([miss])
            window.show()
            window.activateWindow()
            QTest.qWaitForWindowActive(window)
            window.search.setFocus()
            QTest.keyClicks(window.search, "SATOSHI Tom")
            self.assertEqual(window.search.text(), "SATOSHI Tom")
            window.add_paths([hit])
            window.apply_settings(dict(window.preferences, order=1), persist=False)

            def visible():
                items = [window.playlist.item(i) for i in range(window.playlist.count())]
                return [Path(item.data(Qt.UserRole)) for item in items if not item.isHidden()]

            self.assertEqual(visible(), [hit])
            with patch.object(window, "play_item") as play:
                QTest.keyClick(window.search, Qt.Key_Return)
                self.assertEqual(Path(play.call_args.args[0].data(Qt.UserRole)), hit)
                window.search.setText("no-such-track")
                QTest.keyClick(window.search, Qt.Key_Return)
                self.assertEqual(play.call_count, 1)
            QTest.keyClick(window.search, Qt.Key_Escape)
            self.assertCountEqual(visible(), [hit, miss])
            with patch.object(window, "autosave_playlist"):
                window.close()
