/*
 * SCRIPT FORGE — the sets.
 * ------------------------
 * Fifteen places a scene can happen, drawn in code. Moved out of film-art.js,
 * which now keeps only the palette and the film-stock effects.
 *
 * Each set is drawn in three planes — back, mid, fore — so the camera pans
 * and pushes read as a real space instead of a rigid picture:
 *
 *   back  sky, far walls, distant scenery, windows and what is beyond them.
 *   mid   the floor, the furniture and structures the characters stand among.
 *   fore  one dark element close to the lens, partly outside the frame.
 *
 * FilmPlayer draws back, then mid, then the figures, then fore, each under
 * its own transform scaled by FilmSets.PARALLAX.
 *
 * Exposed as window.FilmSets (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var Art = root.FilmArt || (typeof require !== 'undefined' ? require('./film-art.js') : {});
  var rgb = Art.rgb;
  var mix = Art.mix;

  var SETS = {};

  /* How fast each plane moves under the camera. Back barely shifts; fore
   * swings, which is what tells the eye it is close. */
  var PARALLAX = { back: 0.35, mid: 1, fore: 1.7 };

  /* The fore element's ink. Every set's `fore` fills or strokes at one flat
   * near-black alpha; `lift`, when true, is the close/two-shot "out of
   * focus" read (see film-player.js's rack-focus comment) — hazier and
   * lower-contrast, never darker, because black has no room left to darken.
   *
   * This used to be a separate pass: paint the shape solid black, then
   * `source-atop` a `pal.deep` fill over it at globalAlpha 0.55 on an
   * isolated offscreen layer. Both alpha-composite ("source-atop" and the
   * plain paint before it) are linear blends, and black contributes nothing
   * to a linear blend (mixing toward black is just scaling), so the two
   * steps collapse algebraically to one flat color at the *same* alpha the
   * shape already painted at: mix(black, pal.deep, 0.55). Painting that
   * color directly costs exactly what painting black does — no extra
   * canvas, no extra draw call — where the offscreen isolate-and-composite
   * pass measured at ~4-5ms per close-up at 1920x1080, most of a whole
   * frame's 6ms budget for one shot.
   */
  function foreInk(p, alpha, lift) {
    return lift ? rgb(mix([0, 0, 0], p.deep, 0.55), alpha) : 'rgba(0,0,0,' + alpha + ')';
  }

  SETS.lighthouse = {
    back: function (ctx, p, n) {
      // The lamp room shell, the sea and the sky a long way through the glass.
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
      ctx.fillStyle = rgb(p.sky); ctx.fillRect(60, 60, 880, 150);
      ctx.fillStyle = rgb(mix(p.deep, p.key, 0.12)); ctx.fillRect(60, 176, 880, 120);
      for (var i = 0; i < 22; i++) {
        var s = n[i];
        ctx.fillStyle = rgb(p.key, 0.08 + s[2] * 0.12);
        ctx.fillRect(60 + s[0] * 860, 182 + s[1] * 108, 40 + s[2] * 70, 2);
      }
      // floor, far below the lens
      ctx.fillStyle = rgb(p.deep); ctx.fillRect(0, 322, 1000, 98);
      ctx.fillStyle = rgb(p.key, 0.06); ctx.fillRect(0, 322, 1000, 4);
    },
    mid: function (ctx, p, n) {
      // glazing bars and the lit lens itself, where the characters stand
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(0, 0, 1000, 62); ctx.fillRect(0, 292, 1000, 30);
      for (var g = 0; g < 5; g++) ctx.fillRect(60 + g * 220, 60, 16, 236);
      var beam = ctx.createLinearGradient(300, 190, 60, 120);
      beam.addColorStop(0, rgb(p.key, 0.45)); beam.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = beam;
      ctx.beginPath(); ctx.moveTo(300, 150); ctx.lineTo(60, 92); ctx.lineTo(60, 210); ctx.lineTo(300, 220);
      ctx.closePath(); ctx.fill();
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(268, 120, 120, 180);
      ctx.fillStyle = rgb(p.key, 0.92);
      ctx.beginPath(); ctx.ellipse(328, 190, 44, 64, 0, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = rgb(p.deep, 0.55);
      for (var r = 0; r < 5; r++) ctx.fillRect(284, 136 + r * 26, 88, 6);
    },
    fore: function (ctx, p, n, lift) {
      // The rail around the gallery, close to the lens.
      ctx.fillStyle = foreInk(p, 0.9, lift);
      ctx.fillRect(-40, 388, 1080, 60);
      ctx.fillRect(-40, 348, 60, 100);
      ctx.fillRect(980, 348, 60, 100);
    }
  };

  SETS.kitchen = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(mix(p.deep, p.sky, 0.25));
      ctx.fillRect(0, 0, 1000, 420);
      var g = ctx.createLinearGradient(120, 40, 420, 300);
      g.addColorStop(0, rgb(p.key, 0.55));
      g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = rgb(p.key, 0.75); ctx.fillRect(140, 50, 220, 170);
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(244, 50, 10, 170); ctx.fillRect(140, 128, 220, 10);
      ctx.fillStyle = g; ctx.fillRect(120, 40, 400, 300);
    },
    mid: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(0, 300, 1000, 22);
      ctx.fillRect(600, 60, 340, 120);
      ctx.fillStyle = rgb(p.deep); ctx.fillRect(770, 60, 8, 120);
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 322, 1000, 98);
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(660, 268, 44, 32);
      ctx.fillRect(740, 282, 24, 18); ctx.fillRect(784, 282, 24, 18);
    },
    fore: function (ctx, p, n, lift) {
      // A table edge across the bottom of frame, for the camera to move against.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, 386, 1080, 60);
      ctx.fillRect(120, 372, 300, 16);
    }
  };

  SETS.room = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(mix(p.deep, p.sky, 0.18));
      ctx.fillRect(0, 0, 1000, 420);
      // doorway, lit from beyond
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(620, 40, 180, 300);
      ctx.fillStyle = rgb(p.key, 0.5); ctx.fillRect(632, 52, 156, 276);
      ctx.fillStyle = rgb(p.deep); ctx.fillRect(648, 68, 124, 244);
    },
    mid: function (ctx, p, n) {
      // lamp pool on the floor, and the table it falls on
      var g = ctx.createRadialGradient(250, 300, 10, 250, 300, 260);
      g.addColorStop(0, rgb(p.key, 0.32)); g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = g; ctx.fillRect(0, 140, 560, 280);
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(200, 240, 100, 12); ctx.fillRect(244, 252, 12, 60);
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 330, 1000, 90);
    },
    fore: function (ctx, p, n, lift) {
      // A doorframe edge, close on the left of frame.
      ctx.fillStyle = foreInk(p, 0.9, lift);
      ctx.fillRect(-40, -20, 90, 460);
      ctx.fillRect(-40, 380, 1080, 60);
    }
  };

  SETS.corridor = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
      // one-point perspective: receding doorframes, the deepest four
      for (var i = 6; i >= 3; i--) {
        var w = 120 + i * 130, h = 90 + i * 46;
        var x = 500 - w / 2, y = 210 - h / 2;
        ctx.fillStyle = rgb(mix(p.shadow, p.deep, i / 7));
        ctx.fillRect(x, y, w, h);
        ctx.fillStyle = rgb(p.key, 0.06 + i * 0.02);
        ctx.fillRect(x, y, w, 6);
      }
      var g = ctx.createRadialGradient(500, 210, 8, 500, 210, 190);
      g.addColorStop(0, rgb(p.key, 0.5)); g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = g; ctx.fillRect(300, 60, 400, 320);
    },
    mid: function (ctx, p, n) {
      // the nearest two doorframes, where the characters actually stand, and the floor
      for (var i = 2; i >= 1; i--) {
        var w = 120 + i * 130, h = 90 + i * 46;
        var x = 500 - w / 2, y = 210 - h / 2;
        ctx.fillStyle = rgb(mix(p.shadow, p.deep, i / 7));
        ctx.fillRect(x, y, w, h);
        ctx.fillStyle = rgb(p.key, 0.06 + i * 0.02);
        ctx.fillRect(x, y, w, 6);
      }
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 340, 1000, 80);
    },
    fore: function (ctx, p, n, lift) {
      // The nearest doorframe, right at the lens, top and sides only so the
      // corridor still reads as open ahead.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, -20, 1080, 60);
      ctx.fillRect(-40, -20, 90, 460);
      ctx.fillRect(950, -20, 90, 460);
    }
  };

  SETS.woods = {
    back: function (ctx, p, n) {
      var g = ctx.createLinearGradient(0, 0, 0, 340);
      g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(p.deep));
      ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 340);
      ctx.fillStyle = rgb(p.key, 0.16);
      ctx.beginPath(); ctx.arc(300, 96, 44, 0, Math.PI * 2); ctx.fill();
      // canopy closing over the top of the frame
      ctx.fillStyle = rgb(p.ink, 0.85);
      ctx.beginPath();
      ctx.moveTo(0, 0); ctx.lineTo(1000, 0); ctx.lineTo(1000, 70);
      for (var c = 10; c >= 0; c--) {
        var q = n[c + 30];
        ctx.quadraticCurveTo(c * 100 + 50, 40 + q[0] * 90, c * 100, 60 + q[1] * 40);
      }
      ctx.closePath(); ctx.fill();
      // trunks: tapered, in two far depths, with a low branch or two
      for (var layer = 0; layer < 2; layer++) {
        var alpha = 0.40 + layer * 0.28;
        var count = 7 - layer;
        for (var i = 0; i < count; i++) {
          var s = n[layer * 9 + i];
          var x = 40 + ((i + layer * 0.4) / count) * 980 + s[0] * 60;
          var wBase = 14 + s[1] * 26 + layer * 12;
          var top = 30 + layer * 26;
          ctx.fillStyle = rgb(p.ink, alpha);
          ctx.beginPath();
          ctx.moveTo(x - wBase / 2, 360);
          ctx.lineTo(x - wBase * 0.28, top);
          ctx.lineTo(x + wBase * 0.28, top);
          ctx.lineTo(x + wBase / 2, 360);
          ctx.closePath(); ctx.fill();
          if (s[2] > 0.5) {
            ctx.save();
            ctx.strokeStyle = rgb(p.ink, alpha);
            ctx.lineWidth = 4 + layer * 2;
            ctx.beginPath();
            ctx.moveTo(x, 120 + s[1] * 70);
            ctx.lineTo(x + (s[0] > 0.5 ? 1 : -1) * (50 + s[2] * 60), 80 + s[0] * 60);
            ctx.stroke();
            ctx.restore();
          }
        }
      }
    },
    mid: function (ctx, p, n) {
      // the nearest trunks, standing among the characters, and the forest floor
      var layer = 2, alpha = 0.40 + layer * 0.28, count = 7 - layer;
      for (var i = 0; i < count; i++) {
        var s = n[layer * 9 + i];
        var x = 40 + ((i + layer * 0.4) / count) * 980 + s[0] * 60;
        var wBase = 14 + s[1] * 26 + layer * 12;
        var top = 30 + layer * 26;
        ctx.fillStyle = rgb(p.ink, alpha);
        ctx.beginPath();
        ctx.moveTo(x - wBase / 2, 360);
        ctx.lineTo(x - wBase * 0.28, top);
        ctx.lineTo(x + wBase * 0.28, top);
        ctx.lineTo(x + wBase / 2, 360);
        ctx.closePath(); ctx.fill();
      }
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 336, 1000, 84);
      ctx.fillStyle = rgb(p.key, 0.05); ctx.fillRect(0, 336, 1000, 3);
    },
    fore: function (ctx, p, n, lift) {
      // A low branch reaching in from the edge of frame.
      ctx.strokeStyle = foreInk(p, 0.92, lift);
      ctx.lineWidth = 30;
      ctx.beginPath();
      ctx.moveTo(-40, 40); ctx.quadraticCurveTo(260, 120, 520, 30);
      ctx.stroke();
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, 400, 1080, 40);
    }
  };

  SETS.street = {
    back: function (ctx, p, n) {
      var g = ctx.createLinearGradient(0, 0, 0, 300);
      g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(mix(p.sky, p.key, 0.25)));
      ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 300);
      // skyline
      for (var i = 0; i < 16; i++) {
        var s = n[i];
        var w = 50 + s[1] * 90, h = 80 + s[2] * 190;
        ctx.fillStyle = rgb(p.ink, 0.92);
        ctx.fillRect(s[0] * 1000 - w / 2, 300 - h, w, h);
        for (var wnd = 0; wnd < 5; wnd++) {
          var q = n[(i * 5 + wnd + 20) % n.length];
          if (q[2] > 0.55) {
            ctx.fillStyle = rgb(p.key, 0.28 + q[0] * 0.3);
            ctx.fillRect(s[0] * 1000 - w / 2 + 10 + q[0] * (w - 24), 300 - h + 14 + q[1] * (h - 30), 8, 10);
          }
        }
      }
    },
    mid: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 300, 1000, 120);
      // streetlight pool
      var pool = ctx.createRadialGradient(760, 300, 6, 760, 300, 230);
      pool.addColorStop(0, rgb(p.key, 0.36)); pool.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = pool; ctx.fillRect(520, 160, 480, 260);
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(756, 90, 8, 210);
      ctx.fillStyle = rgb(p.key, 0.85); ctx.fillRect(736, 82, 48, 12);
    },
    fore: function (ctx, p, n, lift) {
      // A near lamppost at the edge of frame, and the curb.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-30, -20, 46, 460);
      ctx.fillRect(-40, 396, 1080, 44);
    }
  };

  SETS.field = {
    back: function (ctx, p, n) {
      var g = ctx.createLinearGradient(0, 0, 0, 300);
      g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(mix(p.sky, p.key, 0.4)));
      ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 300);
      ctx.fillStyle = rgb(p.key, 0.22);
      ctx.beginPath(); ctx.arc(720, 250, 60, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = rgb(p.ink, 0.7);
      ctx.beginPath(); ctx.moveTo(0, 300); ctx.lineTo(260, 236); ctx.lineTo(520, 300); ctx.closePath(); ctx.fill();
    },
    mid: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.deep); ctx.fillRect(0, 296, 1000, 124);
      // fence posts running to the horizon
      for (var i = 0; i < 10; i++) {
        ctx.fillStyle = rgb(p.ink, 0.85);
        ctx.fillRect(60 + i * 96, 292 - i * 2, 7, 30 + i * 4);
      }
    },
    fore: function (ctx, p, n, lift) {
      // The nearest fence post and rail, right at the lens.
      ctx.fillStyle = foreInk(p, 0.9, lift);
      ctx.fillRect(20, 260, 20, 160);
      ctx.fillRect(-40, 330, 1080, 26);
      ctx.fillRect(-40, 402, 1080, 30);
    }
  };

  SETS.vehicle = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
      // windscreen: road running away
      var g = ctx.createLinearGradient(0, 40, 0, 260);
      g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(p.deep));
      ctx.fillStyle = g; ctx.fillRect(90, 40, 820, 220);
      ctx.fillStyle = rgb(p.ink, 0.9);
      ctx.beginPath(); ctx.moveTo(300, 260); ctx.lineTo(470, 150); ctx.lineTo(530, 150); ctx.lineTo(700, 260);
      ctx.closePath(); ctx.fill();
      ctx.fillStyle = rgb(p.key, 0.5);
      for (var i = 0; i < 4; i++) ctx.fillRect(494, 168 + i * 26, 12, 14 - i * 2);
    },
    mid: function (ctx, p, n) {
      // dashboard
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 250, 1000, 170);
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(60, 260, 880, 24);
      ctx.fillStyle = rgb(p.accent, 0.8); ctx.beginPath(); ctx.arc(220, 300, 18, 0, Math.PI * 2); ctx.fill();
      ctx.strokeStyle = rgb(p.ink); ctx.lineWidth = 14;
      ctx.beginPath(); ctx.arc(700, 360, 80, Math.PI, Math.PI * 2); ctx.stroke();
    },
    fore: function (ctx, p, n, lift) {
      // The near window pillar and the sill, right at the lens.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, -20, 70, 460);
      ctx.fillRect(-40, 400, 1080, 40);
    }
  };

  SETS.industrial = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
      // rafters + hanging lamps
      ctx.fillStyle = rgb(p.ink);
      for (var i = 0; i < 5; i++) ctx.fillRect(i * 220 + 40, 0, 18, 340);
      ctx.fillRect(0, 44, 1000, 12);
      for (var l = 0; l < 3; l++) {
        var x = 180 + l * 320;
        ctx.fillStyle = rgb(p.ink); ctx.fillRect(x - 2, 56, 4, 60);
        ctx.fillStyle = rgb(p.key, 0.9); ctx.fillRect(x - 22, 116, 44, 12);
        var g = ctx.createRadialGradient(x, 128, 6, x, 128, 220);
        g.addColorStop(0, rgb(p.key, 0.30)); g.addColorStop(1, rgb(p.key, 0));
        ctx.fillStyle = g; ctx.fillRect(x - 230, 100, 460, 320);
      }
    },
    mid: function (ctx, p, n) {
      // crates and the floor the characters stand on
      ctx.fillStyle = rgb(p.ink, 0.95);
      ctx.fillRect(60, 250, 130, 90); ctx.fillRect(200, 286, 90, 54); ctx.fillRect(820, 260, 120, 80);
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 336, 1000, 84);
    },
    fore: function (ctx, p, n, lift) {
      // A near crate stack, low left, close to the lens.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, 300, 220, 140);
      ctx.fillRect(-40, 406, 1080, 34);
    }
  };

  SETS.office = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(mix(p.deep, p.sky, 0.15)); ctx.fillRect(0, 0, 1000, 420);
      // blinds
      for (var i = 0; i < 14; i++) {
        ctx.fillStyle = rgb(p.key, 0.30 - i * 0.008);
        ctx.fillRect(80, 40 + i * 18, 380, 9);
      }
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(70, 30, 12, 280);
    },
    mid: function (ctx, p, n) {
      // desk + lamp + shelves
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(520, 60, 420, 200);
      ctx.fillStyle = rgb(p.deep);
      for (var s = 0; s < 3; s++) ctx.fillRect(530, 74 + s * 62, 400, 10);
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(120, 286, 420, 18); ctx.fillRect(140, 304, 16, 60);
      var g = ctx.createRadialGradient(330, 280, 8, 330, 280, 200);
      g.addColorStop(0, rgb(p.key, 0.34)); g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = g; ctx.fillRect(120, 140, 420, 280);
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 340, 1000, 80);
    },
    fore: function (ctx, p, n, lift) {
      // The near edge of someone else's desk, low right.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(760, 368, 280, 52);
      ctx.fillRect(-40, 402, 1080, 38);
    }
  };

  SETS.bar = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(mix(p.deep, p.shadow, 0.4)); ctx.fillRect(0, 0, 1000, 420);
      // booth windows with night outside
      ctx.fillStyle = rgb(p.sky, 0.8); ctx.fillRect(60, 60, 260, 150);
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(180, 60, 10, 150);
      // back bar bottles
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(560, 70, 380, 150);
      for (var i = 0; i < 12; i++) {
        var s = n[i];
        ctx.fillStyle = rgb(p.accent, 0.35 + s[2] * 0.4);
        ctx.fillRect(580 + i * 30, 120 + s[0] * 20, 12, 74 - s[1] * 20);
      }
    },
    mid: function (ctx, p, n) {
      // counter and stools, where the characters sit
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(0, 290, 1000, 26);
      ctx.fillStyle = rgb(p.key, 0.22); ctx.fillRect(0, 290, 1000, 4);
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 316, 1000, 104);
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(300, 330, 12, 70); ctx.fillRect(276, 322, 60, 10);
      ctx.fillRect(660, 330, 12, 70); ctx.fillRect(636, 322, 60, 10);
    },
    fore: function (ctx, p, n, lift) {
      // The near edge of the counter, right at the lens.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, 384, 1080, 56);
      ctx.fillRect(60, 366, 220, 20);
    }
  };

  SETS.ship = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
      // viewport onto stars
      ctx.save();
      ctx.beginPath(); ctx.ellipse(500, 190, 330, 150, 0, 0, Math.PI * 2); ctx.clip();
      ctx.fillStyle = '#04060f'; ctx.fillRect(170, 40, 660, 300);
      for (var i = 0; i < 60; i++) {
        var s = n[i % n.length];
        ctx.fillStyle = rgb(p.key, 0.25 + s[2] * 0.7);
        ctx.fillRect(180 + s[0] * 640, 50 + s[1] * 280, 2 + s[2] * 2, 2 + s[2] * 2);
      }
      ctx.fillStyle = rgb(p.accent, 0.25);
      ctx.beginPath(); ctx.arc(700, 260, 90, 0, Math.PI * 2); ctx.fill();
      ctx.restore();
      ctx.strokeStyle = rgb(p.ink); ctx.lineWidth = 26;
      ctx.beginPath(); ctx.ellipse(500, 190, 330, 150, 0, 0, Math.PI * 2); ctx.stroke();
    },
    mid: function (ctx, p, n) {
      // console lights, where the crew stand
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(0, 330, 1000, 90);
      for (var c = 0; c < 10; c++) {
        var q = n[(c + 12) % n.length];
        ctx.fillStyle = rgb(q[2] > 0.6 ? p.accent : p.key, 0.5 + q[0] * 0.5);
        ctx.fillRect(90 + c * 88, 348, 26, 8);
      }
    },
    fore: function (ctx, p, n, lift) {
      // The lip of the console, close to the lens.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, 392, 1080, 48);
      ctx.fillRect(-40, 360, 60, 80);
      ctx.fillRect(980, 360, 60, 80);
    }
  };

  SETS.water = {
    back: function (ctx, p, n) {
      var g = ctx.createLinearGradient(0, 0, 0, 280);
      g.addColorStop(0, rgb(p.sky)); g.addColorStop(1, rgb(mix(p.sky, p.key, 0.35)));
      ctx.fillStyle = g; ctx.fillRect(0, 0, 1000, 280);
      ctx.fillStyle = rgb(mix(p.deep, p.key, 0.16)); ctx.fillRect(0, 280, 1000, 140);
      for (var i = 0; i < 30; i++) {
        var s = n[i];
        ctx.fillStyle = rgb(p.key, 0.08 + s[2] * 0.16);
        ctx.fillRect(s[0] * 1000, 286 + s[1] * 120, 60 + s[2] * 90, 3);
      }
      ctx.fillStyle = rgb(p.ink, 0.8); ctx.fillRect(760, 250, 90, 34); // far boat
    },
    mid: function (ctx, p, n) {
      // jetty, where the characters stand
      ctx.fillStyle = rgb(p.ink);
      ctx.fillRect(0, 300, 460, 16);
      for (var j = 0; j < 5; j++) ctx.fillRect(40 + j * 100, 316, 12, 70);
    },
    fore: function (ctx, p, n, lift) {
      // A near post and rail at the end of the jetty, close to the lens.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, 340, 100, 100);
      ctx.fillRect(-40, 400, 1080, 40);
    }
  };

  SETS.ward = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(mix(p.deep, [255, 255, 255], 0.10 + p.lift * 0.2));
      ctx.fillRect(0, 0, 1000, 420);
      ctx.fillStyle = rgb(p.key, 0.5); ctx.fillRect(120, 24, 300, 14); // strip light
      var g = ctx.createLinearGradient(0, 24, 0, 300);
      g.addColorStop(0, rgb(p.key, 0.24)); g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = g; ctx.fillRect(60, 24, 420, 300);
    },
    mid: function (ctx, p, n) {
      // curtain rail + curtain, and the bed the characters stand by
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(560, 40, 380, 8);
      for (var i = 0; i < 12; i++) {
        ctx.fillStyle = rgb(p.deep, 0.55 + (i % 2) * 0.2);
        ctx.fillRect(566 + i * 31, 48, 26, 250);
      }
      ctx.fillStyle = rgb(p.ink); ctx.fillRect(140, 280, 360, 20); ctx.fillRect(150, 300, 14, 70);
      ctx.fillRect(476, 300, 14, 70); ctx.fillRect(120, 210, 20, 90);
      ctx.fillStyle = rgb(p.accent, 0.8); ctx.fillRect(540, 250, 40, 10); // monitor blip
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 370, 1000, 50);
    },
    fore: function (ctx, p, n, lift) {
      // The near curtain, drawn half across the lens.
      ctx.fillStyle = foreInk(p, 0.9, lift);
      ctx.fillRect(-40, -20, 120, 460);
      ctx.fillRect(-40, 404, 1080, 36);
    }
  };

  SETS.chapel = {
    back: function (ctx, p, n) {
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 0, 1000, 420);
      // arched window
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(400, 320); ctx.lineTo(400, 130);
      ctx.arc(500, 130, 100, Math.PI, 0); ctx.lineTo(600, 320); ctx.closePath();
      ctx.clip();
      ctx.fillStyle = rgb(p.key, 0.75); ctx.fillRect(400, 30, 200, 300);
      ctx.fillStyle = rgb(p.accent, 0.45); ctx.fillRect(400, 150, 200, 60);
      ctx.restore();
      var g = ctx.createLinearGradient(500, 130, 500, 420);
      g.addColorStop(0, rgb(p.key, 0.35)); g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = g; ctx.fillRect(300, 130, 400, 290);
    },
    mid: function (ctx, p, n) {
      // pews, where the characters stand among them
      ctx.fillStyle = rgb(p.ink);
      for (var i = 0; i < 3; i++) ctx.fillRect(120, 280 + i * 40, 760, 14);
      ctx.fillStyle = rgb(p.shadow); ctx.fillRect(0, 400, 1000, 20);
    },
    fore: function (ctx, p, n, lift) {
      // The end of the nearest pew, close to the lens.
      ctx.fillStyle = foreInk(p, 0.92, lift);
      ctx.fillRect(-40, 340, 140, 80);
      ctx.fillRect(-40, 406, 1080, 34);
    }
  };

  /* What kind of light each place owns. */
  var LIGHT = {
    lighthouse: 'sweep',  vehicle: 'passing', street: 'passing',
    industrial: 'flicker', corridor: 'flicker', ward: 'flicker',
    field: 'cloud', water: 'cloud', woods: 'cloud',
    kitchen: 'none', room: 'none', office: 'none',
    bar: 'none', ship: 'none', chapel: 'none'
  };

  /* Light that changes during a scene rather than only between them. Tension
   * makes a bulb more agitated; it does not touch the sun. */
  function lightAt(kind, time, tension) {
    var t = tension == null ? 0.4 : tension;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    if (kind === 'sweep') {
      return { brightness: 1 + 0.35 * Math.max(0, Math.sin(time * 0.55)), offset: Math.sin(time * 0.55) };
    }
    if (kind === 'passing') {
      var phase = (time * 0.28) % 1;
      var near = Math.max(0, 1 - Math.abs(phase - 0.5) * 4);
      return { brightness: 1 + near * 0.5, offset: phase * 2 - 1 };
    }
    if (kind === 'flicker') {
      var depth = 0.05 + t * 0.35;
      var jitter = Math.sin(time * 31.7) * Math.sin(time * 7.3) * Math.sin(time * 2.1);
      return { brightness: 1 - depth * Math.max(0, jitter), offset: 0 };
    }
    if (kind === 'cloud') {
      return { brightness: 1 - 0.18 * Math.max(0, Math.sin(time * 0.08)), offset: 0 };
    }
    return { brightness: 1, offset: 0 };
  }

  var API = { SETS: SETS, PARALLAX: PARALLAX, LIGHT: LIGHT, lightAt: lightAt };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmSets = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
