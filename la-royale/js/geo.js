/*
 * Los Angeles, as data.
 *
 * Everything here is real geography written in latitude and longitude and
 * projected onto a 6 km × 6 km play area: the coastline of Santa Monica Bay,
 * the Santa Monica Mountains and Hollywood Hills, the LA River, the freeways,
 * the big boulevards and the neighbourhoods. The real basin is about 38 km
 * across, so the map is squeezed roughly six to one; buildings, cars and
 * people stay full size, which is why the drive from Santa Monica to Downtown
 * takes a couple of minutes instead of an hour.
 *
 * The one place the map cheats is the harbour: the Port of Los Angeles and
 * Long Beach really sit 20 km further south, so they are pulled up into the
 * south-east corner as "San Pedro Bay", the way battle royale maps do.
 *
 * x runs west → east, z runs north → south (three.js convention), metres.
 */

export const SIZE = 6000;
export const HALF = SIZE / 2;

const LAT0 = 34.02, LON0 = -118.355, KX = 14600, KZ = 17600;

export function project(lat, lon) { return [(lon - LON0) * KX, (LAT0 - lat) * KZ]; }
export function unproject(x, z) { return [LAT0 - z / KZ, LON0 + x / KX]; }
const PL = (list) => list.map(([a, b]) => project(a, b));

/* The shoreline from Malibu round Santa Monica Bay to Manhattan Beach. The
 * ocean is everything to the south-west of it. */
export const COAST = PL([
  [34.040, -118.600], [34.0365, -118.560], [34.034, -118.540], [34.030, -118.525],
  [34.022, -118.512], [34.012, -118.502], [34.005, -118.494], [33.995, -118.484],
  [33.985, -118.475], [33.977, -118.465], [33.965, -118.457], [33.950, -118.446],
  [33.935, -118.437], [33.915, -118.428], [33.895, -118.418], [33.870, -118.408],
  [33.840, -118.398]
]);

export const OCEAN = (() => {
  const first = COAST[0], last = COAST[COAST.length - 1];
  return [...COAST, [last[0], 9000], [-9000, 9000], [-9000, first[1]]];
})();

/* Hard-edged water: the marina basin and the (relocated) harbour. */
export const WATER_POLYS = [
  { name: 'Marina del Rey', pts: [[-1700, 690], [-1560, 700], [-1420, 640], [-1290, 660], [-1260, 780], [-1320, 880], [-1520, 860], [-1700, 800]] },
  { name: 'San Pedro Bay', pts: [[560, 3200], [600, 2900], [900, 2790], [1300, 2770], [1600, 2720], [2000, 2670], [2400, 2620], [2700, 2540], [3200, 2470], [3200, 3200]] }
];

/* Ridges: [x, z, height, half-width]. Low points are the passes the 405
 * (Sepulveda Pass) and the 101 (Cahuenga Pass) squeeze through. */
export const RIDGES = [
  { name: 'Santa Monica Mountains', pts: [
    [-3300, -760, 150, 620], [-2700, -900, 190, 650], [-2200, -1150, 170, 600], [-1950, -1300, 140, 520],
    [-1750, -1450, 60, 420], [-1500, -1600, 150, 520], [-1000, -1750, 165, 500], [-500, -1850, 160, 480],
    [0, -1930, 120, 420], [150, -1960, 60, 360], [350, -2000, 165, 430], [520, -2010, 185, 440],
    [800, -1950, 170, 430], [1100, -1980, 140, 420], [1350, -2150, 100, 380], [1500, -2400, 50, 300]] },
  { name: 'Getty hill', pts: [[-1820, -1110, 55, 230], [-1700, -1000, 50, 200]] },
  { name: 'Silver Lake hills', pts: [[1050, -1500, 50, 260], [1300, -1250, 35, 220]] },
  { name: 'Elysian Park', pts: [[1500, -1050, 55, 260], [1750, -1250, 70, 320], [2050, -1450, 80, 350], [2300, -1650, 60, 320]] },
  { name: 'San Rafael Hills', pts: [[1650, -2500, 90, 350], [2000, -2750, 140, 400], [2400, -2950, 120, 380]] },
  { name: 'Baldwin Hills', pts: [[-300, 120, 55, 330], [0, 230, 75, 380], [150, 300, 50, 300]] }
];

