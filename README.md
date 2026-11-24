# Globe3DS

Nintendo 3DS homebrew country games on a spinning 3D globe. Pick a game from the menu:

- **Classic:** a country is hidden somewhere on the globe, and you find it by guessing. Every guess is
  painted in a colour that shows how close it is. Icy blue means far away, orange and red mean you're getting warm.
- **Bridge** and **Bridge: Hard:** you get a start country and a goal country, and you build a bridge between them out of
  countries that share land borders.

![Playing](docs/screenshot-playing.png)

## Classic

- Type a country name on the touch keyboard. Suggestions appear as you type.
- Tap a suggestion, press **Guess**, or press **A** to guess the highlighted one.
- The top screen shows the distance from your guess's border to the hidden country's border,
  whether you got warmer or colder, and your closest guess so far.
- A neighbouring country (sharing a border) is 0 km away.
- Find it in as few guesses as you can.

## Bridge

![Bridge](docs/screenshot-bridge.png)

- The start country is blue and the goal is pink. The top screen tells you the fewest countries a bridge needs.
- Guess countries in any order. Each one is coloured by how useful it is:
  **green** is on a shortest route, **yellow** is close to one (a route at most two countries longer), and **red** is off route.
- The bridge is built as soon as your guesses link the start to the goal through shared borders.
- **Bridge** (normal) has no guess limit: keep going until the bridge is built.
- **Bridge: Hard** gives you only the shortest bridge length plus 4 guesses. Run out and one shortest route is shown.
- Either way, SELECT twice gives up and shows a shortest route.
- Only land borders count. Borders that exist only through overseas territories (France with Brazil and Suriname)
  don't count, and small territories such as Hong Kong aren't used.

## Controls

| Button | Action |
| --- | --- |
| Touch keyboard | Type a country name |
| A | Guess the highlighted suggestion / new game after a round |
| B | Backspace |
| D-Pad up/down | Move through suggestions or scroll the guess list |
| X | Show or hide your guesses, sorted closest first |
| Circle Pad | Spin the globe |
| L / R | Zoom out / in |
| SELECT twice | Give up and reveal the answer |
| START | Back to the menu (quit from the menu) |

Your stats (games played, win rate, average guesses, streak) are saved per mode in `sdmc:/3ds/globe3ds/`.

## Installing

1. Download `Globe3DS.3dsx` (from a GitHub Actions build artifact, or build it yourself below).
2. Copy it to `sdmc:/3ds/` on your SD card.
3. Open it from the Homebrew Launcher.

## Building

You need [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `3ds-dev` group (devkitARM, libctru, citro2d).

```sh
make
```

Or with Docker:

```sh
docker run --rm -v "$PWD":/src -w /src devkitpro/devkitarm make
```

## Country data

Borders come from [Natural Earth](https://www.naturalearthdata.com/) 1:50m Admin 0 countries, which is in the public domain.
`tools/gen_countries.py` turns it into `source/countrydata.c`: it simplifies and triangulates the borders
for the globe, and precomputes the shortest border-to-border distance between every pair of countries.

```sh
pip install shapely numpy mapbox_earcut
curl -LO https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_50m_admin_0_countries.geojson
python3 tools/gen_countries.py ne_50m_admin_0_countries.geojson source/countrydata.c
```

## Desktop preview

`tools/sim` builds the game for your computer with a stand-in for the 3DS libraries, replays a
script of button presses and renders the screens to PNG files. It's useful for checking layout changes
without a console:

```sh
tools/sim/run.sh tools/sim/test.script /tmp/globe3ds-shots
SIM_SEED=2 tools/sim/run.sh tools/sim/bridge.script /tmp/globe3ds-bridge
```

`SIM_SEED` fixes the random puzzle so a script can be replayed.
