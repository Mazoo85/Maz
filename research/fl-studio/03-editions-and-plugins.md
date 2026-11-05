# 3. Editions, business model and the built-in plugins

> Part of the [FL Studio research pack](README.md). Researched 5 November 2026, for FL Studio
> 2026.1.7b.
>
> **Confidence tags.**
> - **[C]** confirmed by an excerpt of an official Image-Line page.
> - **[S]** secondary sources only (retailers, blogs).
> - **[U]** unconfirmed, or the sources conflict. Check it against
>   <https://www.image-line.com/fl-studio/compare-editions> before relying on it.
>
> Which plugin is in which edition is the least certain part of this whole research pack.

---

## 3.1 How Image-Line sells FL Studio

- **One installer, many editions.** Every edition uses the same program. Your Image-Line account's
  licence decides what is unlocked (*Help > Unlock FL Studio > Sign in*) [C]. There is no dongle
  or iLok.
- **Lifetime Free Updates.** You pay once and every future version of your edition is free,
  including the yearly "2024 / 2025 / 2026" releases [C]. This is FL Studio's most famous selling
  point.
- **The free trial has no time or feature limits.** You can do everything, including save and
  export, but you **cannot reopen a saved project** until you buy [C]. After buying, those
  projects open normally. This is a clever conversion hook.
- **Upgrades** between editions cost the price difference [S].
- **"Included" plugins are native versions.** Using the same plugin in *another* DAW (as
  VST/AU) can need a separate plugin licence [C].
- **Editions nest.** Each higher edition contains everything below it [C].

### Edition comparison (desktop: Windows and macOS)

| | **Fruity** | **Producer** | **Signature** | **All Plugins (APE)** |
|---|---|---|---|---|
| Price, widely quoted | $99 | $199 | $299 | $499 |
| Price reported mid-2026 [S] | $99 | $179 | $269 | $449 |
| Lifetime free updates | ✓ | ✓ | ✓ | ✓ |
| Patterns, Piano Roll, Mixer, plugin hosting | ✓ | ✓ | ✓ | ✓ |
| **Audio clips in the Playlist** | **max 8** [C] | unlimited | unlimited | unlimited |
| **Audio recording** | **✗** [C] | ✓ | ✓ | ✓ |
| Edison, Slicex, Frequency Shifter | ✗ | ✓ [C] | ✓ | ✓ |
| Sytrus, Maximus, Vocodex, Newtime | ✗ | ✓ [S] | ✓ | ✓ |
| Gross Beat, NewTone, Pitcher, Harmless, Hardcore | ✗ | ✗ | ✓ [S] | ✓ |
| Harmor, Toxic Biohazard, Poizone, Morphine, Ogun, Sawer, Drumaxx | ✗ | ✗ | ✗ | ✓ [S/U] |
| Kepler Exo, Emphasis, LuxeVerb, Pitch Shifter, **Transmitter** | ✗ | ✗ | ✗ | ✓ [C/S] |

There are frequent sales: a summer sale, a half-price Fruity week in October 2026, and Black
Friday.

**APE and future plugins.** Image-Line's own APE terms say future plugins are *not* guaranteed,
though in practice new ones (Kepler Exo, Transmitter) have been added [C].

### Rent-to-Own Plus (September 2026)
- Pay monthly. After **12 payments** you own a normal perpetual licence with lifetime updates [C].
- Available for Producer, Signature and APE (not Fruity). It starts at **$20 a month** for
  Producer plus FL Cloud Plus [C].
- **FL Cloud Plus** is included from day one, then continues as an ordinary cancellable
  subscription.

### FL Cloud (subscription; does *not* include FL Studio itself)

| Tier | Price | Plugins (rented) | Sounds | AI mastering | Distribution | Cloud storage |
|---|---|---|---|---|---|---|
| Free | $0 | about 10 | small set | 1 preset | ✗ [U] | 500 MB |
| Plus | about $9.99 a month / $99 a year [S] | 18 | full library | unlimited, 15+ presets | ✓ | 5 GB |
| Pro | about $14.99 a month / $149 a year [S] | 65+ or 90+ (sources conflict) | full library | unlimited | ✓ | 1 TB |

