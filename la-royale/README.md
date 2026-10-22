# LA Royale

**A battle royale map of Los Angeles, in 3D, in the browser.** Ride the blimp
across the city, jump, skydive, open your chute over the spot you picked, and
land anywhere from the Santa Monica Pier to Dodger Stadium. Then walk, sprint,
swim, or jump in a car and drive the 405.

This first version is the **explorable map**: no opponents yet. Combat, loot
and the closing circle are the next step (see [What's next](#whats-next)).

A standalone project: it is not part of MAZ ARCADE and does not use the Maz
engine. It is built with [three.js](https://threejs.org) (r169, MIT licence,
vendored in `vendor/` so the game needs nothing from the internet).

## Playing it

The game is plain files, but browsers only run its JavaScript modules when
they're served over http, not opened straight from disk. Either:

- open it on the published site once it's on `main`, at `la-royale/` under
  the repo's GitHub Pages address, or
- run any local web server from the repo root, e.g.
  `python3 -m http.server` and visit `http://localhost:8000/la-royale/`.

| | Keyboard & mouse | Phone / tablet |
|---|---|---|
| Move | W A S D / arrows | left thumb stick |
| Look | mouse (click the game first) | drag on the right side |
| Sprint | Shift | Run |
| Jump · leave the blimp · open chute | Space | Jump |
| Get in / out of a car | E | Car |
| Brake · lowrider hop | Space | Jump |
| Police siren | F | |
| Horn | H | Horn |
| Map (tap a spot, then **Drop here**) | M | Map |
| Skip the clock ahead 3 hours | T | |
| Camera near / far | C, mouse wheel | |

Graphics quality (Low / Medium / High) is in **Settings** on the title
screen. Phones start on Low.

## The map

6 km × 6 km, projected from real latitude and longitude, so everything sits
where it does in LA: the Pacific is west, the Santa Monica Mountains and
Hollywood Hills run along the north with the San Fernando Valley behind
them, Downtown is east, LAX is south-west. The real basin is about 38 km
across, so distances are squeezed roughly six to one; buildings, cars and
people stay full size. The one deliberate cheat is the harbour: the Port of
Los Angeles and Long Beach really sit 20 km further south, so they're pulled
up into the south-east corner.

**60 neighbourhoods**, each with its own street grid and look: Malibu, the
Palisades, Santa Monica, Venice, Marina del Rey, Westwood, Bel Air, Beverly
Hills, Century City, West Hollywood, Hollywood, the Hollywood Hills, Laurel
Canyon, Koreatown, Los Feliz, Silver Lake, Echo Park, Chinatown, Downtown,
the Arts District, Boyle Heights, East LA, Inglewood, Watts, Compton,
Pasadena, Glendale, Burbank, Studio City, Sherman Oaks and more. Downtown's
grid runs 36° off north like the real one; Santa Monica's and Venice's follow
the coast. Beverly Hills is mansions with pools and palm-lined streets, the
hills are houses clinging to the slopes, Venice is narrow lots in beach
colours, Downtown is towers with helipads on the roofs (an old LA fire-code
rule).

**Landmarks** (more than 70): the Hollywood Sign, Griffith Observatory, the
Hollywood Bowl, Capitol Records, the TCL Chinese Theatre and the Walk of
Fame, the Pantages, the Sunset Strip's billboards and the Whisky a Go Go,
Chateau Marmont, Universal Studios, Warner Bros., the Santa Monica Pier
(the Pacific Wheel turns, the West Coaster, the Route 66 "End of the Trail"
sign), Venice Beach (Muscle Beach, the skate park, the VENICE sign, the
canals), Malibu Pier and the beach houses on stilts, Manhattan Beach Pier,
Marina del Rey's boats, lifeguard towers all down the sand, the Getty, UCLA's
Royce Hall, the Beverly Hills Hotel, Rodeo Drive, the Beverly Hills sign,
Greystone, Century City and Fox Plaza ("Nakatomi Plaza"), Urban Light at
LACMA, the La Brea Tar Pits and their mammoths, the Petersen, the Grove,
LAX (the Theme Building, the glowing pylons, the tower, parked jets), the
In-N-Out under the flight path, Randy's Donuts, SoFi Stadium, the Forum,
the Intuit Dome, SpaceX's Falcon 9, the El Segundo refinery, Downtown's
skyline (Wilshire Grand, US Bank Tower, Aon, Gas Company Tower, City
National), Walt Disney Concert Hall, the Broad, Grand Park, Grand Central
Market, City Hall, Union Station, Olvera Street, Chinatown's Dragon Gate,
Crypto.com Arena and LA Live, the Sixth Street Viaduct over the LA River's
concrete channel, Dodger Stadium, Echo Park Lake, Silver Lake Reservoir,
Lake Hollywood, the Coliseum and its Olympic torch, Space Shuttle Endeavour,
Watts Towers, Mariachi Plaza, the Rose Bowl, Pasadena City Hall, the Port's
gantry cranes and a container ship, the Queen Mary, and the Goodyear blimp
base in Carson.

**Freeways and boulevards**: the 405, 10, 101, 110, 5, 105, 710, 134 and the
Arroyo Seco Parkway, plus PCH, Sunset, Hollywood, Santa Monica, Wilshire,
Venice, Olympic, La Brea, Vermont, Western, Crenshaw, Lincoln, Sepulveda,
Century, Ventura and Mulholland Drive along the crest. The freeways are
jammed with hundreds of abandoned cars, the 405 worst of all.

Also: 16,000+ trees (palms along the boulevards, jacarandas, cypresses,
chaparral on the hills), nodding oil pumpjacks in the Baldwin Hills, a day
that ends with the sun setting over the Pacific, and a city that lights up
orange at night.

## How it's built

| File | What it is |
|---|---|
| `js/geo.js` | LA as data: the coastline, ridges, river, lakes, freeways, boulevards and neighbourhoods, in latitude/longitude, plus the projection. |
| `js/landmarks.js` | Every landmark, built from simple shapes at its real position. |
| `js/world.js` | Turns the data into a world: terrain, the city generator, colliders, collision and "where am I". No DOM, no three.js. |
| `js/paint.js` | Paints the ground (blocks, beaches, pools, runways): the terrain's texture, the minimap and the map. Also the night-time street glow. |
| `js/render.js` | Builds the three.js scene: instanced buildings with a window shader, roads, trees, water, sky and the day cycle. |
| `js/models.js` | The player, the parachute, the eight kinds of car, the blimp. |
| `js/hud.js` | Compass, minimap, location banner and the full map. |
| `js/game.js` | The game loop: blimp, skydiving, walking, swimming, driving, the camera and the controls. |
| `tests/world.test.js` | The world's tests (plain Node). |

The world is generated from a seed, the same every time. Everything that
needs to be solid (buildings, landmarks, cars, trees) is described once, and
both the renderer and the collision read that one description.

### Tests

```sh
node la-royale/tests/world.test.js
```

They check the geography (the ocean is west, the Hollywood Sign is high in
the hills, the river runs in a channel), that the generator keeps buildings
out of the sea and off the freeways, that every landmark and car is where it
should be, and that you can't walk through walls or fall through the ground.
CI runs them on every push that touches `la-royale/`
(`.github/workflows/la-royale.yml`).

## What's next

The map was built with the full battle royale in mind:

- **Opponents**: AI players dropping from the same blimp. The colliders and
  "where am I" are already there to steer them.
- **Loot**: weapons, armour and healing spawning in buildings and at
  landmarks.
- **The circle**: a shrinking safe zone (the marine layer rolling in?) that
  forces the fight together.
- **Combat**, health and a last-one-standing win.
- **Building interiors** worth landing in.
