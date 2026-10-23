import { test, assert, eq, done } from './harness.js';
import { hourToDate, dateToHour, formatDate, isNewDay, isNewMonth, isNewYear, isLeapYear, SPEED_HOURS_PER_SECOND } from '../js/sim/clock.js';

test('hour 0 is 1 January 1936, 00:00, a Wednesday', () => {
  const d = hourToDate(0);
  eq(d.year, 1936); eq(d.month, 1); eq(d.day, 1); eq(d.hour, 0); eq(d.weekday, 2); eq(d.dayOfYear, 1);
});

test('1936 is a leap year and 29 February exists', () => {
  assert(isLeapYear(1936)); assert(!isLeapYear(1937)); assert(!isLeapYear(1900)); assert(isLeapYear(2000));
  const h = dateToHour({ year: 1936, month: 2, day: 29, hour: 12 });
  const d = hourToDate(h);
  eq(d.month, 2); eq(d.day, 29); eq(d.hour, 12);
  eq(hourToDate(h + 12).month, 3);
});

test('dates round-trip through hours up to 1948 and before 1936', () => {
  for (let h = -24 * 400; h < 24 * 366 * 13; h += 7919) {
    const d = hourToDate(h);
    eq(dateToHour(d), h, 'round trip at ' + h);
  }
});

test('formatDate reads naturally', () => {
  eq(formatDate(24 * 31), '1 Feb 1936, 00:00');
  eq(formatDate(dateToHour({ year: 1939, month: 9, day: 1, hour: 4 })), '1 Sep 1939, 04:00');
  eq(formatDate(dateToHour({ year: 1939, month: 9, day: 1, hour: 4 }), { long: true }), '1 September 1939, 04:00');
});

test('new day / month / year flags', () => {
  assert(isNewDay(24)); assert(!isNewDay(25));
  assert(isNewMonth(24 * 31)); assert(!isNewMonth(24 * 30));
  assert(isNewYear(24 * 366)); assert(!isNewYear(24 * 365));
});

test('speeds are increasing and speed 0 is paused', () => {
  eq(SPEED_HOURS_PER_SECOND.length, 6);
  eq(SPEED_HOURS_PER_SECOND[0], 0);
  for (let i = 1; i < 6; i++) assert(SPEED_HOURS_PER_SECOND[i] > SPEED_HOURS_PER_SECOND[i - 1]);
});

done('clock');
