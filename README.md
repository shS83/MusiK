# µsiK

The idea for µsiK came from the DeadBeef player, whose playback controls were the
same color as the background: black. I figured I could at least do that better.
The project grew, an AI gremlin got its hands on it, and this is the result: a
decent music player with a few rather good-looking visualizers included by default.

## Getting started

```bash
uv sync
uv run musik.py
# You can also add tracks from the command line:
uv run musik.py "/path/to/track.mp3" "/path/to/another.flac"
```

Requires Python 3.14+, PySide6 / Qt 6.8+, NumPy, and ModernGL. Visualization
requires OpenGL 3.3 and a working desktop session. Qt's FFmpeg backend handles
playback and provides samples of the playing audio. Supported codecs depend on
the Qt/FFmpeg installation; common formats include MP3, FLAC, WAV, Ogg, Opus,
and M4A.

## Usage

- **+ Add files** adds multiple selected tracks at once (Ctrl/Shift).
  Double-click a track in the playlist to start playback.
- **Add folders** opens a dialog for selecting multiple directories. Select
  several folders in the tree with Ctrl/Shift and click Open, or collect folders
  from different locations with **Add selected folders**. All subfolders are
  included; overlapping folder selections do not add the same file twice.
  Scanning runs in the background, so playback can continue. During a scan, the
  same button shows **Cancel scan**; canceling keeps tracks already added.
  Directory symlinks are not followed. Folder scanning and the file picker
  support the same extensions: MP3, FLAC, WAV/WAVE, OGG/OGA, Opus, M4A/M4B,
  AAC, AIFF/AIF/AIFC, and WMA.
- **Play / Pause** is one button: it shows Pause during playback and Play when
  paused. Resuming continues the same track from the same position.
  **Stop / Next / Previous** stop playback or change tracks. Previous returns
  to the start of the current track if it has played for more than three
  seconds. Playback advances at the end of a track and stops at the end of the
  playlist. The Next/Previous buttons wrap around the playlist.
- **− Remove files** removes selected tracks from the playlist. Use Ctrl/Shift
  to select multiple tracks. Drag tracks to reorder them. Audio files are not
  deleted from disk.
- **Load playlist** replaces the playlist with the contents of an M3U/M3U8
  file; **Save playlist** writes a UTF-8 playlist with relative file paths.
  Missing files and web links are skipped with a notification.
- **Click the oscilloscope** to switch to the spectrum analyzer; click again to
  return to the oscilloscope. The spectrum has 48 logarithmically spaced
  frequency bands (30 Hz–20 kHz, subject to the sample rate) and a −72–0 dB
  scale.
- Use the timeline to seek and Volume to adjust the sound level.
- **Visualization** opens the visualizer in a separate window. Closing it does
  not interrupt the music. Closing the main window also closes the visualizer.

# Chrome Sphere visualization

| Key | Action |
| --- | --- |
| Space (in the player window) | Play / pause |
| Ctrl+O | Add files |
| Delete (in the playlist) | Remove selected tracks from the list |
| F11 (in the visualizer) | Toggle fullscreen |
| Esc (in the visualizer) | Exit fullscreen / close the window |
| ↑ / ↓ | Fog density |
| ← / → | Fog movement speed |
| R | Restore default fog settings |

Bass detection measures energy at 35–180 Hz using a 2048-sample FFT window.
An adaptive threshold, rising energy, and a minimum interval of 220 ms help
distinguish beats from sustained bass. The power of the stereo channels is
combined, so out-of-phase bass does not cancel out. Detection is heuristic
and cannot guarantee every beat is found. The sphere grows on beats, and the
background flashes a random color. The oscilloscope displays the stronger
audio channel. Analysis follows audio buffers decoded by the player; buffering
in the audio device can cause a small timing difference.

The sphere's surface also responds continuously to midrange frequencies
(180–2500 Hz) and high frequencies (2500–16000 Hz, subject to the audio file's
sample rate). Midrange sounds create broader ripples, while high frequencies
produce sharp, outward-pointing spikes. The spikes rise in a curve with soft
bases directly from the sphere's surface. Their maximum length is 1.35 times
the sphere's base radius. Spike height follows high-frequency intensity
logarithmically, and the spikes recede as the sound fades. The deformation
affects both the silhouette and the metal reflections. Gentle level scaling
also reveals quieter instruments; a fast attack and slower decay smooth out
the motion. Silence dampens the movement, and pausing or stopping resets it.

The fog noise and domain warp layers were adapted from
`/home/shs/Code/Fog/fog.py`. The shaders are in this project's `shaders/`
directory; the original Fog project is not needed at runtime.

## Settings and visualizations

**Settings** saves preferences per user (in the QSettings MusiK/MusiK registry
settings on Windows). OK applies changes immediately; Cancel keeps the
previous settings. Saved settings are restored at startup.

- Oscilloscope density: 64–2048 points, subject to the available audio samples
  and display resolution.
- Spectrum density: 16–128 logarithmic FFT bars. A larger number of bars does
  not improve the FFT window's actual frequency resolution at low frequencies.
