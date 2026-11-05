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

- **+ Add files** lisää useita kerralla valittuja kappaleita (Ctrl/Shift);
  kaksoisnapsautus soittolistalla aloittaa toiston.
- **Add folders** avaa usean hakemiston valinnan. Valitse puusta Ctrl/Shiftillä
  useita kansioita ja paina Open, tai kerää eri sijainneista kansioita
  **Add selected folders** -painikkeella. Kaikki alikansiot sisällytetään;
  päällekkäisistä kansiovalinnoista ei lisätä samaa tiedostoa kahdesti. Haku toimii taustalla, joten toistoa voi
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
- **Oskilloskoopin napsautus** vaihtaa spektrianalysaattoriin; uusi napsautus
  palauttaa oskilloskoopin. Spektrissä on 48 logaritmisesti jaettua taajuuskaistaa
  (30 Hz–20 kHz, näytteenottotaajuuden rajoissa) ja −72–0 dB:n asteikko.
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
teräviä piikkejä. Piikit nousevat kaarevasti ja pehmeällä juurella suoraan pallon pinnasta.
Niiden enimmäispituus on 1,35 kertaa pallon perussäde.
Piikkien korkeus seuraa ylä-äänien voimakkuutta logaritmisesti, ja ne
vetäytyvät äänen vaimentuessa. Muodonmuutos vaikuttaa sekä siluettiin että
metallin heijastuksiin.
Pehmeä voimakkuuden skaalaus tuo esiin myös hiljaisempia soittimia;
nopea nousu ja hitaampi vaimeneminen tasoittavat liikettä.
Hiljaisuus vaimentaa värinän ja tauko tai pysäytys nollaa sen.

Sumun kohina ja domain warp -kerrokset on sovitettu projektista
`/home/shs/Code/Fog/fog.py`. Shaderit ovat tämän projektin `shaders/`-hakemistossa;
alkuperäistä Fog-projektia ei tarvita käynnistyksessä.

## Settings ja visualisaatiot

**Settings** tallentaa asetukset käyttäjäkohtaisesti (Windowsissa QSettingsin
MusiK/MusiK-rekisteriasetuksiin). OK ottaa muutokset käyttöön heti; Cancel
säilyttää entiset asetukset. Asetukset palautuvat käynnistettäessä.

- Oskilloskoopin tiheys: 64–2048 pistettä, käytettävissä olevien ääninäytteiden
  ja näyttötarkkuuden rajoissa.
- Spektrin tiheys: 16–128 logaritmista FFT-palkkia. Suuri palkkimäärä ei lisää
  FFT-ikkunan todellista taajuuserottelua matalilla taajuuksilla.
- Soittolistan järjestys: tuonti/oma järjestys, nimi A–Z, nimi Z–A,
  kansio/tiedostonimi tai sekoitettu. Oma järjestys sallii raahaamisen;
  siihen palaaminen palauttaa tuontijärjestyksen. Sekoitettu järjestys säilyy
  vakaana tuonnin aikana. Järjestyksen vaihtaminen ei katkaise soivaa kappaletta.
- 20 pysyvää gradienttiteemaa: alkuperäinen MusiK ja 19 kiinteällä satunnaissiemenellä
  luotua palettia. Värit pysyvät samoina käynnistyksestä toiseen.
- Visualisaatio: Chrome sphere, PsyTunnel, Hiss tai Matrixia. Vaihto päivittää
  myös avoimen visualisaatioikkunan ja säilyttää koko näytön tilan.

**PsyTunnel** käyttää alkuperäisen Psytunnel-projektin tunnelishaderia.
Äänen energia ja taajuuskaistat kiihdyttävät etenemistä, suvantokohdat hidastavat
sitä ja voimakkaasta hiljaiseen siirtyminen voi käynnistää lyhyen peruutuksen.
Bassoisku synnyttää valkoisen, pehmeästi häipyvän välähdyksen.

**Hiss** käyttää alkuperäisen hiss-projektin valkoista kuutiota ja
kaksivaiheista taittoefektiä ilman HUDia. Musiikki muuttaa pyörimisen ja
virtauksen nopeutta, vääristymää sekä RGB-hajontaa; hiljaiset kohdat rauhoittavat
liikettä.

**Matrixia** sovittaa alkuperäisen merkkisateen Qt:lle ilman Pygame-riippuvuutta
ja toast/HUD-tekstejä. Sateen nopeus seuraa kokonaisenergiaa, bassoiskuja,
keskiääniä ja ylä-ääniä. Napsautus kiertää seitsemän värin välillä.

Efektit toimitetaan MusiKin mukana, joten alkuperäisten projektien ei tarvitse
olla asennettuina. Lähteet: sisarprojektit `Psytunnel/tunnel.frag`,
`hiss/shaders/scene.frag`, `hiss/shaders/hiss.frag` ja `Matrixia/matrixia.py`.
Niiden MIT-lisenssi on mukana tiedostossa `licenses/effects-MIT.txt`.

## Lasers-visualisaatiot

Settings → Visualization sisältää myös viisi Lasers-projektista sovitettua
visualisaatiota ilman HUD-tekstejä:

