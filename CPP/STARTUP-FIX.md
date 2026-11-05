# MusiK C++ — soittolistan latauksen nopeutus

Korjaus perustuu 5.11.2026 toimitettuun `koodit.zip`-lähdekoodiin.

## Mikä hidasti?

`PlayerWindow`-konstruktori kutsui `loadList()`-funktiota, joka kutsui
`addFile()`-funktiota jokaista soittolistan riviä kohden. `addFile()`
etsi duplikaatit käymällä koko aiemmin lisätyn listan läpi, minkä jälkeen
`refreshNumbers()` kävi jälleen läpi **kaikki** rivit. Lisäksi se selvitti
soivan kappaleen tiedostopolun jokaiselle riville uudelleen.

Käyttäjän 12 797 kappaleen soittolistalla `PlayerWindow`-konstruktorin
mitattu kesto oli noin 41,5 sekuntia.

## Tehdyt muutokset

- `addFiles(QStringList)` lisää tiedostot yhdessä erässä.
- `QSet<QString>` huolehtii duplikaattien tunnistamisesta ilman toistuvaa
  koko soittolistan läpikäyntiä.
- Näkymän päivitykset jäädytetään erälisäyksen ajaksi.
- Rivinumerot päivitetään vain kerran erän lopuksi.
- `refreshNumbers()` selvittää toistettavan tiedoston polun vain kerran.
- M3U8-lataus, kansiotuonti, usean tiedoston tuonti, lajittelu ja
  komentorivitiedostojen käsittely käyttävät erälisäystä.
- `loadList()` tulostaa erikseen lukemisen ja lisäyksen ajat.
- `tests/player.cpp` sisältää toistuvan erälatauksen ja duplikaattien
  regressiontestin.

## Kääntäminen ja käyttö Arch Linuxissa

Projektin juuressa:

```bash
./build-linux.sh
QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_glx ./build-linux/MusiK
```

`QT_QPA_PLATFORM=xcb`-asetus on mukana, koska tämän koneen
NVIDIA/Wayland/EGL-yhdistelmällä OpenGL-kontekstin luonti epäonnistui.
Se on erillinen ongelma kuin hidas soittolistan lataus.

Käynnistyksen tulosteen uusi osa on esimerkiksi:

```text
[MusiK] Playlist read: ... ms, inserted: ... ms, entries: 12797
[MusiK] Window constructed: ... ms
```

Tuloksen todellista nopeutta ei ole tässä paketissa mitattu, koska
käytettävissä olevassa suoritusympäristössä ei ole Qt6:n C++-kehityskirjastoja.

## Testit

Jos Qt6 on asennettu ja haluat ajaa C++-testit:

```bash
cmake -S . -B build-linux -DMUSIK_BUILD_TESTS=ON
cmake --build build-linux -j
QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_glx ctest --test-dir build-linux --output-on-failure
```

`player_regression` sisältää myös ääni- ja OpenGL-testit, joten se vaatii
sopivan graafisen ympäristön ja ajurit. Varmista, että ajat uuden
`build-linux/MusiK`-binäärin etkä vanhaa versiota.
