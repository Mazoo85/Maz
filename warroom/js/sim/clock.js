/*
 * The game clock. One tick is one hour; hour 0 is 1 January 1936, 00:00.
 * Everything is integer arithmetic from that origin, so dates are exact and
 * the same on every machine.
 */

export const START = { year: 1936, month: 1, day: 1 };

export const MONTH_NAMES = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
export const MONTH_NAMES_LONG = ['January', 'February', 'March', 'April', 'May', 'June', 'July',
  'August', 'September', 'October', 'November', 'December'];
export const WEEKDAY_NAMES = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];

/** Hours of game time that pass per real second at each speed 0..5. */
export const SPEED_HOURS_PER_SECOND = [0, 1, 2, 6, 12, 48];

export function isLeapYear(year) {
  return (year % 4 === 0 && year % 100 !== 0) || year % 400 === 0;
}

export function daysInMonth(year, month) {
  if (month === 2) return isLeapYear(year) ? 29 : 28;
  return [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31][month - 1];
}

function daysInYear(year) {
  return isLeapYear(year) ? 366 : 365;
}

/** Days from 1 Jan 1936 to the given civil date (may be negative before 1936). */
export function dateToDay({ year, month, day }) {
  let days = 0;
  if (year >= START.year) {
    for (let y = START.year; y < year; y++) days += daysInYear(y);
  } else {
    for (let y = year; y < START.year; y++) days -= daysInYear(y);
  }
  for (let m = 1; m < month; m++) days += daysInMonth(year, m);
  return days + (day - 1);
}

export function dateToHour({ year, month, day, hour = 0 }) {
  return dateToDay({ year, month, day }) * 24 + hour;
}

export function hourToDate(hourIndex) {
  let days = Math.floor(hourIndex / 24);
  const hour = hourIndex - days * 24;
  let year = START.year;
  while (days >= daysInYear(year)) { days -= daysInYear(year); year++; }
  while (days < 0) { year--; days += daysInYear(year); }
  const dayOfYear = days + 1;
  let month = 1;
  while (days >= daysInMonth(year, month)) { days -= daysInMonth(year, month); month++; }
  const day = days + 1;
  // 1 January 1936 was a Wednesday (index 2 with Monday = 0).
  const weekday = (((Math.floor(hourIndex / 24) + 2) % 7) + 7) % 7;
  return { year, month, day, hour, dayOfYear, weekday };
}

export function formatDate(hourIndex, { long = false } = {}) {
  const d = hourToDate(hourIndex);
  const hh = String(d.hour).padStart(2, '0');
  const mon = long ? MONTH_NAMES_LONG[d.month - 1] : MONTH_NAMES[d.month - 1];
  return `${d.day} ${mon} ${d.year}, ${hh}:00`;
}

export function isNewDay(hourIndex) { return hourIndex % 24 === 0; }
export function isNewMonth(hourIndex) {
  if (hourIndex % 24 !== 0) return false;
  return hourToDate(hourIndex).day === 1;
}
export function isNewYear(hourIndex) {
  if (hourIndex % 24 !== 0) return false;
  const d = hourToDate(hourIndex);
  return d.day === 1 && d.month === 1;
}
