#pragma once

#include <string>

namespace maz::audio {

class Sequencer;
class Mixer;
class Automation;
class SynthInstrument;

// Save/load a CJC Music Station project to a simple line-based text file (".cjc"). A project
// captures the full musical + mixing state: tempo, the drum step grid, the piano-roll notes, the
// drum/synth bus gains, the synth engine, every mixer effect's parameters, and the automation
// lanes. Round-trips exactly (see tests/unit_project.cpp). Returns false and sets *err (when
// non-null) on any I/O or parse failure.
bool saveProject(const std::string& path, Sequencer& seq, Mixer& mixer, Automation& automation,
                 std::string* err = nullptr);
bool loadProject(const std::string& path, Sequencer& seq, Mixer& mixer, Automation& automation,
                 std::string* err = nullptr);

// In-memory variants of the same serialization: serialize the whole project to a string, and restore
// it from one. Identical format to the .cjc file (the file savers are thin wrappers over these), so a
// string snapshot round-trips exactly. These enable undo/redo (snapshot then restore), project
// templates, and copy/paste without touching the disk.
std::string saveProjectToString(Sequencer& seq, Mixer& mixer, Automation& automation);
bool loadProjectFromString(const std::string& text, Sequencer& seq, Mixer& mixer,
                           Automation& automation, std::string* err = nullptr);

// Save/load a single synth engine as a standalone instrument preset (".cjcpatch"), the FL "channel
// preset" workflow: dial in a patch on one channel, save it, and recall it onto any synth channel or
// in another project. The preset stores exactly the two synth lines that appear in a full .cjc file,
// written and parsed by the same code, so it round-trips precisely and stays back-compatible as new
// synth fields are appended. Returns false and sets *err (when non-null) on any I/O or parse failure.
bool saveSynthPreset(const std::string& path, const SynthInstrument& s, std::string* err = nullptr);
bool loadSynthPreset(const std::string& path, SynthInstrument& s, std::string* err = nullptr);

} // namespace maz::audio