- The sample library is described as 1M+ or 2M+ sounds.
- FL Studio 2026 shipped with "the biggest FL Cloud update yet": project backup, a bigger loop
  library, and a status badge in Loop Starter [C].

### Related products
- **FL Studio Mobile** (iOS and Android): $14.99 one-time, plus in-app sound packs. It also runs
  as a plugin inside desktop FL Studio. The 2025 "Mobile Rack" brought its instruments and
  effects to the desktop.
- **Individual plugins** (Harmor, Gross Beat and others) are also sold separately, as VST/AU
  plugins for other DAWs [C].
- **FL Studio Remote:** a phone app for controlling FL Studio (mentioned in 2026.1.5 fixes).

---

## 3.2 Built-in plugin catalogue

**Edition key:**
- **F** = Fruity and up
- **P** = Producer and up
- **S** = Signature and up
- **APE** = All Plugins Edition only

### Synthesizers

| Plugin | What it is | Synthesis type | Edition |
|---|---|---|---|
| **FLEX** | Preset-pack player with 8 macros; rebuilt in 2026 | Sample and synth "rompler" | F (free) [C] |
| 3x Osc | Classic three-oscillator synth | Subtractive | F |
| Fruity DX10 | DX-style FM synth | FM | F |
| SimSynth, MiniSynth | Simple analog-style synths | Subtractive | F |
| BooBass | Bass-guitar instrument | Sample/modelled | F |
| Plucked! | Plucked strings | Physical modelling (Karplus-Strong) | F |
| Groove Machine Synth | Groovebox with sequencer | Hybrid | F [S] |
| Transistor Bass | 303-style acid bass with step sequencer | Subtractive | F [U] |
| Kepler | Two-oscillator analog-style synth (Juno-6 inspired) | Subtractive | P [U] |
| Kepler Exo | Expanded Kepler (Juno/Jupiter inspired) | Subtractive | APE [C] |
| **Sytrus** | 6-operator FM/RM synth with filters and a modulation matrix | FM + subtractive + additive | P [S] |
| Harmless | Simplified Harmor | Additive (subtractive-style) | S [S] |
| **Harmor** | Flagship; resynthesises audio and images | Additive/resynthesis | APE [S/U] |
| Sakura | Bowed/struck strings | Physical modelling | S or APE [U] |
| Toxic Biohazard | Hybrid synth | FM + subtractive | APE [U] |
| Poizone | Easy subtractive synth | Subtractive | APE [U] |
| Morphine | Morphs between four additive sounds | Additive | APE [U] |
| Ogun | Metallic, glassy tones | Additive | APE [U] |
| Sawer | Vintage supersaw | Subtractive | APE [U] |
| Autogun | Browses procedurally generated Ogun patches | Additive | F [U] |
| Drumaxx | Drum synth | Modelled percussion | APE [U] |
| Fruit Kick | Kick-drum synth | Sine sweep | F |
| Fruity Granulizer | Granular sampler-synth | Granular | F |
| BeepMap | Turns images into sound | Image to additive | F [U] |

### Samplers and drums

| Plugin | What it is | Edition |
|---|---|---|
| Channel Sampler | The default sample channel | F |
| **FPC** | MPC-style 16-pad drum sampler with layers | F |
| Fruity Slicer / **Fruity Slicer 2** (2025.2) | Chops loops into slices played from the Piano Roll | F |
| **Slicex** | Advanced slicer with per-slice editing | P [C] |
| DirectWave | Multisample sampler and editor | F/P [U] |
| Fruity Soundfont Player | Plays .sf2 SoundFonts | F |
| FL Keys | Piano and keys | F |
| Wave Traveller | Scratch/vinyl sample player | F |

### Note generators, MIDI and visual tools

