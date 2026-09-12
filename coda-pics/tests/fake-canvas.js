/*
 * A canvas 2D context that records instead of drawing.
 *
 * It is here so the whole painting pipeline — read the words, paint the scene,
 * finish it in a style — can be run in Node, with no browser and no
 * dependencies, for every subject, every setting and every style. The pixel
 * buffer is real (a Uint8ClampedArray), so the finishing passes do their
 * actual arithmetic on it rather than being skipped.
 */
'use strict';

function FakeContext(width, height) {
  this.width = width;
  this.height = height;
  this.calls = 0;
  this.ops = Object.create(null);
  this.canvas = { width: width, height: height };
  this._pixels = new Uint8ClampedArray(width * height * 4);
  for (var i = 0; i < this._pixels.length; i += 4) {
    this._pixels[i] = (i * 7) % 255;
    this._pixels[i + 1] = (i * 13) % 255;
    this._pixels[i + 2] = (i * 29) % 255;
    this._pixels[i + 3] = 255;
  }
}

var NOOPS = [
  'save', 'restore', 'beginPath', 'closePath', 'moveTo', 'lineTo',
  'quadraticCurveTo', 'bezierCurveTo', 'arc', 'arcTo', 'ellipse', 'rect',
  'fill', 'stroke', 'clip', 'fillRect', 'strokeRect', 'clearRect',
  'translate', 'rotate', 'scale', 'setTransform', 'transform', 'drawImage',
  'fillText', 'strokeText', 'setLineDash'
];

NOOPS.forEach(function (name) {
  FakeContext.prototype[name] = function () {
    this.calls++;
    this.ops[name] = (this.ops[name] || 0) + 1;
  };
});

FakeContext.prototype.createLinearGradient = function () {
  this.calls++;
  return { addColorStop: function () {} };
};
FakeContext.prototype.createRadialGradient = FakeContext.prototype.createLinearGradient;
FakeContext.prototype.createPattern = function () { return null; };
FakeContext.prototype.measureText = function (t) { return { width: String(t).length * 6 }; };

FakeContext.prototype.getImageData = function (x, y, w, h) {
  this.calls++;
  this.ops.getImageData = (this.ops.getImageData || 0) + 1;
  return { data: this._pixels, width: w, height: h };
};
FakeContext.prototype.putImageData = function (img) {
  this.calls++;
  this.ops.putImageData = (this.ops.putImageData || 0) + 1;
  if (img && img.data) this._pixels = img.data;
};

module.exports = { FakeContext: FakeContext };
