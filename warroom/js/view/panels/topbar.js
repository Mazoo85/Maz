/* The top bar: who you are, the date, the speed, the map modes. */
import { formatDate } from '../../sim/clock.js';
import { flagSvg } from '../flags.js';

export function createTopbar({ send, onMode, onShowCountry, onShowSaves }) {
  const el = document.getElementById('topbar');
  const dateEl = document.getElementById('date');
  const nameEl = document.getElementById('player-name');
  const flagEl = document.getElementById('player-flag');
  const speedButtons = [...el.querySelectorAll('button.speed')];
  speedButtons.forEach((b) => b.addEventListener('click', () => send({ type: 'set_speed', speed: Number(b.dataset.speed) })));
  nameEl.addEventListener('click', onShowCountry);
  document.getElementById('saves-button').addEventListener('click', onShowSaves);

  const modes = document.getElementById('mapmodes');
  modes.querySelectorAll('button').forEach((b) => b.addEventListener('click', () => {
    modes.querySelectorAll('button').forEach((x) => x.classList.toggle('on', x === b));
    onMode(b.dataset.mode);
  }));

  window.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT' || !api.state) return;
    if (e.key === ' ') { e.preventDefault(); send({ type: api.state.speed ? 'pause' : 'resume' }); }
    else if (e.key >= '1' && e.key <= '5') send({ type: 'set_speed', speed: Number(e.key) });
    else if (e.key === '+' || e.key === '=') send({ type: 'set_speed', speed: Math.min(5, api.state.speed + 1) });
    else if (e.key === '-') send({ type: 'set_speed', speed: Math.max(0, api.state.speed - 1) });
  });

  let lastHour = -1, lastSpeed = -1, lastPlayer = '?';
  const api = {
    state: null,
    update(state) {
      api.state = state;
      if (state.hour !== lastHour) { dateEl.textContent = formatDate(state.hour); lastHour = state.hour; }
      if (state.speed !== lastSpeed) { speedButtons.forEach((b) => b.classList.toggle('on', Number(b.dataset.speed) === state.speed)); lastSpeed = state.speed; }
      if (state.player !== lastPlayer) {
        lastPlayer = state.player;
        const c = state.player ? state.countries[state.player] : null;
        nameEl.textContent = c ? c.name : 'Observer';
        flagEl.innerHTML = c ? flagSvg(c.flag) : '';
      }
    },
    show() { el.hidden = false; modes.hidden = false; },
  };
  return api;
}