| Plugin | What it is | Edition |
|---|---|---|
| **Patcher** | Modular container that wires plugins together and saves them as one preset | F |
| MIDI Out | Sends notes and CC to external gear | F |
| Dashboard | Custom control panels for external hardware | F |
| VFX Sequencer, Arpeggiator, Keyboard Splitter, Key Mapper, Level Scaler, Envelope, Color Mapper | "Visual FX" note processors | P [U] |
| Note Arpeggiator (2026) | Turns held notes into rhythmic patterns | [U] |
| ZGameEditor Visualizer | Real-time visuals and music-video rendering | F [U] |

### Dynamics

| Plugin | What it is | Edition |
|---|---|---|
| Fruity Limiter | Limiter, compressor and gate in one | F |
| Fruity Compressor, Multiband Compressor | Compressors | F |
| **Maximus** | 3-band mastering maximiser | P [S] |
| Soundgoodizer | One-knob "make it better" (Maximus presets) | F |
| Fruity Soft Clipper | Soft clipper | F |
| Emphasis | Multi-stage mastering limiter (2025) | APE [C] |
| Emphasizer | Simplified Emphasis (2025.2) | reportedly all editions [U] |
| **Transmitter** (2026) | Splits audio into transient and sustain streams on separate mixer tracks | APE [C] |
| Fruity Squeeze | Bit-crush and squash | F |

### EQ and filters

| Plugin | What it is | Edition |
|---|---|---|
| **Fruity Parametric EQ 2** | 7-band EQ with spectrum display | F |
| Fruity 7 Band EQ | Graphic EQ | F |
| Fruity Filter, Free Filter | Simple and multimode filters | F |
| Fruity Love Philter | Multi-stage filter and modulation workstation | F [U] |
| Low Lifter (2024) | Low-end enhancer | [U] |

### Modulation and stereo

| Plugin | What it is | Edition |
|---|---|---|
| Fruity Chorus, Flanger, Flangus, Phaser | Classic modulation effects | F |
| Vintage Chorus, Vintage Phaser, Hyper Chorus | Analog emulations | P/S [U] |
| Frequency Shifter | Frequency shifter and ring modulator | P [C] |
| Fruity Stereo Enhancer, Stereo Shaper, Spreader (2024) | Stereo width | F / [U] |
| Effector | 12-effect performance unit | P [U] |

### Delay and reverb

| Plugin | What it is | Edition |
|---|---|---|
| Fruity Delay 3 | Analog/tape-style delay | F |
| Fruity Delay 2, Delay Bank | Older delay; multi-tap delay | F / P [U] |
| Multiband Delay | Separate delay per frequency band | [U] |
| Fruity Reeverb 2 | Algorithmic reverb | F |
| Fruity Convolver | Convolution (impulse-response) reverb | P [U] |
| **LuxeVerb** | High-end algorithmic reverb | APE [C] |

### Distortion and character

| Plugin | What it is | Edition |
|---|---|---|
| Fruity Blood Overdrive, Fast Dist, WaveShaper | Distortion and shaping | F |
| Distructor | Distortion multi-effect rack | P [U] |
| Hardcore | Guitar amp and pedal suite | S [S] |

### Pitch, time and creative

| Plugin | What it is | Edition |
|---|---|---|
| **Gross Beat** | Time and volume mangling: stutter, gate, scratch, tape-stop | S [C/S] |
| **NewTone** | Note-by-note pitch editing (Melodyne-like) | S [S] |
| **Pitcher** | Real-time auto-tune and harmoniser | S [S] |
| Pitch Shifter | Pitch-shift and harmony | APE [S] |
| Newtime | Audio time-stretch and warp editor | P [S] |
| Vocodex | Vocoder (up to 1024 bands) | P [S] |
| Fruity Vocoder | Simpler vocoder | F [U] |
| Fruity Scratcher | Turntable scratch | F |

### Analysis and editing

