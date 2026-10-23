/*
 * One tick is one hour. Subsystems run in a fixed order so a game replays the
 * same on every machine. Each subsystem is a function (state, data, hour); none
 * of them call each other.
 */
import { isNewDay } from './clock.js';
import { buildModifiers } from './modifiers.js';
import { tickWeather } from './weather.js';

export function tick(state, data) {
  const hour = state.hour;
  if (isNewDay(hour)) {
    state.modifiers = buildModifiers(state, data);
    tickWeather(state, data, hour);
  }
  state.hour = hour + 1;
}

export function advance(state, data, hours) {
  for (let i = 0; i < hours; i++) tick(state, data);
}
