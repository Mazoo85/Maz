// The bundled data of the desktop build (scripts/fetch-data.js) is made on the phone instead, the first time a
// layer needs it, from the same free sources, then kept. Same queries and the same shapes as the desktop files.
export const BATTLES_QUERY = `SELECT ?b ?bLabel ?coord ?date ?warLabel ?article WHERE {
    ?b wdt:P31/wdt:P279* wd:Q178561; wdt:P625 ?coord.
    OPTIONAL { ?b wdt:P585 ?d1 } OPTIONAL { ?b wdt:P580 ?d2 } BIND(COALESCE(?d1, ?d2) AS ?date) FILTER(BOUND(?date))
    OPTIONAL { ?b wdt:P361 ?war }
    OPTIONAL { ?article schema:about ?b; schema:isPartOf <https://en.wikipedia.org/> }
    SERVICE wikibase:label { bd:serviceParam wikibase:language "en". } }`;
export const BATTLES_URL = 'https://query.wikidata.org/sparql?format=json&query=' + encodeURIComponent(BATTLES_QUERY);
export const VOLCANOES_URL = 'https://webservices.volcano.si.edu/geoserver/GVP-VOTW/ows?service=WFS&version=2.0.0&request=GetFeature&typeName=GVP-VOTW:Smithsonian_VOTW_Holocene_Volcanoes&outputFormat=application/json';

export function battlesFromSparql(data) {
  const byId = new Map();
  for (const r of data.results.bindings) {
    const id = r.b.value.split('/').pop();
    const m = ((r.coord && r.coord.value) || '').match(/Point\(([-\d.eE]+) ([-\d.eE]+)\)/); if (!m) continue;
    if (!r.date) continue;
    const date = r.date.value;
    if (!/^[+-]?\d/.test(date)) continue;                               // "unknown value" comes through as a URL
    const year = parseInt(date.replace(/^\+/, ''), 10);
    const prev = byId.get(id);
    if (prev) { if (r.warLabel && !prev.war.includes(r.warLabel.value)) prev.war.push(r.warLabel.value); continue; }
    const name = r.bLabel ? r.bLabel.value : id;
    if (/^Q\d+$/.test(name)) continue;                                  // no English name
    const lon = +m[1], lat = +m[2];
    if (!(Math.abs(lat) <= 90 && Math.abs(lon) <= 180) || !Number.isInteger(year)) continue;
    byId.set(id, { id, name, lon: +lon.toFixed(4), lat: +lat.toFixed(4), year, date: date.replace(/^\+/, '').slice(0, date.startsWith('-') ? 11 : 10), war: r.warLabel ? [r.warLabel.value] : [], wiki: r.article ? r.article.value : '' });
  }
  return [...byId.values()].sort((a, b) => a.year - b.year);
}

export function volcanoesFromWfs(j) {
  return j.features.filter(f => f.geometry && f.geometry.coordinates).map(f => ({ n: f.properties.Volcano_Number, name: f.properties.Volcano_Name, country: f.properties.Country, type: f.properties.Primary_Volcano_Type, last: f.properties.Last_Eruption_Year, elev: f.properties.Elevation, lat: +(+f.geometry.coordinates[1]).toFixed(4), lon: +(+f.geometry.coordinates[0]).toFixed(4) }));
}