/* The LA River's concrete channel and Ballona Creek. */
export const RIVERS = [
  { name: 'LA River', w: 16, depth: 6, pts: PL([
    [34.166, -118.600], [34.165, -118.470], [34.155, -118.400], [34.142, -118.350], [34.155, -118.300],
    [34.140, -118.277], [34.110, -118.268], [34.085, -118.245], [34.065, -118.228], [34.040, -118.226],
    [34.010, -118.222], [33.970, -118.215], [33.930, -118.205], [33.880, -118.195], [33.830, -118.190]]) },
  { name: 'Ballona Creek', w: 8, depth: 4, pts: PL([
    [34.038, -118.365], [34.020, -118.400], [33.995, -118.430], [33.978, -118.452]]) }
];

/* Venice's canals, nudged inland so they clear the (compressed) beach. */
export const CANALS = [
  [[-1600, 560], [-1460, 560]], [[-1600, 600], [-1460, 600]], [[-1600, 640], [-1460, 640]],
  [[-1590, 545], [-1590, 655]], [[-1530, 545], [-1530, 655]], [[-1470, 545], [-1470, 655]]
];

export const LAKES = [
  { name: 'Echo Park Lake', ll: [34.0726, -118.2606], r: 48 },
  { name: 'Silver Lake Reservoir', ll: [34.0928, -118.2701], r: 70 },
  { name: 'Lake Hollywood', ll: [34.1255, -118.3314], r: 60 },
  { name: 'MacArthur Park', ll: [34.0575, -118.2775], r: 34 },
  { name: 'La Brea Tar Pits', ll: [34.0656, -118.3554], r: 20, tar: true }
].map((l) => { const [x, z] = project(...l.ll); return { ...l, x, z }; });

