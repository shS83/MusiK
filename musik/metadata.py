"""Delayed playlist hover and an asynchronous, read-only track information dialog."""
from pathlib import Path

from PySide6.QtCore import (
    QEvent, QModelIndex, QObject, QPersistentModelIndex, QRunnable, QSize,
    Qt, QThreadPool, QTimer, QUrl, Signal,
)
from PySide6.QtGui import QImage, QImageReader, QPixmap
from PySide6.QtMultimedia import QMediaMetaData, QMediaPlayer
from PySide6.QtWidgets import (
    QApplication, QDialog, QFormLayout, QHBoxLayout, QLabel, QListWidget,
    QPushButton, QScrollArea, QVBoxLayout, QWidget,
)


class MetadataPlaylist(QListWidget):
    metadata_requested = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMouseTracking(True)
        self.hover_index = QPersistentModelIndex()
        self.hover_position = None
        self.last_opened_path = None
        self.metadata_open = False
        self.hover_timer = QTimer(self)
        self.hover_timer.setSingleShot(True)
        self.hover_timer.setInterval(2000)
        self.hover_timer.setTimerType(Qt.TimerType.PreciseTimer)
        self.hover_timer.timeout.connect(self.open_hovered)
        self.verticalScrollBar().valueChanged.connect(self.cancel_hover)
        self.horizontalScrollBar().valueChanged.connect(self.cancel_hover)
        self.model().rowsAboutToBeRemoved.connect(self.cancel_hover)
        self.model().modelAboutToBeReset.connect(self.cancel_hover)

    def cancel_hover(self, *args):
        self.hover_timer.stop()
        self.hover_index = QPersistentModelIndex()

    def mouseMoveEvent(self, event):
        super().mouseMoveEvent(event)
        index = self.indexAt(event.position().toPoint())
        path = index.data(Qt.ItemDataRole.UserRole) if index.isValid() else None
        if path != self.last_opened_path:
            self.last_opened_path = None
        if (self.metadata_open or event.buttons() != Qt.MouseButton.NoButton
                or path is None or path == self.last_opened_path):
            self.cancel_hover()
            return
        if index != self.hover_index:
            self.hover_index = QPersistentModelIndex(index)
            self.hover_position = event.position().toPoint()
            self.hover_timer.start()

    def viewportEvent(self, event):
        if event.type() in (QEvent.Type.Leave, QEvent.Type.MouseButtonPress,
                            QEvent.Type.Wheel, QEvent.Type.Hide):
            self.cancel_hover()
            if event.type() == QEvent.Type.Leave and not self.metadata_open:
                self.last_opened_path = None
        return super().viewportEvent(event)

    def open_hovered(self):
        if (not self.hover_index.isValid() or self.metadata_open or not self.isVisible()
                or QApplication.activeModalWidget() is not None
                or QApplication.mouseButtons() != Qt.MouseButton.NoButton):
            return
        if self.indexAt(self.hover_position) != QModelIndex(self.hover_index):
            return
        path = self.hover_index.data(Qt.ItemDataRole.UserRole)
        self.last_opened_path = path
        self.metadata_open = True
        self.cancel_hover()
        self.metadata_requested.emit(path)


def folder_art(path):
    """Read one decodable local image, preferring explicit album-cover names."""
    track = Path(path)
    preferred = (track.stem.casefold(), "cover", "folder", "front", "albumart", "album", "artwork")
    extensions = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}
    candidates = [entry for entry in track.parent.iterdir()
                  if entry.suffix.casefold() in extensions and entry.is_file()]
    candidates.sort(key=lambda entry: (preferred.index(entry.stem.casefold())
                                       if entry.stem.casefold() in preferred else len(preferred),
                                       entry.name.casefold()))
    for candidate in candidates:
        try:
            if candidate.stat().st_size > 32 * 1024 * 1024:
                continue
            reader = QImageReader(str(candidate))
            reader.setAutoTransform(True)
            size = reader.size()
            if size.isValid():
                reader.setScaledSize(size.scaled(QSize(600, 600), Qt.AspectRatioMode.KeepAspectRatio))
            image = reader.read()
            if not image.isNull():
                return image, candidate.name
        except OSError:
            continue
    return QImage(), ""


class ArtSignals(QObject):
    ready = Signal(object, str)


class ArtReader(QRunnable):
    def __init__(self, path):
        super().__init__()
        self.path = path
        self.signals = ArtSignals()

    def run(self):
        try:
            image, name = folder_art(self.path)
        except OSError:
            image, name = QImage(), ""
        self.signals.ready.emit(image, name)