| MusiKin nimi | Alkuperäinen tiedosto | Ilme |
| --- | --- | --- |
| Laser Nexus | `laser_convergence.py` | Pisteet kohti laukaisuhetken kohdetta |
| Orbit Streams | `laser_convergence_v2.py` | Pistevirrat seuraavat liikkuvaa kohdetta |
| Glyph Waves | `laser_convergence_v4.py` | Aaltoilevasti kääntyvät fonttimerkit |
| Glyph Spin | `laser_convergence_v4_2.py` | Merkkien jatkuva täysi pyörintä |
| Glyph Trails | `laser_convergence_v4_3.py` | Pyörivät merkit ja häipyvät jäljet |

**Laser pattern changes** valitsee 4 tai 8 tahdin vaihtovälin. Tahtiarvio olettaa
4/4-tahtilajin ja arvioi neljäsosanuotin pituuden bassoiskujen väleistä.
Jakso käynnistyy ensimmäisistä tunnistetuista iskuista; tämä ei ole kappaleen
ensimmäisen tahdin tai tahtilajin varma tunnistus. Synkopointi ja puoli-/tuplatempo
voivat vaikuttaa arvioon. Ääninäytteiden kello ohjaa vaihtoja, joten ikkunan
piirtonopeus ei muuta tahtimäärää. Pitkä hiljaisuus pysäyttää tahtiarvion.

Jaksojen rajoilla muuttuvat säteiden määrä, pyörimisnopeus/suunta ja väri sekä
merkkiversioissa merkkimäärä, koko ja merkki. Nopeus ja väri siirtyvät pehmeästi.
Lisäksi kokonaisvoimakkuus, keskiäänet ja ylä-äänet ohjaavat liikettä jatkuvasti.
Bassoiskut lisäävät valkoisen, eksponentiaalisesti häipyvän kick-välähdyksen,
jota ei tallenneta efektin jälkikuvaan. Tauko, pysäytys ja haku nollaavat
ääni- ja tahtianalyysin. Asetus säilyy seuraavaan käynnistykseen.

Sovitukset ovat MusiKin sisällä eivätkä tarvitse Pygamea tai erillistä
Lasers-hakemistoa. Alkuperäinen MIT-lisenssi: `licenses/lasers-MIT.txt`.

## Kappaleen metatiedot ja Party Mode

Pidä hiirtä soittolistan kappaleen päällä **2 sekuntia**, niin metatietoikkuna
avautuu. Se näyttää tiedoston saatavilla olevat tiedot, esimerkiksi nimen,
esittäjän, albumin, raidan numeron, genren, vuoden, keston ja bittinopeuden.
Tietojen saatavuus riippuu tiedoston tageista ja Qt/FFmpeg-tuesta.
Lukeminen ei vaihda soivaa kappaletta eikä keskeytä toistoa.

Kansikuva etsitään samasta hakemistosta: ensisijaisesti kappaleen niminen kuva,
`cover`, `folder`, `front`, `albumart`, `album` tai `artwork` (JPG, PNG, WebP,
BMP); myös muut hakemiston kuvat kelpaavat. Jos hakemistosta ei löydy kuvaa,
käytetään saatavilla olevaa upotettua kansikuvaa. Kuvahaku ei käytä verkkoa.
Sulje ikkuna **Escillä** tai oikean yläkulman **×**-painikkeella. Sama kappale
ei avaa ikkunaa heti uudelleen; siirrä kohdistin ensin pois kappaleelta.
Selaus, raahaaminen ja riviltä poistuminen peruuttavat odottavan avauksen.

**Party Mode** on **Start visualization** -painikkeen yläpuolella. Se käynnistää
visualisoinnin ja kiertää kaikkia yhdeksää efektiä **15 sekunnin välein**.
Uusi painallus pysäyttää kierron ja jättää nykyisen efektin näkyviin.
Visualisaatioikkunan sulkeminen lopettaa myös Party Moden. Koko näytön tila
ja ikkunan koko säilyvät vaihdoissa. Asetus- tai metatietoikkunan ollessa auki
vaihtoja odotetaan, jotta kierto ei häiritse ikkunan käyttöä.
Settingsistä valittu yksittäinen visualisaatio säilyy oletusvalintana;
Party Mode ei tallenna kiertäviä efektejä sen päälle.

## Tarkistukset

```bash
uv run python -m unittest tests.test_core -v
# Asetukset, monivalinta ja efektien renderöinti (työpöytäistunto):
uv run python -m unittest tests.test_settings_effects tests.test_lasers tests.test_metadata_party -v
# Työpöydällä; luo testiaallon, pitää äänen mykistettynä ja avaa testin ajaksi ikkunat:
uv run python -m tests.smoke_desktop
```

Qt:n äänipuskurirajapinta:
[QAudioBufferOutput](https://doc.qt.io/QT-6/qaudiobufferoutput.html).

## Circler Tetra

The Python visualizer includes **Circler Tetra** (Settings → Visualization).
Tetrahedra spiral/grow as in the original Circler; bass triggers a radial
jump and restrained white flash. New neon colors arrive after every 16
detected hits; the background cycles deep blue → yellow → red. See
`CIRCLER-TETRA.md` for testing and parameters.

## fog_shS visualization

The new **fog_shS** effect follows Circler Tetra in Settings → Visualization. See [FOG-SHS.md](FOG-SHS.md) for audio-driven 3D contour behavior, rendering requirements, and testing instructions.