| Plugin | What it is | Edition |
|---|---|---|
| **Edison** | Audio editor, recorder and analyser (spectral view, denoise, declip, stems) | P [C] |
| Wave Candy | Oscilloscope, spectrum and vectorscope | F |
| Fruity Spectroman, dB Meter, Tuner, Big Clock, NoteBook 2 | Meters and utilities | F |

### Control and routing (modulation sources)

| Plugin | What it is | Edition |
|---|---|---|
| Control Surface | Custom on-screen control panels (XY pads, knobs, buttons) | F |
| Fruity Formula Controller | Control signal from a maths formula | F |
| **Fruity Peak Controller** | Audio level becomes a control signal (sidechain-style modulation) | F |
| Fruity Envelope Controller, LFO, Keyboard Controller, X-Y / X-Y-Z Controller | Modulation sources | F |
| Fruity Balance, Center, Mute 2, Phase Inverter, Send | Mixer utilities | F |
| Fruity Wrapper | Hosts third-party VST2/VST3/AU/CLAP plugins | F (core) |

---

## 3.3 Takeaways for our own program

1. **The main paywall is audio.** Fruity is a complete MIDI/pattern studio. Paying unlocks
   recording and unlimited audio clips. This is a simple, understandable tier line.
2. **Higher tiers sell "signature" sounds and tools**, not workflow features. The core workflow is
   identical in every edition.
3. **Perpetual licence plus free updates builds huge loyalty.** Image-Line now adds recurring
   income through FL Cloud (sounds, mastering, distribution, rented plugins, storage) rather than
   charging for upgrades.
4. **Breadth is mostly small, focused plugins.** A clone needs a solid core set (synth, sampler,
   drum pads, slicer, EQ, compressor, limiter, delay, reverb, distortion, chorus, analyser,
   controllers) more than dozens of flagship instruments.

## Sources

**Image-Line (seen as search excerpts)**
- [Compare editions](https://www.image-line.com/fl-studio/compare-editions)
- [Feature/edition table in the manual](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/app_feature.htm)
- [Registration and unlocking](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/basics_registration.htm)
- [2026 release](https://www.image-line.com/fl-studio/release/2026)
- [All-new FLEX](https://www.image-line.com/news/all-new-flex)
- [FL Cloud 2026](https://www.image-line.com/news/whats-new-in-fl-cloud26)
- [Rent-to-Own Plus](https://www.image-line.com/news/announcing-rent-to-own-plus)
- FL Cloud: [overview](https://www.image-line.com/fl-cloud), [FAQ](https://image-line.com/fl-cloud/faq), [plugins](https://www.image-line.com/fl-cloud/plugins), [mastering](https://www.image-line.com/fl-cloud/mastering)
- [Shop](https://support.image-line.com/jshop/shop.php)
- [APE terms](https://support.image-line.com/action/knowledgebase/?ans=603)
- [Audio in Fruity](https://support.image-line.com/knowledgebase/base.php?ans=502)
- [Kepler Exo](https://www.image-line.com/fl-studio-news/fl-studio-kepler-exo)
- [LuxeVerb](https://www.image-line.com/fl-studio-news/fl-studio-luxeverb)

**Secondary**
- [Audeobox pricing guide](https://www.audeobox.com/learn/fl-studio/fl-studio-pricing-which-edition/)
- [Violet Recording](https://violetrecording.com/how-much-does-fl-studio-cost/)
- [ThePricer](https://www.thepricer.org/how-much-does-fl-studio-cost/)
- [flstudiopro stock plugins](https://flstudiopro.com/fl-studio-stock-plugins/)
- [EDMTunes](https://www.edmtunes.com/2026/07/fl-studio-2026-is-out-now-free-for-all-existing-users/)
- [FL Studio Mobile on the App Store](https://apps.apple.com/app/id432850619)
- [Packt: What is FL Studio](https://subscription.packtpub.com/book/business-and-other/9781837631650/2/ch02lvl1sec07/what-is-fl-studio)