- Playlist order: import/custom order, name A–Z, name Z–A, folder/filename, or
  shuffled. Custom order allows dragging; returning to it restores the import
  order. Shuffled order remains stable while importing. Changing the order
  does not interrupt the playing track.
- 20 persistent gradient themes: the original MusiK theme and 19 palettes
  generated with fixed random seeds. Their colors remain the same across
  launches.
- Visualization: Chrome Sphere, PsyTunnel, Hiss, or Matrixia. Switching also
  updates an open visualizer window and preserves its fullscreen state.

**PsyTunnel** uses the tunnel shader from the original Psytunnel project.
Audio energy and frequency bands speed up movement through the tunnel, quiet
passages slow it down, and a transition from loud to quiet can trigger a brief
reverse movement. A bass beat produces a white flash that fades smoothly.

**Hiss** uses the white cube and two-stage refraction effect from the original
hiss project, without its HUD. Music changes the speed of rotation and flow,
distortion, and RGB dispersion; quiet passages calm the motion.

**Matrixia** adapts the original character rain to Qt without a Pygame
dependency or toast/HUD text. The rain speed follows overall energy, bass
beats, midrange, and high frequencies. Click to cycle through seven colors.

The effects are bundled with MusiK, so the original projects do not need to
be installed. Sources: sibling projects `Psytunnel/tunnel.frag`,
`hiss/shaders/scene.frag`, `hiss/shaders/hiss.frag`, and `Matrixia/matrixia.py`.
Their MIT license is included in `licenses/effects-MIT.txt`.

## Lasers visualizations

Settings → Visualization also includes three visualizations adapted from the
Lasers project, without HUD text:

| MusiK name | Original file | Appearance |
| --- | --- | --- |
| Orbit Streams | `laser_convergence_v2.py` | Streams of points follow a moving target |
| Glyph Spin | `laser_convergence_v4_2.py` | Characters rotate continuously through full turns |
| Glyph Trails | `laser_convergence_v4_3.py` | Rotating characters leave fading trails |

**Laser pattern changes** selects a 4- or 8-bar interval between changes.
The beat estimate assumes a 4/4 time signature and estimates the quarter-note
duration from intervals between bass beats. The cycle starts with the first
detected beats; this does not reliably identify the track's first bar or its
time signature. Syncopation and half or double tempo can affect the estimate.
The audio sample clock controls changes, so the window's frame rate does not
affect the bar count. Prolonged silence pauses the beat estimate.

At cycle boundaries, the number of beams, rotation speed and direction, and
color change; glyph variants also change the number, size, and choice of
characters. Speed and color transition smoothly. Overall volume, midrange,
and high frequencies also drive continuous movement. Bass beats add a white
kick flash that fades exponentially and is not retained in the effect's
afterimage. Pausing, stopping, and seeking reset the audio and beat analysis.
The setting persists across launches.

The adaptations are included in MusiK and do not require Pygame or a separate
Lasers directory. The original MIT license is in `licenses/lasers-MIT.txt`.

## Track metadata and Party Mode

Right-click a track and select Track information to open its metadata view.
It shows available file details such as title, artist, album, track number,
genre, year, duration, and bit rate. Availability depends on the file's tags
and Qt/FFmpeg support. Reading the information does not change the playing
track or interrupt playback.

Cover art is sought in the same directory, prioritizing an image named after
the track, then `cover`, `folder`, `front`, `albumart`, `album`, or `artwork`
(JPG, PNG, WebP, BMP); other images in the directory are also accepted. If
none is found there, available embedded cover art is used. The search does
not use the network. Close the window with **Esc** or the **×** button in the
upper-right corner. The same track will not open the window again immediately;
first move the pointer away from the track. Scrolling, dragging, or leaving
the row cancels a pending opening.

**Party Mode** is above the **Start visualization** button. It starts the
visualizer and cycles through all nine effects **every 15 seconds**. Pressing
it again stops the cycle and leaves the current effect visible. Closing the
visualizer window also stops Party Mode. Fullscreen state and window size are
preserved between changes. While the Settings or metadata window is open,
changes wait so the cycle does not interfere with using the window. The
individual visualization selected in Settings remains the default; Party
Mode does not save the cycling effects over it.

## Checks

```bash
uv run python -m unittest tests.test_core -v
# Settings, multiple selection, and effect rendering (desktop session):
uv run python -m unittest tests.test_settings_effects tests.test_lasers tests.test_metadata_party -v
# On a desktop; generates a test waveform, keeps audio muted, and opens windows for the test:
uv run python -m tests.smoke_desktop
```

Qt's audio buffer interface:
[QAudioBufferOutput](https://doc.qt.io/QT-6/qaudiobufferoutput.html).

## Circler Tetra

**Circler Tetra** (Settings → Visualization).
Rotating tetrahedra grow as they spiral forward. Bass makes the tetrahedra
jump and triggers a white flash.
`CIRCLER-TETRA.md`

## fog_shS visualization

**fog_shS** [FOG-SHS.md](FOG-SHS.md) Rotating shS text that vibrates with the music.

# Crawler visualization

Press **h** to show its keyboard shortcuts in the HUD.
