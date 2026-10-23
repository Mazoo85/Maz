/*
 * Commands are the only way anything changes the game state. The view sends
 * them, the AI sends them, and a multiplayer relay will only ever forward them.
 *
 * validateCommand returns null when the command is allowed, or a short reason in
 * plain words the interface can show on a disabled button.
 */
import { SPEED_HOURS_PER_SECOND } from './clock.js';

const HANDLERS = {
  set_speed: {
    validate(state, cmd) {
      if (!Number.isInteger(cmd.speed) || cmd.speed < 0 || cmd.speed >= SPEED_HOURS_PER_SECOND.length) {
        return `speed must be 0 to ${SPEED_HOURS_PER_SECOND.length - 1}`;
      }
      return null;
    },
    apply(state, cmd) { state.speed = cmd.speed; },
  },
  pause: {
    validate() { return null; },
    apply(state) { state.speed = 0; },
  },
  resume: {
    validate(state, cmd) {
      if (cmd.speed !== undefined) return HANDLERS.set_speed.validate(state, cmd);
      return null;
    },
    apply(state, cmd) { state.speed = cmd.speed === undefined ? Math.max(1, state.lastSpeed || 1) : cmd.speed; },
  },
  select_country: {
    validate(state, cmd) {
      if (cmd.tag !== null && !state.countries[cmd.tag]) return `no country "${cmd.tag}"`;
      return null;
    },
    apply(state, cmd) { state.player = cmd.tag; },
  },
};

export const COMMAND_TYPES = Object.keys(HANDLERS);

export function validateCommand(state, cmd) {
  if (!cmd || typeof cmd !== 'object') return 'not a command';
  const h = HANDLERS[cmd.type];
  if (!h) return `unknown command "${cmd.type}"`;
  return h.validate(state, cmd);
}

export function applyCommand(state, cmd) {
  const reason = validateCommand(state, cmd);
  if (reason) throw new Error(`command ${cmd && cmd.type} refused: ${reason}`);
  if (cmd.type === 'set_speed' && cmd.speed > 0) state.lastSpeed = cmd.speed;
  HANDLERS[cmd.type].apply(state, cmd);
  state.log.push({ hour: state.hour, cmd });
  if (state.log.length > 10000) state.log.splice(0, state.log.length - 10000);
}