class MetadataDialog(QDialog):
    def __init__(self, path, parent=None):
        super().__init__(parent)
        self.path = Path(path)
        self.setWindowTitle("MusiK · Track information")
        self.setWindowModality(Qt.WindowModality.WindowModal)
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.resize(760, 520)
        self.setMinimumSize(560, 360)
        self.folder_image = QImage()
        self.embedded_image = QImage()
        self.closed = False
        layout = QVBoxLayout(self)
        header = QHBoxLayout()
        title = QLabel(self.path.stem)
        title.setTextFormat(Qt.TextFormat.PlainText)
        title.setWordWrap(True)
        header.addWidget(title, 1)
        self.close_button = QPushButton("×")
        self.close_button.setAccessibleName("Close track information")
        self.close_button.setFixedWidth(44)
        self.close_button.clicked.connect(self.reject)
        header.addWidget(self.close_button, 0, Qt.AlignmentFlag.AlignTop)
        layout.addLayout(header)
        body = QHBoxLayout()
        cover_column = QVBoxLayout()
        self.cover = QLabel("No album art")
        self.cover.setFixedSize(240, 240)
        self.cover.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.cover.setStyleSheet("background: rgba(0,0,0,70); border-radius: 10px;")
        cover_column.addWidget(self.cover)
        self.cover_source = QLabel("Searching this folder…")
        self.cover_source.setTextFormat(Qt.TextFormat.PlainText)
        self.cover_source.setWordWrap(True)
        self.cover_source.setMaximumWidth(240)
        cover_column.addWidget(self.cover_source)
        cover_column.addStretch()
        body.addLayout(cover_column)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border: none; background: transparent; }")
        content = QWidget()
        self.form = QFormLayout(content)
        self.form.setRowWrapPolicy(QFormLayout.RowWrapPolicy.WrapLongRows)
        scroll.setWidget(content)
        body.addWidget(scroll, 1)
        layout.addLayout(body, 1)
        self.status = QLabel("Reading metadata… · Esc closes this window")
        self.status.setWordWrap(True)
        layout.addWidget(self.status)
        # This player only probes metadata: no audio output, play() or changes
        # to the application's actual playback player.
        self.reader = QMediaPlayer(self)
        self.reader.metaDataChanged.connect(self.refresh)
        self.reader.durationChanged.connect(self.refresh)
        self.reader.mediaStatusChanged.connect(self.media_status)
        self.reader.errorOccurred.connect(self.read_error)
        self.timeout = QTimer(self)
        self.timeout.setSingleShot(True)
        self.timeout.setInterval(8000)
        self.timeout.timeout.connect(self.read_timeout)
        self.refresh()
        self.art_reader = ArtReader(path)
        self.art_reader.signals.ready.connect(self.set_folder_art)
        QThreadPool.globalInstance().start(self.art_reader)
        self.reader.setSource(QUrl.fromLocalFile(str(self.path)))
        self.timeout.start()

    def add_field(self, key, value):
        label = QLabel(str(value))
        label.setTextFormat(Qt.TextFormat.PlainText)
        label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        label.setWordWrap(True)
        self.form.addRow(key, label)

    def refresh(self, *args):
        if self.closed:
            return
        while self.form.rowCount():
            self.form.removeRow(0)
        metadata = self.reader.metaData()
        excluded = {QMediaMetaData.Key.CoverArtImage, QMediaMetaData.Key.ThumbnailImage,
                    QMediaMetaData.Key.Duration}
        priority = (QMediaMetaData.Key.Title, QMediaMetaData.Key.ContributingArtist,
                    QMediaMetaData.Key.AlbumTitle, QMediaMetaData.Key.AlbumArtist,
                    QMediaMetaData.Key.TrackNumber, QMediaMetaData.Key.Genre,
                    QMediaMetaData.Key.Date, QMediaMetaData.Key.Composer)
        keys = sorted(metadata.keys(), key=lambda key: (
            priority.index(key) if key in priority else len(priority), key.name))
        for key in keys:
            if key not in excluded:
                value = metadata.stringValue(key)
                if key == QMediaMetaData.Key.AudioBitRate:
                    bitrate = metadata.value(key)
                    if isinstance(bitrate, (int, float)):
                        value = f"{bitrate / 1000:g} kb/s"
                if value:
                    self.add_field(QMediaMetaData.metaDataKeyToString(key), value)
        duration = self.reader.duration()
        if duration:
            seconds = duration // 1000
            self.add_field("Duration", f"{seconds // 60}:{seconds % 60:02d}")
        self.add_field("File", self.path.name)
        self.add_field("Folder", str(self.path.parent))
        for key in (QMediaMetaData.Key.CoverArtImage, QMediaMetaData.Key.ThumbnailImage):
            if key not in metadata.keys():
                continue
            value = metadata.value(key)
            if isinstance(value, QImage) and not value.isNull():
                self.embedded_image = value
                break
        self.update_cover()

    def set_folder_art(self, image, name):
        if self.closed:
            return
        self.folder_image = image
        self.cover_source.setText(name if not image.isNull() else "No cover image in this folder")
        self.update_cover()

    def update_cover(self):
        image = self.folder_image if not self.folder_image.isNull() else self.embedded_image
        if not image.isNull():
            self.cover.setPixmap(QPixmap.fromImage(image).scaled(
                self.cover.size() * self.devicePixelRatioF(), Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation))
            pixmap = self.cover.pixmap()
            pixmap.setDevicePixelRatio(self.devicePixelRatioF())
            self.cover.setPixmap(pixmap)
            if self.folder_image.isNull():
                self.cover_source.setText("Embedded album art")

    def media_status(self, status):
        if status in (QMediaPlayer.MediaStatus.LoadedMedia, QMediaPlayer.MediaStatus.BufferedMedia):
            self.timeout.stop()
            self.refresh()
            self.status.setText("Available metadata · Esc or × closes this window")

    def read_error(self, error, message):
        if self.closed:
            return
        self.timeout.stop()
        self.status.setText("Metadata unavailable for this file · Esc or × closes this window")

    def read_timeout(self):
        self.status.setText("Some metadata could not be read · Esc or × closes this window")

    def done(self, result):
        self.closed = True
        self.timeout.stop()
        self.reader.setSource(QUrl())
        super().done(result)