/* Roads. kind: freeway (30 m), blvd (18 m), local (12 m). */
const R = (name, kind, ll) => ({ name, kind, pts: PL(ll) });
export const ROADS = [
  R('I-405 San Diego Fwy', 'freeway', [[34.200, -118.470], [34.160, -118.470], [34.130, -118.476], [34.095, -118.470], [34.065, -118.448], [34.035, -118.433], [34.000, -118.410], [33.975, -118.393], [33.950, -118.378], [33.930, -118.370], [33.900, -118.355], [33.850, -118.330]]),
  R('I-10 Santa Monica Fwy', 'freeway', [[34.016, -118.488], [34.022, -118.440], [34.028, -118.410], [34.035, -118.380], [34.037, -118.340], [34.036, -118.300], [34.032, -118.275], [34.030, -118.255], [34.040, -118.225], [34.050, -118.195], [34.060, -118.150]]),
  R('US-101 Hollywood Fwy', 'freeway', [[34.055, -118.225], [34.062, -118.245], [34.075, -118.270], [34.088, -118.300], [34.098, -118.322], [34.115, -118.338], [34.135, -118.355], [34.150, -118.380], [34.155, -118.420], [34.160, -118.470], [34.165, -118.570]]),
  R('I-110 Harbor Fwy', 'freeway', [[34.0625, -118.2475], [34.050, -118.266], [34.035, -118.272], [34.000, -118.280], [33.950, -118.281], [33.900, -118.283], [33.860, -118.285], [33.845, -118.282]]),
  R('Arroyo Seco Pkwy', 'freeway', [[34.0625, -118.2475], [34.075, -118.228], [34.095, -118.212], [34.110, -118.192], [34.125, -118.175], [34.140, -118.160], [34.150, -118.140]]),
  R('I-5 Golden State Fwy', 'freeway', [[34.200, -118.330], [34.175, -118.305], [34.150, -118.275], [34.120, -118.260], [34.090, -118.235], [34.070, -118.222], [34.050, -118.210], [34.030, -118.195], [34.010, -118.165], [33.990, -118.140]]),
  R('I-105 Century Fwy', 'freeway', [[33.929, -118.398], [33.929, -118.360], [33.928, -118.300], [33.926, -118.240], [33.920, -118.140]]),
  R('I-710 Long Beach Fwy', 'freeway', [[34.040, -118.170], [34.000, -118.172], [33.950, -118.180], [33.900, -118.190], [33.860, -118.192]]),
  R('SR-134 Ventura Fwy', 'freeway', [[34.155, -118.360], [34.157, -118.300], [34.150, -118.250], [34.145, -118.200], [34.140, -118.140]]),
  R('Lincoln Blvd', 'blvd', [[34.0135, -118.4925], [34.000, -118.468], [33.985, -118.448], [33.972, -118.432], [33.957, -118.425]]),
  R('Sepulveda Blvd', 'blvd', [[33.927, -118.396], [33.890, -118.395], [33.845, -118.393]]),
  R('Sunset Blvd', 'blvd', [[34.040, -118.556], [34.048, -118.530], [34.055, -118.505], [34.060, -118.480], [34.070, -118.460], [34.077, -118.445], [34.0805, -118.425], [34.0815, -118.410], [34.0860, -118.395], [34.0905, -118.380], [34.0960, -118.360], [34.0980, -118.340], [34.0980, -118.310], [34.0950, -118.290], [34.0850, -118.275], [34.0770, -118.260], [34.0610, -118.240]]),
  R('Hollywood Blvd', 'blvd', [[34.1010, -118.370], [34.1015, -118.345], [34.1016, -118.330], [34.1018, -118.315], [34.1020, -118.295], [34.0980, -118.280]]),
  R('Santa Monica Blvd', 'blvd', [[34.0170, -118.494], [34.0300, -118.470], [34.0450, -118.445], [34.0600, -118.420], [34.0735, -118.399], [34.0830, -118.385], [34.0900, -118.370], [34.0910, -118.345], [34.0910, -118.310], [34.0905, -118.285]]),
  R('Wilshire Blvd', 'blvd', [[34.0230, -118.500], [34.0400, -118.470], [34.0570, -118.445], [34.0640, -118.420], [34.0670, -118.400], [34.0640, -118.380], [34.0625, -118.355], [34.0620, -118.330], [34.0618, -118.300], [34.0580, -118.278], [34.0530, -118.262]]),
  R('Venice Blvd', 'blvd', [[33.987, -118.470], [34.000, -118.430], [34.020, -118.400], [34.035, -118.370], [34.040, -118.330], [34.045, -118.290]]),
  R('Olympic Blvd', 'blvd', [[34.028, -118.470], [34.050, -118.420], [34.054, -118.380], [34.050, -118.330], [34.047, -118.290], [34.040, -118.255]]),
  R('La Brea Ave', 'blvd', [[34.1020, -118.3445], [34.0620, -118.3440], [34.0300, -118.3460], [34.0000, -118.3520], [33.9620, -118.3530], [33.9300, -118.3530]]),
  R('Vermont Ave', 'blvd', [[34.1150, -118.2915], [34.0600, -118.2915], [34.0000, -118.2915], [33.9000, -118.2915]]),
  R('Western Ave', 'blvd', [[34.1050, -118.3090], [34.0500, -118.3090], [33.9500, -118.3090], [33.8800, -118.3090]]),
  R('Crenshaw Blvd', 'blvd', [[34.0620, -118.3255], [34.0200, -118.3330], [33.9600, -118.3300], [33.9000, -118.3265]]),
  R('Century Blvd', 'blvd', [[33.9456, -118.392], [33.9456, -118.330], [33.9456, -118.250]]),
  R('Ventura Blvd', 'blvd', [[34.163, -118.580], [34.158, -118.470], [34.149, -118.420], [34.140, -118.385], [34.139, -118.360]]),
  R('Colorado Blvd', 'blvd', [[34.146, -118.215], [34.146, -118.150]]),
  R('Cesar Chavez Ave', 'blvd', [[34.0560, -118.2420], [34.0480, -118.2050], [34.0420, -118.1700]])
];

/* Pacific Coast Highway hugs the shore, just behind the beach houses. */
ROADS.unshift((() => {
  const pts = [];
  for (let i = 0; i < 7; i++) {
    const a = COAST[Math.max(0, i - 1)], b = COAST[Math.min(COAST.length - 1, i + 1)];
    const L = Math.hypot(b[0] - a[0], b[1] - a[1]);
    // inland is to the left of the shoreline's direction of travel
    pts.push([COAST[i][0] + ((b[1] - a[1]) / L) * 140, COAST[i][1] - ((b[0] - a[0]) / L) * 140]);
  }
  pts.push(project(34.0135, -118.4925));
  return { name: 'Pacific Coast Hwy', kind: 'blvd', pts };
})());

