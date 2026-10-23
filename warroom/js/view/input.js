/*
 * Pointer input on the globe: a tap (little movement) picks a province; moving the
 * mouse hovers. Dragging is left to the camera controls.
 */
export function attachInput(canvas, { onPick, onHover }) {
  let down = null;
  canvas.addEventListener('pointerdown', (e) => { down = { x: e.clientX, y: e.clientY, id: e.pointerId, t: performance.now() }; });
  canvas.addEventListener('pointerup', (e) => {
    if (!down || e.pointerId !== down.id) return;
    const moved = Math.hypot(e.clientX - down.x, e.clientY - down.y);
    const held = performance.now() - down.t;
    down = null;
    if (moved < 7 && held < 600) onPick(e.clientX, e.clientY);
  });
  canvas.addEventListener('pointercancel', () => { down = null; });
  let hoverTimer = 0;
  canvas.addEventListener('pointermove', (e) => {
    if (e.pointerType !== 'mouse' || down) return;
    if (hoverTimer) return;
    hoverTimer = requestAnimationFrame(() => { hoverTimer = 0; onHover(e.clientX, e.clientY); });
  });
  canvas.addEventListener('pointerleave', () => onHover(-1, -1));
}
