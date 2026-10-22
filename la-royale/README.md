# LA Royale

**A battle royale map of Los Angeles, in 3D, in the browser.** Ride the blimp
across the city, jump, skydive, open your chute over the spot you picked, and
land anywhere from the Santa Monica Pier to Dodger Stadium. Then walk, sprint,
swim, or jump in a car and drive the 405.

This first version is the **explorable map**: no opponents yet. Combat, loot
and the closing circle are the next step (see [What's next](#whats-next)).

A standalone project: it is not part of MAZ ARCADE and does not use the Maz
engine. It is built with [three.js](https://threejs.org) (r169, MIT licence,
vendored in `vendor/` with the few add-ons it uses, so the game needs nothing
from the internet).

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

**Landmarks** (more than 140 named places; the full list is in
`js/landmarks.js` and `js/landmarks2.js`), among them: the Hollywood Sign, Griffith Observatory, the
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
Crypto.com Arena and LA Live, the Bradbury Building, Angels Flight, the
Broadway theatre marquees, the Last Bookstore, Pershing Square, the
Cathedral, the Music Center, the Wiltern, the Academy Museum's sphere, El
Capitan, the Egyptian, the Roosevelt, Musso & Frank, Amoeba, Paramount's gate,
Pink's, Canter's, the Pacific Design Center, the Greek Theatre, the Zoo,
Bronson Caves, the Venice Binoculars, Bob's Big Boy, the Farmer John mural,
the Sears tower in Boyle Heights, Eagle Rock, the Sixth Street Viaduct over the LA River's
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

**Street level**: cobra-head streetlights down the boulevards, wooden power
poles with sagging lines over the residential streets, traffic signals and
zebra crosswalks where boulevards cross, stop signs, yellow hydrants, bus
benches, blue mailboxes, bins out on the curb, raised sidewalks, Jersey
barriers down the freeways, and green overhead signs with real shields and
LA's real control cities ("405 NORTH · Sacramento", "10 WEST · Santa
Monica"). Shops face every boulevard with signs that fit the neighbourhood:
panaderías and carnicerías in East LA, Korean BBQ and noraebang in
Koreatown, surf shops and juice bars in Venice, tattoo parlours and record
stores in Hollywood.

**A living city**: traffic on the boulevards (it stops for you), jets
landing over Inglewood and taking off over the Pacific at LAX, and an LAPD
helicopter circling with its searchlight at night.

Also: 16,000+ trees (palms along the boulevards, jacarandas, cypresses,
chaparral on the hills), nodding oil pumpjacks in the Baldwin Hills, a day
that ends with the sun setting over the Pacific, and a city that lights up
orange at night.

## Graphics

Aiming at a 2008-era console open world, in a browser:

- physically based materials lit by the sun and by the sky itself: a
  scattering sky (with a bit of smog) is rendered into an environment map
  as the sun moves, so glass towers, car paint and the ocean reflect the
  real sky, from noon blue to sunset orange;
- soft shadows that follow you, plus contact shadows baked round every
  building and tree;
- facades from a shader: windows with frames, blinds and reflective glass,
  floor lines, shop windows on the ground floor, grime at the foot of the
  wall; terracotta barrel-tile roofs; windows that light up at night;
- bloom on everything that glows: neon, streetlights, signals, lit windows;
- textured leaves and palm fronds, asphalt with patches and cracks;
- a night sky with stars and a moon, and streets glowing sodium-orange.

**Graphics quality** in Settings: Low (no shadows or bloom, for phones),
Medium, High (bigger shadow maps, more anti-aliasing). The game also lowers
its resolution on its own when frames get slow, and raises it back when
there's room. F shows the frame rate.

## Real LA, true scale (beta)

A second map is on its way: **real Los Angeles at 1:1 size**, built from
OpenStreetMap. Every building comes from its real footprint and its measured
height (most LA buildings in OpenStreetMap carry one from the LA County
building survey), in its real spot; the streets are the real streets at
their real widths, with the real names; the ground is real elevation data,
every 10 m. The first area is **Hollywood to Downtown** (about 12 × 12 km);
it will grow outward from there.

Only the area near you is drawn in full detail (in 1 km squares, loaded as
you move); the taller buildings of the whole area are always shown, so the
skyline is there from anywhere. Up close each square gets:

- each building's real outline and height, with windows by type (houses,
  apartments, storefronts, glass towers, warehouses), LA stucco colours,
  hipped and gabled roofs on the houses (half Spanish tile, half
  shingle), parapets and rooftop air-conditioning on the flat roofs;
- the streets with lanes worked out from their real width (yellow centre
  lines on two-way streets), sidewalks, parking lots with painted stalls;
- the mapped trees, plus palms or jacarandas down each street's parkways,
  trees in the parks and back yards, chaparral on the hills;
- backyard pools, lakes and reservoirs you can swim in;
- streetlights, hydrants and cars parked along the kerbs;
- every named shop, café, bar and restaurant as a sign with its real name on
  the wall of the building it's in.
- the landmarks modelled on their real buildings: the Hollywood Sign,
  Griffith Observatory, Capitol Records, City Hall, the US Bank Tower's
  crown, the Wilshire Grand's spire, Disney Hall's steel sails, the Cinerama
  Dome, the Chinese Theatre, Union Station, the Hollywood Bowl, Dodger
  Stadium, Angels Flight, and Metro entrances at the real B/D Line stations;
- the Walk of Fame's stars down Hollywood Blvd and Vine, and a lit marquee
  and blade sign on every theatre and cinema;
- crosswalks, stop lines and arrows at every junction, LA's coloured kerbs,
  Wilshire's red bus lanes, street-name signs, bus stops, power lines, bins,
  meters, billboards (made-up ads), murals and graffiti;
- dingbats over their carports, balconies, window air conditioners and
  security bars;
- the mountains and the sea on the horizon out to 64 km, the basin glowing
  at night, grey marine-layer mornings, premiere searchlights, people on the
  sidewalks, crows and parrots, and the city's sound (synthesised).

To build it (about a minute, ~110 MB of downloads):

```sh
pip install pyarrow
python3 la-royale/tools/overture.py       # the buildings, streets, parks, water, shops (Overture Maps)
node la-royale/tools/fetch.mjs dem        # the ground elevation
node la-royale/tools/fetch.mjs far        # the mountains and coast round it, for the horizon
node la-royale/tools/build-tiles.mjs      # → la-royale/data/real/ (33 MB, not committed)
```

`overture.py` reads [Overture Maps](https://overturemaps.org), free open map
data on AWS that is OpenStreetMap's buildings, streets, parks and water plus
extra outlines, heights and named businesses; it fetches only the parts
covering the area. (`node la-royale/tools/fetch.mjs osm` gets the same from
OpenStreetMap's Overpass service directly, where that's reachable.) For
Hollywood to Downtown that's 151,000 buildings, 149,000 of them with a
measured height, 54,000 street pieces and 20,000 named shops and
restaurants.

Then a **Real LA** button appears on the title screen (or open
`index.html?map=real`). `tools/fixture.mjs` builds a made-up test
neighbourhood instead, for checking the engine with no download
(`index.html?map=real&data=fixture`); it's never published.

Map data © OpenStreetMap contributors, under the
[ODbL](https://www.openstreetmap.org/copyright), via Overture Maps (places
under CDLA-Permissive-2.0). Elevation: Mapzen Terrarium
tiles (AWS Open Data).

## How it's built

| File | What it is |
|---|---|
| `js/geo.js` | LA as data: the coastline, ridges, river, lakes, freeways, boulevards and neighbourhoods, in latitude/longitude, plus the projection. |
| `js/landmarks.js`, `js/landmarks2.js` | Every landmark, built from simple shapes at its real position. |
| `js/street.js` | Street furniture, shop signs and freeway signs. |
| `js/ambient.js` | Traffic, the LAX flights and the LAPD helicopter. |
| `js/world.js` | Turns the data into a world: terrain, the city generator, colliders, collision and "where am I". No DOM, no three.js. |
| `js/paint.js` | Paints the ground (blocks, beaches, pools, runways): the terrain's texture, the minimap and the map. Also the night-time street glow. |
| `js/render.js` | Builds the three.js scene: instanced buildings with a window shader, roads, trees, water, sky and the day cycle. |
| `js/models.js` | The player, the parachute, the eight kinds of car, the blimp. |
| `js/hud.js` | Compass, minimap, location banner and the full map. |
| `js/game.js` | The game loop: blimp, skydiving, walking, swimming, driving, the camera and the controls. |
| `js/atmosphere.js` | Sky, sun, moon and stars, the light, fog and bloom: shared by both maps. |
| `js/osm.js` | Turns OpenStreetMap buildings, streets, areas, trees and shops into compact tiles. No DOM. |
| `js/realcity.js` | The true-scale map: loads the ground and skyline, streams the detailed city in around you. |
| `js/reallandmarks.js` | The real map's landmarks, modelled on their real buildings. |
| `js/audio.js` | The city's sound, synthesised with Web Audio. |
| `tools/overture.py`, `tools/fetch.mjs`, `tools/build-tiles.mjs` | Download the real data and pack it into tiles (`tools/png.mjs` reads and writes the images). |
| `tests/world.test.js`, `tests/real.test.js` | The tests (plain Node). |

The world is generated from a seed, the same every time. Everything that
needs to be solid (buildings, landmarks, cars, trees) is described once, and
both the renderer and the collision read that one description.

### Tests

```sh
node la-royale/tests/world.test.js
node la-royale/tests/real.test.js
```

They check the geography (the ocean is west, the Hollywood Sign is high in
the hills, the river runs in a channel), that the generator keeps buildings
out of the sea and off the freeways, that every landmark and car is where it
should be, and that you can't walk through walls or fall through the ground.
`real.test.js` checks the true-scale map: the projection is 1:1, OpenStreetMap
heights, levels, roofs, materials and road widths are read right, tiles
survive packing, the ground follows the elevation data, landmarks are at
their real coordinates, and real building outlines stop you. CI runs them on every push that touches `la-royale/`
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
