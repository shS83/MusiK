from pathlib import Path
import tempfile
import unittest

import numpy as np
from PySide6.QtCore import QByteArray
from PySide6.QtMultimedia import QAudioBuffer, QAudioFormat

from musik.audio import AudioAnalysis, BassDetector
from musik.library import AUDIO_EXTENSIONS, scan_audio_files
from musik.playlist import load_playlist, save_playlist


class AudioTests(unittest.TestCase):
    def pulses(self, frequency=70, rate=48000):
        t = np.arange(rate * 3) / rate
        phase = (t - 0.25) % 0.5
        return (0.75 * np.sin(2 * np.pi * frequency * t)
                * np.exp(-phase * 30) * (phase < 0.18)).astype(np.float32)

    def detect(self, samples, block=731, rate=48000):
        detector = BassDetector()
        beats = []
        for start in range(0, len(samples), block):
            beats.extend(detector.feed(samples[start:start + block], rate))
        return beats

    def test_bass_pulses_and_buffer_boundaries(self):
        samples = self.pulses()
        for block in (256, 731, 4096):
            self.assertEqual(len(self.detect(samples, block)), 6)

    def test_silence_and_treble_do_not_trigger(self):
        self.assertEqual(self.detect(np.zeros(48000)), [])
        self.assertEqual(self.detect(self.pulses(4000)), [])

    def test_opposite_phase_stereo(self):
        samples = self.pulses()
        self.assertEqual(len(self.detect(np.column_stack((samples, -samples)))), 6)

    def test_sample_rate_change(self):
        detector = BassDetector()
        detector.feed(np.zeros((4000, 2)), 48000)
        self.assertEqual(detector.feed(np.zeros(4000), 44100), [])
        self.assertEqual(len(self.detect(self.pulses(rate=44100), rate=44100)), 6)

    def test_mid_and_treble_are_separate_continuous_envelopes(self):
        rate = 48000
        t = np.arange(rate // 2) / rate
        for frequency, active_band in ((700, 0), (6000, 1)):
            detector = BassDetector()
            tone = (0.15 * np.sin(2 * np.pi * frequency * t)).astype(np.float32)
            beats = detector.feed(np.column_stack((tone, -tone)), rate)
            self.assertEqual(beats, [])
            self.assertGreater(detector.levels[active_band], 0.45)
            self.assertLess(detector.levels[1 - active_band], 0.01)
            detector.feed(np.zeros((rate, 2)), rate)
            self.assertLess(max(detector.levels), 0.005)
            detector.reset()
            np.testing.assert_array_equal(detector.levels, [0, 0])

    def test_motion_levels_ignore_decoder_chunk_size(self):
        signal = self.pulses(700) + self.pulses(6000) * 0.5
        results = []
        for block in (256, 731, 4096):
            detector = BassDetector()
            for start in range(0, len(signal), block):
                detector.feed(signal[start:start + block], 48000)
            results.append(detector.levels.copy())
        np.testing.assert_allclose(results, [results[0]] * 3)

    def test_band_signal_and_reset(self):
        analysis = AudioAnalysis()
        received = []
        analysis.bands.connect(lambda mid, treble: received.append((mid, treble)))
        fmt = QAudioFormat()
        fmt.setSampleRate(48000)
        fmt.setChannelCount(1)
        fmt.setSampleFormat(QAudioFormat.SampleFormat.Float)
        t = np.arange(4096) / 48000
        tone = (0.2 * np.sin(2 * np.pi * 700 * t)).astype(np.float32)
        analysis.active = True
        analysis.receive(QAudioBuffer(QByteArray(tone.tobytes()), fmt))
        self.assertGreater(received[-1][0], 0.4)
        analysis.reset()
        self.assertEqual(received[-1], (0, 0))

    def test_spectrum_pitch_stereo_and_silence(self):
        for rate in (32000, 44100, 48000):
            for frequency in (100, 1000, 8000):
                t = np.arange(rate // 2) / rate
                tone = 0.3 * np.sin(2 * np.pi * frequency * t)
                detector = BassDetector()
                for start in range(0, len(tone), 731):
                    part = tone[start:start + 731]
                    detector.feed(np.column_stack((part, -part)), rate)
                peak = np.argmax(detector.spectrum_levels)
                self.assertLess(detector.spectrum_edges[peak], frequency * 1.35)
                self.assertGreater(detector.spectrum_edges[peak + 1], frequency / 1.35)
                self.assertGreater(detector.spectrum_levels[peak], 0.7)
                detector.feed(np.zeros((rate * 2, 2)), rate)
                self.assertLess(max(detector.spectrum_levels), 0.001)
                detector.reset()
                self.assertEqual(max(detector.spectrum_levels), 0)

    def test_pcm_formats_and_inactive_analysis(self):
        analysis = AudioAnalysis()
        received = []
        analysis.waveform.connect(received.append)
        for sample_format, dtype, scale, offset in (
            (QAudioFormat.SampleFormat.Float, np.float32, 1, 0),
            (QAudioFormat.SampleFormat.Int16, np.int16, 32768, 0),
            (QAudioFormat.SampleFormat.Int32, np.int32, 2147483648, 0),
            (QAudioFormat.SampleFormat.UInt8, np.uint8, 128, 128),
        ):
            fmt = QAudioFormat()
            fmt.setSampleRate(48000)
            fmt.setChannelCount(2)
            fmt.setSampleFormat(sample_format)
            values = np.full((2048, 2), 0.25 * scale + offset, dtype=dtype)
            buffer = QAudioBuffer(QByteArray(values.tobytes()), fmt)
            analysis.active = True
            analysis.receive(buffer)
            np.testing.assert_allclose(received[-1], 0.25)
        count = len(received)
        analysis.active = False
        analysis.receive(buffer)
        self.assertEqual(len(received), count)


class FolderTests(unittest.TestCase):
    def test_nested_formats_case_and_non_audio_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            album = root / "Äänet"
            album.mkdir()
            expected = []
            for index, extension in enumerate(sorted(AUDIO_EXTENSIONS)):
                track = album / f"{index:02d}{extension.upper()}"
                track.touch()
                expected.append(track)
            (root / "cover.jpg").touch()
            (album / "playlist.m3u8").touch()
            (root / "directory.mp3").mkdir()
            self.assertEqual(list(scan_audio_files(root)), expected)

    def test_symlink_loop_and_cancellation(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            track = root / "one.mp3"
            track.touch()
            (root / "loop").symlink_to(root, target_is_directory=True)
            (root / "missing.flac").symlink_to(root / "not-here")
            self.assertEqual(list(scan_audio_files(root)), [track])
            self.assertEqual(list(scan_audio_files(root, cancelled=lambda: True)), [])

    def test_unavailable_folder_is_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            errors = []
            self.assertEqual(list(scan_audio_files(Path(folder) / "missing", on_error=errors.append)), [])
            self.assertEqual(len(errors), 1)


class PlaylistTests(unittest.TestCase):
    def test_roundtrip_unicode_spaces_and_hash(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            tracks = [root / name for name in ("ääni 1.wav", "#intro.flac")]
            for track in tracks:
                track.touch()
            filename = root / "music.m3u8"
            save_playlist(filename, tracks)
            self.assertEqual(load_playlist(filename), (tracks, []))

    def test_bom_missing_and_file_url(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            audio = root / "a b.wav"
            audio.touch()
            filename = root / "music.m3u8"
            filename.write_text("\ufeff#EXTM3U\n" + audio.as_uri() + "\nmissing.wav\nhttps://example.com/audio\n", encoding="utf-8")
            tracks, missing = load_playlist(filename)
            self.assertEqual(tracks, [audio])
            self.assertEqual(len(missing), 2)


if __name__ == "__main__":
    unittest.main()