/* Mulholland Drive winds along the crest of the mountains. */
ROADS.push((() => {
  const crest = RIDGES[0].pts;
  const pts = [];
  for (let x = -3000; x <= 450; x += 30) {
    let i = 0; while (i < crest.length - 2 && crest[i + 1][0] < x) i++;
    const a = crest[i], b = crest[i + 1];
    const t = Math.min(1, Math.max(0, (x - a[0]) / (b[0] - a[0])));
    pts.push([x, a[1] + (b[1] - a[1]) * t + 28 * Math.sin(x / 55) + 12 * Math.sin(x / 23)]);
  }
  return { name: 'Mulholland Dr', kind: 'local', pts };
})());

export const ROAD_WIDTH = { freeway: 30, blvd: 18, local: 11 };

/*
 * Neighbourhoods. Each is a Voronoi seed: a point belongs to whichever seed is
 * nearest, which gives every part of the map a name and a building style.
 * rot turns the street grid (Downtown's grid runs 36° off north, Santa
 * Monica's and Venice's follow the coast).
 */
const D = (name, ll, style, rot = 0, extra = {}) => {
  const [x, z] = Array.isArray(ll[0]) ? ll[0] : project(ll[0], ll[1]);
  return { name, x, z, style, rot, ...extra };
};
export const DISTRICTS = [
  D('Malibu', [34.0345, -118.555], 'beach', 0.08),
  D('Pacific Palisades', [34.050, -118.522], 'mansion', 0.5),
  D('Topanga', [34.085, -118.560], 'wild'),
  D('Santa Monica Mountains', [34.110, -118.505], 'wild'),
  D('Santa Monica', [34.022, -118.488], 'coastal', 0.72),
  D('Brentwood', [34.055, -118.475], 'mansion', 0.35),
  D('Venice', [33.991, -118.462], 'beach', 0.78),
  D('Marina del Rey', [33.973, -118.440], 'midrise', 0.78),
  D('Playa Vista', [33.972, -118.418], 'midrise', 0.0),
  D('Mar Vista', [34.003, -118.432], 'residential', 0.55),
  D('Westchester', [33.958, -118.398], 'residential'),
  D('LAX', [33.9425, -118.4081], 'airport'),
  D('El Segundo', [33.915, -118.412], 'industrial'),
  D('Manhattan Beach', [33.888, -118.402], 'beach'),
  D('Hawthorne', [33.912, -118.352], 'residential'),
  D('Inglewood', [33.962, -118.352], 'residential'),
  D('Baldwin Hills', [34.006, -118.362], 'oilfield'),
  D('Culver City', [34.021, -118.396], 'mixed', 0.3),
  D('Westwood', [34.064, -118.447], 'campus', 0.6),
  D('Bel Air', [34.092, -118.452], 'hills'),
  D('Beverly Hills', [34.073, -118.400], 'mansion', 0.4),
  D('Century City', [34.057, -118.416], 'towers', 0.6),
  D('West Hollywood', [34.088, -118.370], 'mixed'),
  D('Laurel Canyon', [34.118, -118.378], 'hills'),
  D('Hollywood Hills', [34.124, -118.340], 'hills'),
  D('Hollywood', [34.100, -118.330], 'hollywood'),
  D('Miracle Mile', [34.062, -118.352], 'midrise'),
  D('Mid-City', [34.045, -118.345], 'residential'),
  D('Koreatown', [34.0618, -118.300], 'midrise'),
  D('Los Feliz', [34.108, -118.292], 'residential'),
  D('Griffith Park', [34.130, -118.295], 'park'),
  D('Silver Lake', [34.087, -118.270], 'residential'),
  D('Echo Park', [34.076, -118.257], 'residential'),
  D('Elysian Park', [34.081, -118.236], 'park'),
  D('Chinatown', [34.064, -118.2385], 'mixed', 0.45),
  D('Downtown LA', [34.049, -118.254], 'downtown', 0.63),
  D('Arts District', [34.039, -118.233], 'industrial', 0.63),
  D('Boyle Heights', [34.033, -118.205], 'residential'),
  D('East LA', [34.024, -118.170], 'residential'),
  D('Vernon', [33.997, -118.226], 'industrial'),
  D('Exposition Park', [34.016, -118.287], 'campus'),
  D('Leimert Park', [34.006, -118.330], 'residential'),
  D('South LA', [33.975, -118.295], 'residential'),
  D('Watts', [33.940, -118.243], 'residential'),
  D('Compton', [33.896, -118.220], 'residential'),
  D('Gardena', [33.888, -118.309], 'residential'),
  D('Highland Park', [34.111, -118.192], 'residential'),
  D('Glendale', [34.145, -118.255], 'midrise'),
  D('Arroyo Seco', [34.160, -118.172], 'park'),
  D('Pasadena', [34.142, -118.152], 'mixed'),
  D('Burbank', [34.181, -118.309], 'suburb'),
  D('Universal City', [34.140, -118.356], 'mixed'),
  D('Studio City', [34.147, -118.395], 'suburb'),
  D('Sherman Oaks', [34.155, -118.450], 'suburb'),
  D('Encino', [34.162, -118.515], 'suburb'),
  D('Sepulveda Basin', [34.176, -118.480], 'park'),
  D('Carson', [[900, 2600]], 'industrial'),
  D('Port of Los Angeles', [[1800, 2480]], 'port'),
  D('Long Beach', [[2750, 2280]], 'midrise', 0.2)
];

