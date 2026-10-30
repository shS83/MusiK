# MusiK

Qt-musiikkisoitin ja basson tahtiin reagoiva OpenGL-visualisointi.
Purppuran ja oranssin liukuväri, oskilloskooppi, sumu, horisonttiin
jatkuva shakkilautamaasto ja heijastava metallikuula.

## Käynnistys

```bash
uv sync
uv run musik.py
# Voit lisätä kappaleita myös komentoriviltä:
uv run musik.py "/polku/kappale.mp3" "/polku/toinen.flac"
```

Python 3.14+, PySide6 / Qt 6.8+, NumPy ja ModernGL. Visualisointi vaatii
OpenGL 3.3:n ja toimivan työpöytäistunnon. Qt:n FFmpeg-tausta hoitaa toiston
sekä soivan äänen näytteet. Tuetut koodekit riippuvat Qt/FFmpeg-asennuksesta;
tavallisia muotoja ovat MP3, FLAC, WAV, Ogg, Opus ja M4A.

## Käyttö

- **+ Add files** lisää kappaleita; kaksoisnapsautus aloittaa toiston.
- **Add folder** lisää tuetut äänitiedostot valitusta kansiosta ja sen
  alikansioista aakkosjärjestyksessä. Haku toimii taustalla, joten toistoa voi
  jatkaa. Haun aikana sama painike näyttää **Cancel scan**; keskeyttäminen
  säilyttää jo lisätyt kappaleet. Hakemistolinkkejä ei seurata.
  Kansiohaku ja tiedostovalitsin käyttävät samoja tiedostopäätteitä:
  MP3, FLAC, WAV/WAVE, OGG/OGA, Opus, M4A/M4B, AAC, AIFF/AIF/AIFC ja WMA.
- **Play / Pause** on yksi painike: toiston aikana se näyttää Pause ja tauolla
  Play. Tauolta jatketaan samaa kappaletta samasta kohdasta.
  **Stop / Next / Previous** ohjaavat pysäytystä ja kappaleenvaihtoa.
  Previous palaa kappaleen alkuun, kun sitä on soitettu yli kolme sekuntia.
  Kappaleen lopussa siirrytään seuraavaan; listan lopussa toisto pysähtyy.
  Next/Previous-napeilla voi kiertää listan päästä toiseen.
- **− Remove files** poistaa valitut kappaleet listalta. Ctrl/Shift valitsee
  useita. Kappaleita voi järjestää vetämällä. Äänitiedostoja ei poisteta levyltä.
- **Load playlist** korvaa listan M3U/M3U8-tiedoston sisällöllä;
  **Save playlist** tallentaa UTF-8-soittolistan suhteellisilla tiedostopoluilla.
  Puuttuvat tiedostot ja verkkolinkit ohitetaan ilmoituksen kanssa.
- Aikajana siirtää toistokohtaa, Volume säätää äänenvoimakkuutta.
- **Visualization** avaa visualisoinnin omaan ikkunaan. Sen sulkeminen ei
  keskeytä musiikkia. Pääikkunan sulkeminen lopettaa myös visualisoinnin.

| Näppäin | Toiminto |
| --- | --- |
| Space (soitinikkunassa) | Toisto / tauko |
| Ctrl+O | Lisää tiedostoja |
| Delete (soittolistassa) | Poista valitut listalta |
| F11 (visualisoinnissa) | Koko näyttö / ikkuna |
| Esc (visualisoinnissa) | Pois koko näytöstä / sulje ikkuna |
| ↑ / ↓ | Sumun tiheys |
| ← / → | Sumun liikkeen nopeus |
| R | Palauta sumun oletusasetukset |

Bassontunnistus mittaa 35–180 Hz:n energiaa 2048 näytteen FFT-ikkunalla.
Mukautuva kynnys, energian nousu ja 220 ms:n vähimmäisväli erottavat iskuja
jatkuvasta bassosta. Stereokanavien tehot yhdistetään, joten vastakkaisvaiheinen
basso ei kumoudu. Tunnistus on heuristinen eikä takaa jokaisen iskun tunnistusta.
Kuula suurenee iskuista ja tausta välähtää satunnaisella värillä. Oskilloskooppi
näyttää voimakkaamman äänikanavan. Analyysi seuraa soittimen dekoodaamia
äänipuskureita; äänilaitteen puskurointi voi aiheuttaa pienen ajoituseron.

Kuulan pinta reagoi lisäksi jatkuvasti keskiääniin (180–2500 Hz) ja
ylä-ääniin (2500–16000 Hz, äänitiedoston näytteenottotaajuuden rajoissa).
Keskiäänet synnyttävät laajempaa aaltoilua ja ylä-äänet ulospäin työntyviä,
teräviä piikkejä. Piikkien korkeus seuraa ylä-äänien voimakkuutta, ja ne
vetäytyvät äänen vaimentuessa. Muodonmuutos vaikuttaa sekä siluettiin että
metallin heijastuksiin.
Pehmeä voimakkuuden skaalaus tuo esiin myös hiljaisempia soittimia;
nopea nousu ja hitaampi vaimeneminen tasoittavat liikettä.
Hiljaisuus vaimentaa värinän ja tauko tai pysäytys nollaa sen.

Sumun kohina ja domain warp -kerrokset on sovitettu projektista
`/home/shs/Code/Fog/fog.py`. Shaderit ovat tämän projektin `shaders/`-hakemistossa;
alkuperäistä Fog-projektia ei tarvita käynnistyksessä.

## Tarkistukset

```bash
uv run python -m unittest discover -s tests -v
# Työpöydällä; luo testiaallon, pitää äänen mykistettynä ja avaa testin ajaksi ikkunat:
uv run python -m tests.smoke_desktop
```

Qt:n äänipuskurirajapinta:
[QAudioBufferOutput](https://doc.qt.io/QT-6/qaudiobufferoutput.html).
