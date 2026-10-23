/*
 * A small schema checker for the data files. Errors name the path to the bad
 * value, e.g. "states[12].owner: expected string". No dependencies.
 */
export function check(value, schema, path = 'value') {
  const t = schema.type;
  if (schema.optional && (value === undefined || value === null)) return;
  if (t === 'object') {
    if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${path}: expected object`);
    for (const k of schema.required || []) {
      if (value[k] === undefined) throw new Error(`${path}.${k}: missing`);
    }
    for (const [k, sub] of Object.entries(schema.props || {})) {
      if (value[k] !== undefined) check(value[k], sub, `${path}.${k}`);
    }
    if (schema.values) {
      for (const [k, v] of Object.entries(value)) check(v, schema.values, `${path}.${k}`);
    }
    return;
  }
  if (t === 'array') {
    if (!Array.isArray(value)) throw new Error(`${path}: expected array`);
    if (schema.items) value.forEach((v, i) => { if (!(schema.nullable && v === null)) check(v, schema.items, `${path}[${i}]`); });
    return;
  }
  if (t === 'string') {
    if (typeof value !== 'string') throw new Error(`${path}: expected string`);
  } else if (t === 'number' || t === 'integer') {
    if (typeof value !== 'number' || Number.isNaN(value)) throw new Error(`${path}: expected number`);
    if (t === 'integer' && !Number.isInteger(value)) throw new Error(`${path}: expected integer`);
    if (schema.min !== undefined && value < schema.min) throw new Error(`${path}: ${value} is below ${schema.min}`);
    if (schema.max !== undefined && value > schema.max) throw new Error(`${path}: ${value} is above ${schema.max}`);
  } else if (t === 'boolean') {
    if (typeof value !== 'boolean') throw new Error(`${path}: expected boolean`);
  } else if (t === 'any') {
    // anything goes
  } else {
    throw new Error(`${path}: schema has unknown type "${t}"`);
  }
  if (schema.enum && !schema.enum.includes(value)) throw new Error(`${path}: "${value}" is not one of ${schema.enum.join(', ')}`);
}

export const TERRAIN = ['plains', 'forest', 'hills', 'mountain', 'urban', 'jungle', 'marsh', 'desert', 'wasteland', 'ocean', 'lake'];
export const CLIMATES = ['tropical', 'arid', 'temperate', 'continental', 'polar', 'ocean'];
export const IDEOLOGIES = ['democratic', 'fascist', 'communist', 'non_aligned'];

const PROVINCE = {
  type: 'object', required: ['id', 'name', 'type', 'terrain', 'state', 'region', 'vp', 'x', 'y', 'lat', 'lon'],
  props: {
    id: { type: 'integer', min: 1 }, name: { type: 'string' },
    type: { type: 'string', enum: ['land', 'sea', 'lake', 'wasteland'] },
    terrain: { type: 'string', enum: TERRAIN }, state: { type: 'integer', min: 0 }, region: { type: 'integer', min: 0 },
    coastal: { type: 'boolean', optional: true }, vp: { type: 'integer', min: 0 },
    x: { type: 'integer', min: 0 }, y: { type: 'integer', min: 0 }, lat: { type: 'number', min: -90, max: 90 }, lon: { type: 'number', min: -180, max: 180 },
  },
};

export const SCHEMAS = {
  provinces: { type: 'array', nullable: true, items: PROVINCE },
  adjacency: { type: 'array', items: { type: 'array', items: { type: 'integer', min: 1 } } },
  rivers: { type: 'array', items: { type: 'array', items: { type: 'integer', min: 1 } } },
  states: {
    type: 'array', items: {
      type: 'object', required: ['id', 'name', 'owner', 'cores', 'provinces', 'capital_province', 'manpower', 'infrastructure'],
      props: {
        id: { type: 'integer', min: 1 }, name: { type: 'string' }, owner: { type: 'string' },
        cores: { type: 'array', items: { type: 'string' } }, provinces: { type: 'array', items: { type: 'integer', min: 1 } },
        capital_province: { type: 'integer', min: 1 }, manpower: { type: 'integer', min: 0 }, infrastructure: { type: 'integer', min: 0, max: 5 },
        resources: { type: 'object', optional: true, values: { type: 'number', min: 0 } },
      },
    },
  },
  regions: {
    type: 'array', items: {
      type: 'object', required: ['id', 'name', 'type', 'climate', 'provinces'],
      props: { id: { type: 'integer', min: 1 }, name: { type: 'string' }, type: { type: 'string', enum: ['land', 'sea'] },
        climate: { type: 'string', enum: CLIMATES }, provinces: { type: 'array', items: { type: 'integer', min: 1 } } },
    },
  },
  countries: {
    type: 'array', items: {
      type: 'object', required: ['tag', 'name', 'adjective', 'color', 'ideology', 'leader', 'capital_state', 'capital_province', 'flag'],
      props: { tag: { type: 'string' }, name: { type: 'string' }, adjective: { type: 'string' }, color: { type: 'string' },
        ideology: { type: 'string', enum: IDEOLOGIES }, leader: { type: 'string' },
        capital_state: { type: 'integer', min: 1 }, capital_province: { type: 'integer', min: 1 },
        flag: { type: 'object', required: ['kind', 'colors'], props: { kind: { type: 'string' }, colors: { type: 'array', items: { type: 'string' } } } } },
    },
  },
  weather: { type: 'object', required: ['types', 'climates'], props: { types: { type: 'array', items: { type: 'string' } }, climates: { type: 'object', values: { type: 'object', required: ['from', 'season'] } } } },
  terrain: { type: 'object', required: ['types'], props: { types: { type: 'array', items: { type: 'string', enum: TERRAIN } } } },
  scenario: {
    type: 'object', required: ['start_hour', 'armies'],
    props: { start_hour: { type: 'integer', min: 0 }, armies: { type: 'object', values: { type: 'array', items: { type: 'object', required: ['state', 'divisions'], props: { state: { type: 'integer', min: 1 }, divisions: { type: 'integer', min: 0 } } } } } },
  },
};
