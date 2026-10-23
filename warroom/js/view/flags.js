/* Simple SVG flags drawn from a colour description, so no flag images are needed. */
export function flagSvg(flag) {
  const c = flag.colors;
  const w = 30, h = 20;
  let body = '';
  switch (flag.kind) {
    case 'vtricolor': body = `<rect width="10" height="${h}" fill="${c[0]}"/><rect x="10" width="10" height="${h}" fill="${c[1]}"/><rect x="20" width="10" height="${h}" fill="${c[2]}"/>`; break;
    case 'htricolor': body = `<rect width="${w}" height="7" fill="${c[0]}"/><rect y="7" width="${w}" height="6" fill="${c[1]}"/><rect y="13" width="${w}" height="7" fill="${c[2]}"/>`; break;
    case 'bicolor': body = `<rect width="${w}" height="10" fill="${c[0]}"/><rect y="10" width="${w}" height="10" fill="${c[1]}"/>`; break;
    case 'vbicolor': body = `<rect width="12" height="${h}" fill="${c[0]}"/><rect x="12" width="18" height="${h}" fill="${c[1]}"/>`; break;
    case 'cross': body = `<rect width="${w}" height="${h}" fill="${c[0]}"/><rect x="9" width="4" height="${h}" fill="${c[1]}"/><rect y="8" width="${w}" height="4" fill="${c[1]}"/><rect x="10" width="2" height="${h}" fill="${c[2]}"/><rect y="9" width="${w}" height="2" fill="${c[2]}"/>`; break;
    case 'disc': body = `<rect width="${w}" height="${h}" fill="${c[0]}"/><circle cx="15" cy="10" r="6" fill="${c[1]}"/>`; break;
    case 'canton': body = `<rect width="${w}" height="${h}" fill="${c[0]}"/><rect y="4" width="${w}" height="3" fill="${c[1]}"/><rect y="10" width="${w}" height="3" fill="${c[1]}"/><rect y="16" width="${w}" height="3" fill="${c[1]}"/><rect width="13" height="10" fill="${c[2]}"/>`; break;
    case 'stripes': body = `<rect width="${w}" height="${h}" fill="${c[0]}"/>` + [2, 6, 10, 14].map((y) => `<rect y="${y}" width="${w}" height="2" fill="${c[1]}"/>`).join('') + (c[2] ? `<rect y="8" width="${w}" height="4" fill="${c[2]}"/>` : ''); break;
    default: body = `<rect width="${w}" height="${h}" fill="${c[0]}"/>`;
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none">${body}</svg>`;
}
