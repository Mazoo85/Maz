# Smoke test: run the sandbox headless for a few frames and require a clean exit(0).
# This validates module wiring, resource lifetime, and shutdown without needing a GPU/display.
add_test(
    NAME sandbox_headless_smoke
    COMMAND sandbox --headless --frames 30)

set_tests_properties(sandbox_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# Blender pipeline: load the test cube (glTF) and require both a clean exit and the
# expected geometry. Exercises the whole asset path with no GPU/display.
add_test(
    NAME model_load_gltf
    COMMAND sandbox --headless --frames 1
            --load-model ${CMAKE_SOURCE_DIR}/assets/models/cube.gltf)

set_tests_properties(model_load_gltf PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "24 verts, 12 tris")

# Unit tests: pure C++ (no GPU/display). Camera view/projection math.
add_executable(maz_unit_tests unit_camera.cpp)
target_link_libraries(maz_unit_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_camera COMMAND maz_unit_tests)
set_tests_properties(unit_camera PROPERTIES TIMEOUT 30)

# Unit tests: scene model + .mazscene save/load round-trip.
add_executable(maz_scene_tests unit_scene.cpp)
target_link_libraries(maz_scene_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_scene COMMAND maz_scene_tests)
set_tests_properties(unit_scene PROPERTIES TIMEOUT 30)

# Unit tests: audio DSP. Oscillator waveform/frequency/envelope + offline engine render. No device.
add_executable(maz_audio_tests unit_audio.cpp)
target_link_libraries(maz_audio_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_audio COMMAND maz_audio_tests)
set_tests_properties(unit_audio PROPERTIES TIMEOUT 30)

# Unit tests: step sequencer + drum voices. Pattern grid, transport timing, sound-vs-silence.
add_executable(maz_sequencer_tests unit_sequencer.cpp)
target_link_libraries(maz_sequencer_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_sequencer COMMAND maz_sequencer_tests)
set_tests_properties(unit_sequencer PROPERTIES TIMEOUT 30)

# Unit tests: A2 melodic path — pitch math, poly synth (ADSR), piano roll, melodic scheduling.
add_executable(maz_synth_tests unit_synth.cpp)
target_link_libraries(maz_synth_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_synth COMMAND maz_synth_tests)
set_tests_properties(unit_synth PROPERTIES TIMEOUT 30)

# Unit tests: A3 mixer + effects — delay tap, low-pass, compressor, reverb tail, master gain.
add_executable(maz_effects_tests unit_effects.cpp)
target_link_libraries(maz_effects_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_effects COMMAND maz_effects_tests)
set_tests_properties(unit_effects PROPERTIES TIMEOUT 30)

# Unit tests: A4 project save/load — full .cjc round-trip (grid, notes, bus gains, mixer, effects).
add_executable(maz_project_tests unit_project.cpp)
target_link_libraries(maz_project_tests PRIVATE maz::maz maz_warnings)

add_test(NAME unit_project COMMAND maz_project_tests)
set_tests_properties(unit_project PROPERTIES TIMEOUT 30)

# DAW: render one second of a 440 Hz tone offline to a WAV and require a clean exit + the stats log.
# Exercises the whole audio path (Oscillator -> AudioEngine -> WavWriter) with no audio device.
add_test(
    NAME daw_headless_smoke
    COMMAND daw --headless --seconds 1 --freq 440 --wav ${CMAKE_BINARY_DIR}/tone.wav)
set_tests_properties(daw_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "audio: rendered 48000")

# DAW: render two seconds of the demo drum pattern offline and require a clean exit + stats. Proves
# the sequencer (DrumVoice -> Sequencer -> AudioEngine) makes sound headless; a silent render exits 1.
add_test(
    NAME daw_beat_smoke
    COMMAND daw --headless --beat --seconds 2 --bpm 120 --wav ${CMAKE_BINARY_DIR}/beat.wav)
set_tests_properties(daw_beat_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "audio: rendered 96000 frames.*beat @120 BPM")

# DAW: render two seconds of the demo piano-roll melody offline (synth -> Sequencer -> AudioEngine).
add_test(
    NAME daw_melody_smoke
    COMMAND daw --headless --melody --seconds 2 --bpm 120 --wav ${CMAKE_BINARY_DIR}/melody.wav)
set_tests_properties(daw_melody_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "audio: rendered 96000 frames.*melody @120 BPM")

# DAW: save the demo project to a .cjc file (fixture), then load it back and render it. The load
# test requires the save test to have run first (FIXTURES), proving end-to-end project persistence.
add_test(
    NAME daw_project_save
    COMMAND daw --headless --save ${CMAKE_BINARY_DIR}/demo.cjc --seconds 1)
set_tests_properties(daw_project_save PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "project: saved"
    FIXTURES_SETUP cjc_project)

add_test(
    NAME daw_project_load
    COMMAND daw --headless --load ${CMAKE_BINARY_DIR}/demo.cjc --seconds 1)
set_tests_properties(daw_project_load PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "audio: rendered 48000 frames.*project @"
    FIXTURES_REQUIRED cjc_project)

# Load a scene headless and require a clean exit — exercises scene parsing + the render wiring
# end to end (draws no-op with no GPU).
add_test(
    NAME scene_load_headless
    COMMAND sandbox --headless --frames 1 --scene ${CMAKE_SOURCE_DIR}/assets/scenes/demo.mazscene)
set_tests_properties(scene_load_headless PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "loaded scene")

# Editor: start headless, load a scene, run a few frames, exit cleanly. Verifies the editor app +
# ImGui integration link and boot (the UI itself needs a GPU and is not exercised here).
add_test(
    NAME editor_headless_smoke
    COMMAND editor --headless --frames 3 --scene ${CMAKE_SOURCE_DIR}/assets/scenes/demo.mazscene)
set_tests_properties(editor_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "Maz Editor shutting down")

# Render probe: actually rasterize the cube off-screen (via a software Vulkan driver, lavapipe, in
# CI) and verify the pixels — proof the mesh renderer draws, not just compiles. Requires a Vulkan
# ICD; fails loudly if none is present so a missing driver is visible rather than silently skipped.
add_test(
    NAME render_probe
    COMMAND render_probe ${CMAKE_SOURCE_DIR}/assets/models/cube.gltf)
set_tests_properties(render_probe PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "render probe: PASS")
