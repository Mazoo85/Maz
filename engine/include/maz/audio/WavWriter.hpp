#pragma once

#include <string>

namespace maz::audio {

// Write `frames` of interleaved float32 samples (values expected in [-1, 1]) to a canonical 16-bit
// PCM WAV file. Samples outside the range are clamped. Returns false and sets *err (when non-null)
// on any I/O failure.
//
// `dither`: when true, add triangular-PDF (TPDF) dither of ±1 LSB before quantizing float→16-bit.
// Dithering decorrelates the quantization error from the signal, trading audible quantization
// distortion on quiet/fading tails for a benign, constant, inaudible noise floor — the standard
// mastering step when reducing to 16-bit (FL Studio's export "Dithering" option). The dither uses a
// fixed-seed PRNG so a given input renders to a byte-identical WAV every time. false (default) keeps
// the exact previous behaviour (plain rounding, no added noise).
bool writeWav16(const std::string& path, const float* interleaved, int frames, int channels,
                int sampleRate, std::string* err = nullptr, bool dither = false);

} // namespace maz::audio
