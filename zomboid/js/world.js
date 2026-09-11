/* =============================================================================
 *  ZOMBOID: ANCHORAGE  —  WORLD DATA
 *  A tile map of downtown / midtown Anchorage, Alaska.
 *  Street grid + named real-world buildings & landmarks.
 *  Tile codes:
 *    0 grass        1 street(asphalt)   2 sidewalk     3 building floor
 *    4 wall         5 door              6 water        7 tree
 *    8 parkingLot   9 railroad
 * ========================================================================== */
(function (global) {
  'use strict';

  const T = {
    GRASS: 0, STREET: 1, SIDEWALK: 2, FLOOR: 3, WALL: 4,
    DOOR: 5, WATER: 6, TREE: 7, LOT: 8, RAIL: 9,
  };

  const MAP_W = 160;
  const MAP_H = 140;

  // ---- East/West streets (rows) of Anchorage, north -> south ----
  // Ship Creek / railyard sits at the very north, then the numbered avenues.
  const AVENUES = [
    { name: 'Ship Creek',                 row: 6  },
    { name: '1st Avenue',                 row: 14 },
    { name: '3rd Avenue',                 row: 22 },
    { name: '4th Avenue',                 row: 30 },
    { name: '5th Avenue',                 row: 38 },
    { name: '6th Avenue',                 row: 46 },
    { name: '9th Avenue',                 row: 54 },
    { name: 'Delaney Park Strip',         row: 62 },
    { name: '15th Avenue',                row: 72 },
    { name: 'Northern Lights Boulevard',  row: 88 },
    { name: 'Benson Boulevard',           row: 100 },
    { name: 'Tudor Road',                 row: 122 },
  ];

  // ---- North/South streets (columns), west -> east ----
  const STREETS = [
    { name: 'Cook Inlet',          col: 4   },
    { name: 'L Street',            col: 14  },
    { name: 'K Street',            col: 24  },
    { name: 'I Street',            col: 34  },
    { name: 'G Street',            col: 44  },
    { name: 'E Street',            col: 54  },
    { name: 'C Street',            col: 66  },
    { name: 'A Street',            col: 78  },
    { name: 'Cordova Street',      col: 90  },
    { name: 'Gambell Street',      col: 104 },
    { name: 'Ingra Street',        col: 114 },
    { name: 'Lake Otis Parkway',   col: 132 },
    { name: 'Boniface Parkway',    col: 150 },
  ];

  // ---- Named landmark buildings. x,y = top-left tile; w,h in tiles ----
  // door = [dx,dy] offset for the entrance. loot = container richness 0..3
  const BUILDINGS = [
    { name: 'Hotel Captain Cook',            x: 26, y: 24, w: 10, h: 5, door: [4, 4], loot: 2, kind: 'hotel' },
    { name: '4th Avenue Theatre',            x: 46, y: 24, w: 7,  h: 4, door: [3, 3], loot: 1, kind: 'theatre' },
    { name: 'Anchorage City Hall',           x: 56, y: 24, w: 6,  h: 4, door: [2, 3], loot: 1, kind: 'gov' },
    { name: 'Egan Convention Center',        x: 68, y: 24, w: 9,  h: 4, door: [4, 3], loot: 1, kind: 'civic' },
    { name: 'Dena’ina Center',          x: 26, y: 32, w: 8,  h: 4, door: [3, 3], loot: 1, kind: 'civic' },
    { name: 'Town Square Park',              x: 46, y: 32, w: 7,  h: 5, door: [3, 0], loot: 0, kind: 'park' },
    { name: 'Performing Arts Center',        x: 56, y: 32, w: 8,  h: 5, door: [3, 4], loot: 1, kind: 'civic' },
    { name: 'Nordstrom (5th Ave Mall)',      x: 68, y: 32, w: 9,  h: 5, door: [4, 4], loot: 3, kind: 'mall' },
    { name: 'Anchorage Museum',              x: 56, y: 40, w: 11, h: 5, door: [5, 0], loot: 1, kind: 'museum' },
    { name: 'Z.J. Loussac Library',          x: 80, y: 40, w: 8,  h: 5, door: [3, 4], loot: 1, kind: 'gov' },
    { name: 'Snow City Cafe',                x: 16, y: 40, w: 5,  h: 3, door: [2, 2], loot: 2, kind: 'food' },
    { name: 'Glacier Brewhouse',             x: 26, y: 40, w: 5,  h: 3, door: [2, 2], loot: 2, kind: 'food' },
    { name: '49th State Brewing',            x: 36, y: 40, w: 6,  h: 3, door: [2, 2], loot: 2, kind: 'food' },
    { name: 'Carrs Aurora Village',          x: 92, y: 32, w: 10, h: 6, door: [5, 5], loot: 3, kind: 'grocery' },
    { name: 'Fred Meyer',                    x: 106,y: 32, w: 12, h: 7, door: [6, 6], loot: 3, kind: 'grocery' },
    { name: 'Title Wave Books',              x: 92, y: 42, w: 7,  h: 4, door: [3, 3], loot: 1, kind: 'shop' },
    { name: 'REI Anchorage',                 x: 102,y: 42, w: 7,  h: 4, door: [3, 3], loot: 3, kind: 'outdoor' },
    { name: 'Sullivan Arena',                x: 100,y: 50, w: 12, h: 7, door: [6, 6], loot: 1, kind: 'arena' },
    { name: 'Merrill Field Hangars',         x: 120,y: 24, w: 14, h: 8, door: [7, 7], loot: 2, kind: 'industrial' },
    { name: 'Providence Medical Center',     x: 120,y: 92, w: 16, h: 9, door: [8, 8], loot: 3, kind: 'hospital' },
    { name: 'Alaska Native Medical Center',  x: 120,y: 104,w: 14, h: 8, door: [7, 7], loot: 3, kind: 'hospital' },
    { name: 'Moose’s Tooth Pub',        x: 96, y: 92, w: 6,  h: 4, door: [3, 3], loot: 2, kind: 'food' },
    { name: 'Bear Tooth Theatrepub',         x: 84, y: 92, w: 6,  h: 4, door: [3, 3], loot: 2, kind: 'food' },
    { name: 'Chilkoot Charlie’s',       x: 16, y: 90, w: 7,  h: 4, door: [3, 3], loot: 2, kind: 'bar' },
    { name: 'Spenard Builders Supply',       x: 16, y: 100,w: 9,  h: 5, door: [4, 4], loot: 3, kind: 'hardware' },
    { name: 'Dimond Center Mall',            x: 30, y: 118,w: 16, h: 9, door: [8, 8], loot: 3, kind: 'mall' },
    { name: 'University of Alaska Anchorage', x: 132,y: 70, w: 16, h: 10,door: [8, 9], loot: 2, kind: 'campus' },
    { name: 'Alaska Regional Hospital',      x: 132,y: 56, w: 12, h: 7, door: [6, 6], loot: 3, kind: 'hospital' },
    { name: 'Anchorage Police Dept',         x: 80, y: 56, w: 8,  h: 5, door: [4, 4], loot: 2, kind: 'police' },
    { name: 'APD Crime Lab',                 x: 92, y: 56, w: 6,  h: 4, door: [2, 3], loot: 2, kind: 'police' },
    { name: 'Alaska Railroad Depot',         x: 26, y: 8,  w: 10, h: 4, door: [5, 3], loot: 1, kind: 'transit' },
    { name: 'Westchester Lagoon Pavilion',   x: 14, y: 64, w: 5,  h: 3, door: [2, 2], loot: 0, kind: 'park' },
    { name: 'Bass Pro / Sportsman',          x: 100,y: 100,w: 10, h: 6, door: [5, 5], loot: 3, kind: 'outdoor' },
    { name: 'Midtown Mall',                  x: 70, y: 92, w: 9,  h: 5, door: [4, 4], loot: 3, kind: 'mall' },
    { name: 'The Lakefront Hotel',           x: 54, y: 100,w: 9,  h: 5, door: [4, 4], loot: 2, kind: 'hotel' },
    { name: 'Tesoro Gas Station',            x: 60, y: 76, w: 5,  h: 3, door: [2, 2], loot: 2, kind: 'gas' },
    { name: 'Holiday Fuel',                  x: 96, y: 76, w: 5,  h: 3, door: [2, 2], loot: 2, kind: 'gas' },
  ];

  global.ANCHORAGE = { T, MAP_W, MAP_H, AVENUES, STREETS, BUILDINGS };
})(typeof window !== 'undefined' ? window : this);