/* How each style lays out its blocks: block length × depth, street width,
 * and the colour of the ground between the buildings. */
export const STYLES = {
  residential: { bx: 112, bz: 64, st: 12, lot: '#a3a679' },
  suburb: { bx: 124, bz: 72, st: 12, lot: '#9fa874' },
  beach: { bx: 84, bz: 46, st: 9, lot: '#cdbf98' },
  coastal: { bx: 96, bz: 66, st: 12, lot: '#b8b19c' },
  mansion: { bx: 150, bz: 92, st: 14, lot: '#7f9a5a' },
  mixed: { bx: 100, bz: 70, st: 14, lot: '#a8a294' },
  hollywood: { bx: 100, bz: 70, st: 14, lot: '#a49d92' },
  midrise: { bx: 100, bz: 72, st: 16, lot: '#aaa59b' },
  towers: { bx: 96, bz: 96, st: 18, lot: '#b4b0a8' },
  downtown: { bx: 92, bz: 92, st: 16, lot: '#b3aea5' },
  industrial: { bx: 160, bz: 100, st: 16, lot: '#a39d90' },
  port: { bx: 170, bz: 110, st: 18, lot: '#8f8c86' },
  campus: { bx: 140, bz: 100, st: 12, lot: '#8fa86a' },
  hills: null, park: null, wild: null, oilfield: null, airport: null
};

/* Los Angeles colours: stucco, terracotta, glass. */
export const PALETTE = {
  stucco: ['#efe6d2', '#e9d8b8', '#f2d4b4', '#e8c39e', '#f4ead5', '#d9c7a7', '#f1dcc3', '#e6b89c', '#d8d0c0', '#f0e2a8', '#c9d6c2', '#d6e0e8', '#f2c6b4', '#fbfaf5'],
  beach: ['#f6f1e4', '#9fd3d6', '#f4d06f', '#f28f6b', '#a8d5a2', '#e9e2d0', '#7fb7d9', '#f2b5c4', '#ffffff'],
  roofTile: ['#b5562f', '#a8492b', '#c0663b', '#9b4a2f'],
  roofGray: ['#6b6b6b', '#7d7468', '#5a5f66'],
  glass: ['#6f8797', '#4d6475', '#8aa1ad', '#9aa6ae', '#3e4b56', '#b8b2a6', '#a39e94', '#5d6f7e', '#7d8f88'],
  brick: ['#9a5a43', '#8b4a37', '#b0a28a', '#8d8f8c', '#c2b59b', '#a76b4d'],
  white: ['#f7f3ea', '#efe7d6', '#f3e9dc', '#e9e4da'],
  car: ['#c0392b', '#f5f5f5', '#1d1d1f', '#2c3e50', '#bdc3c7', '#7f8c8d', '#2980b9', '#f1c40f', '#16a085', '#8e44ad', '#d35400', '#e8e2d0']
};
