# Unit tests for the pure-logic modules (math, collision, spatial grid, ECS, shake, particles).
# No GPU/window needed — fast, deterministic, and the first line of defence against logic bugs.
add_executable(maz_unit_tests unit/main.cpp)
target_link_libraries(maz_unit_tests PRIVATE maz::maz maz_warnings)
# The ZOMBOID game logic (scripts + scene builder) is shared between its app and these tests.
target_include_directories(maz_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/zomboid)
add_test(NAME unit_tests COMMAND maz_unit_tests)
set_tests_properties(unit_tests PROPERTIES TIMEOUT 30)

# Networking transport: a REAL UDP round-trip between two sockets on the loopback interface. This is
# the "networking between two machines" proof at its smallest honest scale — actual OS sockets, not a
# mock — the same code path two physical machines use, only the destination IP differs. No GPU needed.
add_executable(maz_net_tests net/loopback.cpp)
target_link_libraries(maz_net_tests PRIVATE maz::maz maz_warnings)
add_test(NAME net_loopback COMMAND maz_net_tests)
set_tests_properties(net_loopback PROPERTIES TIMEOUT 30)

# Reliability wired onto the transport: net::ReliableChannel delivers whole messages reliably and
# in order over real UDP even across a 40%-packet-loss link (retransmit + ack + reorder). The proof
# that the ack layer (AckSender/AckReceiver) + UdpSocket compose into real reliable networking.
add_executable(maz_net_reliable_tests net/reliable.cpp)
target_link_libraries(maz_net_reliable_tests PRIVATE maz::maz maz_warnings)
add_test(NAME net_reliable COMMAND maz_net_reliable_tests)
set_tests_properties(net_reliable PROPERTIES TIMEOUT 30)

# FBX geometry import: parse an ASCII .fbx mesh (Vertices + PolygonVertexIndex, ~i polygon terminator,
# fan-triangulation) into shapes::MeshData. Pure CPU — verifies a hand-authored unit cube headlessly.
add_executable(maz_fbx_tests render/fbx.cpp)
target_link_libraries(maz_fbx_tests PRIVATE maz::maz maz_warnings)
add_test(NAME fbx_import COMMAND maz_fbx_tests)
set_tests_properties(fbx_import PROPERTIES TIMEOUT 30)

# Golden-image regression: render each app on lavapipe and diff against committed references. Self-
# skips (exit 0) when software Vulkan / Xvfb / ImageMagick are unavailable, so it's harmless in a
# GPU-less CI. Requires an up-to-date build/bin.
add_test(NAME golden_images COMMAND ${CMAKE_SOURCE_DIR}/tools/golden.sh check)
set_tests_properties(golden_images PROPERTIES TIMEOUT 360)

# API-docs generator stays runnable: regenerate docs/API.md from the headers and require exit 0.
# Self-skips when python3 is unavailable so it never breaks a minimal CI.
find_program(MAZ_PYTHON3 python3)
if(MAZ_PYTHON3)
    add_test(NAME api_docs_gen COMMAND ${MAZ_PYTHON3} ${CMAKE_SOURCE_DIR}/tools/gen_api_docs.py)
    set_tests_properties(api_docs_gen PROPERTIES TIMEOUT 60)
endif()

# Smoke test: run the sandbox headless for a few frames and require a clean exit(0).
# This validates module wiring, resource lifetime, and shutdown without needing a GPU/display.
add_test(
    NAME sandbox_headless_smoke
    COMMAND sandbox --headless --frames 30)

set_tests_properties(sandbox_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME orbs_headless_smoke
    COMMAND orbs --headless --frames 30)

set_tests_properties(orbs_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME swarm_headless_smoke
    COMMAND swarm --headless --frames 30)

set_tests_properties(swarm_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME cube_headless_smoke
    COMMAND cube --headless --frames 30)

set_tests_properties(cube_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME scene3d_headless_smoke
    COMMAND scene3d --headless --frames 30)

set_tests_properties(scene3d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME world_headless_smoke
    COMMAND world --headless --frames 30)

set_tests_properties(world_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME model_headless_smoke
    COMMAND model --headless --frames 30)

set_tests_properties(model_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME village_headless_smoke
    COMMAND village --headless --frames 30)

set_tests_properties(village_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME water_headless_smoke
    COMMAND water --headless --frames 30)

set_tests_properties(water_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME instances_headless_smoke
    COMMAND instances --headless --frames 30)

set_tests_properties(instances_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME glass_headless_smoke
    COMMAND glass --headless --frames 30)

set_tests_properties(glass_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME maze_headless_smoke
    COMMAND maze --headless --frames 30)

set_tests_properties(maze_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME crowd_headless_smoke
    COMMAND crowd --headless --frames 30)

set_tests_properties(crowd_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME tween_headless_smoke
    COMMAND tween --headless --frames 30)

set_tests_properties(tween_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME menu_headless_smoke
    COMMAND menu --headless --frames 30)

set_tests_properties(menu_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME persist_headless_smoke
    COMMAND persist --headless --frames 30)

set_tests_properties(persist_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME guard_headless_smoke
    COMMAND guard --headless --frames 30)

set_tests_properties(guard_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME sprites_headless_smoke
    COMMAND sprites --headless --frames 30)

set_tests_properties(sprites_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME events_headless_smoke
    COMMAND events --headless --frames 30)

set_tests_properties(events_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME jobs_headless_smoke
    COMMAND jobs --headless --frames 30)

set_tests_properties(jobs_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME assetcache_headless_smoke
    COMMAND assetcache --headless --frames 30)

set_tests_properties(assetcache_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME skeleton_headless_smoke
    COMMAND skeleton --headless --frames 30)

set_tests_properties(skeleton_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME animclip_headless_smoke
    COMMAND animclip --headless --frames 30)

set_tests_properties(animclip_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME physics_headless_smoke
    COMMAND physics --headless --frames 30)

set_tests_properties(physics_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME animator_headless_smoke
    COMMAND animator --headless --frames 30)

set_tests_properties(animator_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME behavior_headless_smoke
    COMMAND behavior --headless --frames 30)

set_tests_properties(behavior_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME boxes_headless_smoke
    COMMAND boxes --headless --frames 30)

set_tests_properties(boxes_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME scenes_headless_smoke
    COMMAND scenes --headless --frames 30)

set_tests_properties(scenes_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME catcher_headless_smoke
    COMMAND catcher --headless --frames 30)

set_tests_properties(catcher_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME data_headless_smoke
    COMMAND data --headless --frames 30)

set_tests_properties(data_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME level_headless_smoke
    COMMAND level --headless --frames 30)

set_tests_properties(level_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME config_headless_smoke
    COMMAND config --headless --frames 30)

set_tests_properties(config_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME profiler_headless_smoke
    COMMAND profiler --headless --frames 30)

set_tests_properties(profiler_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME ecsave_headless_smoke
    COMMAND ecsave --headless --frames 30)

set_tests_properties(ecsave_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME actions_headless_smoke
    COMMAND actions --headless --frames 30)

set_tests_properties(actions_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME solar_headless_smoke
    COMMAND solar --headless --frames 30)

set_tests_properties(solar_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME camera_headless_smoke
    COMMAND camera --headless --frames 30)

set_tests_properties(camera_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME fireworks_headless_smoke
    COMMAND fireworks --headless --frames 30)

set_tests_properties(fireworks_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME scatter_headless_smoke
    COMMAND scatter --headless --frames 30)

set_tests_properties(scatter_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME noise_headless_smoke
    COMMAND noise --headless --frames 30)

set_tests_properties(noise_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME uilayout_headless_smoke
    COMMAND uilayout --headless --frames 30)

set_tests_properties(uilayout_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME navmesh_headless_smoke
    COMMAND navmesh --headless --frames 30)

set_tests_properties(navmesh_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME vectors_headless_smoke
    COMMAND vectors --headless --frames 30)

set_tests_properties(vectors_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME lights2d_headless_smoke
    COMMAND lights2d --headless --frames 30)

set_tests_properties(lights2d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME tumble_headless_smoke
    COMMAND tumble --headless --frames 30)

set_tests_properties(tumble_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME blendspace_headless_smoke
    COMMAND blendspace --headless --frames 30)

set_tests_properties(blendspace_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME joints_headless_smoke
    COMMAND joints --headless --frames 30)

set_tests_properties(joints_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME spatial2d_headless_smoke
    COMMAND spatial2d --headless --frames 30)

set_tests_properties(spatial2d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME form_headless_smoke
    COMMAND form --headless --frames 30)

set_tests_properties(form_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME cave_headless_smoke
    COMMAND cave --headless --frames 30)

set_tests_properties(cave_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME reach_headless_smoke
    COMMAND reach --headless --frames 30)

set_tests_properties(reach_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME avoid_headless_smoke
    COMMAND avoid --headless --frames 30)

set_tests_properties(avoid_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME bus_headless_smoke
    COMMAND bus --headless --frames 30)

set_tests_properties(bus_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME timeline_headless_smoke
    COMMAND timeline --headless --frames 30)

set_tests_properties(timeline_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME softshadow_headless_smoke
    COMMAND softshadow --headless --frames 30)

set_tests_properties(softshadow_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME groove_headless_smoke
    COMMAND groove --headless --frames 30)

set_tests_properties(groove_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME stylebox_headless_smoke
    COMMAND stylebox --headless --frames 30)

set_tests_properties(stylebox_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME blackboard_headless_smoke
    COMMAND blackboard --headless --frames 30)

set_tests_properties(blackboard_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME statemachine_headless_smoke
    COMMAND statemachine --headless --frames 30)

set_tests_properties(statemachine_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME reverb_headless_smoke
    COMMAND reverb --headless --frames 30)

set_tests_properties(reverb_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME tentacle_headless_smoke
    COMMAND tentacle --headless --frames 30)

set_tests_properties(tentacle_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME goap_headless_smoke
    COMMAND goap --headless --frames 30)

set_tests_properties(goap_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME theme_headless_smoke
    COMMAND theme --headless --frames 30)

set_tests_properties(theme_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME stack_headless_smoke
    COMMAND stack --headless --frames 30)

set_tests_properties(stack_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME normalmap_headless_smoke
    COMMAND normalmap --headless --frames 30)

set_tests_properties(normalmap_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME flowfield_headless_smoke
    COMMAND flowfield --headless --frames 30)

set_tests_properties(flowfield_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME sequencer_headless_smoke
    COMMAND sequencer --headless --frames 30)

set_tests_properties(sequencer_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME envelope_headless_smoke
    COMMAND envelope --headless --frames 30)

set_tests_properties(envelope_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME tree_headless_smoke
    COMMAND tree --headless --frames 30)

set_tests_properties(tree_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME area2d_headless_smoke
    COMMAND area2d --headless --frames 30)

set_tests_properties(area2d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME blendtree_headless_smoke
    COMMAND blendtree --headless --frames 30)

set_tests_properties(blendtree_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME layers_headless_smoke
    COMMAND layers --headless --frames 30)

set_tests_properties(layers_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME tileset_headless_smoke
    COMMAND tileset --headless --frames 30)

set_tests_properties(tileset_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME emitter_headless_smoke
    COMMAND emitter --headless --frames 30)

set_tests_properties(emitter_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME spatial3d_headless_smoke
    COMMAND spatial3d --headless --frames 30)

set_tests_properties(spatial3d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME containers_headless_smoke
    COMMAND containers --headless --frames 30)

set_tests_properties(containers_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME parallax_headless_smoke
    COMMAND parallax --headless --frames 30)

set_tests_properties(parallax_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME choreo_headless_smoke
    COMMAND choreo --headless --frames 30)

set_tests_properties(choreo_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME prefab_headless_smoke
    COMMAND prefab --headless --frames 30)

set_tests_properties(prefab_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME line2d_headless_smoke
    COMMAND line2d --headless --frames 30)

set_tests_properties(line2d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME restext_headless_smoke
    COMMAND restext --headless --frames 30)

set_tests_properties(restext_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME signals_headless_smoke
    COMMAND signals --headless --frames 30)

set_tests_properties(signals_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME wav_headless_smoke
    COMMAND wav --headless --frames 30)

set_tests_properties(wav_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME grid3d_headless_smoke
    COMMAND grid3d --headless --frames 30)

set_tests_properties(grid3d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME rayquery_headless_smoke
    COMMAND rayquery --headless --frames 30)

set_tests_properties(rayquery_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME strtable_headless_smoke
    COMMAND strtable --headless --frames 30)

set_tests_properties(strtable_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME locale_headless_smoke
    COMMAND locale --headless --frames 30)

set_tests_properties(locale_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME textwrap_headless_smoke
    COMMAND textwrap --headless --frames 30)

set_tests_properties(textwrap_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME addblend_headless_smoke
    COMMAND addblend --headless --frames 30)

set_tests_properties(addblend_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME primitives_headless_smoke
    COMMAND primitives --headless --frames 30)

set_tests_properties(primitives_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME slotmap_headless_smoke
    COMMAND slotmap --headless --frames 30)

set_tests_properties(slotmap_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME polycollide_headless_smoke
    COMMAND polycollide --headless --frames 30)

set_tests_properties(polycollide_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME sampler_headless_smoke
    COMMAND sampler --headless --frames 30)

set_tests_properties(sampler_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME respack_headless_smoke
    COMMAND respack --headless --frames 30)

set_tests_properties(respack_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME richtext_headless_smoke
    COMMAND richtext --headless --frames 30)

set_tests_properties(richtext_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME curve_headless_smoke
    COMMAND curve --headless --frames 30)

set_tests_properties(curve_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME multimesh_headless_smoke
    COMMAND multimesh --headless --frames 30)

set_tests_properties(multimesh_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME billboard_headless_smoke
    COMMAND billboard --headless --frames 30)

set_tests_properties(billboard_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME oneway_headless_smoke
    COMMAND oneway --headless --frames 30)

set_tests_properties(oneway_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME modfx_headless_smoke
    COMMAND modfx --headless --frames 30)

set_tests_properties(modfx_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME rootmotion_headless_smoke
    COMMAND rootmotion --headless --frames 30)

set_tests_properties(rootmotion_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME groups_headless_smoke
    COMMAND groups --headless --frames 30)

set_tests_properties(groups_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME rects_headless_smoke
    COMMAND rects --headless --frames 30)

set_tests_properties(rects_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME progress_headless_smoke
    COMMAND progress --headless --frames 30)

set_tests_properties(progress_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME interp_headless_smoke
    COMMAND interp --headless --frames 30)

set_tests_properties(interp_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME gravzones_headless_smoke
    COMMAND gravzones --headless --frames 30)

set_tests_properties(gravzones_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME ortho3d_headless_smoke
    COMMAND ortho3d --headless --frames 30)

set_tests_properties(ortho3d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME base64_headless_smoke
    COMMAND base64 --headless --frames 30)

set_tests_properties(base64_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME floatcurve_headless_smoke
    COMMAND floatcurve --headless --frames 30)

set_tests_properties(floatcurve_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME polyfill_headless_smoke
    COMMAND polyfill --headless --frames 30)

set_tests_properties(polyfill_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME deadzone_headless_smoke
    COMMAND deadzone --headless --frames 30)

set_tests_properties(deadzone_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME randomizer_headless_smoke
    COMMAND randomizer --headless --frames 30)

set_tests_properties(randomizer_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME kinematic_headless_smoke
    COMMAND kinematic --headless --frames 30)

set_tests_properties(kinematic_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME geometry_headless_smoke
    COMMAND geometry --headless --frames 30)

set_tests_properties(geometry_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME itemlist_headless_smoke
    COMMAND itemlist --headless --frames 30)

set_tests_properties(itemlist_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME gradient_headless_smoke
    COMMAND gradient --headless --frames 30)

set_tests_properties(gradient_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME camera3d_headless_smoke
    COMMAND camera3d --headless --frames 30)

set_tests_properties(camera3d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME ring_headless_smoke
    COMMAND ring --headless --frames 30)

set_tests_properties(ring_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME inifile_headless_smoke
    COMMAND inifile --headless --frames 30)

set_tests_properties(inifile_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME spectrum_headless_smoke
    COMMAND spectrum --headless --frames 30)

set_tests_properties(spectrum_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME xform2d_headless_smoke
    COMMAND xform2d --headless --frames 30)

set_tests_properties(xform2d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME popupmenu_headless_smoke
    COMMAND popupmenu --headless --frames 30)

set_tests_properties(popupmenu_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME astar_headless_smoke
    COMMAND astar --headless --frames 30)

set_tests_properties(astar_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME expr_headless_smoke
    COMMAND expr --headless --frames 30)

set_tests_properties(expr_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME forcefield_headless_smoke
    COMMAND forcefield --headless --frames 30)

set_tests_properties(forcefield_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME atlas_headless_smoke
    COMMAND atlas --headless --frames 30)

set_tests_properties(atlas_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME shapecast_headless_smoke
    COMMAND shapecast --headless --frames 30)

set_tests_properties(shapecast_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME xml_headless_smoke
    COMMAND xml --headless --frames 30)

set_tests_properties(xml_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME normals_headless_smoke
    COMMAND normals --headless --frames 30)

set_tests_properties(normals_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME filters_headless_smoke
    COMMAND filters --headless --frames 30)

set_tests_properties(filters_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME equalizer_headless_smoke
    COMMAND equalizer --headless --frames 30)

set_tests_properties(equalizer_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME busmixer_headless_smoke
    COMMAND busmixer --headless --frames 30)

set_tests_properties(busmixer_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME limiter_headless_smoke
    COMMAND limiter --headless --frames 30)

set_tests_properties(limiter_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME stereo_headless_smoke
    COMMAND stereo --headless --frames 30)

set_tests_properties(stereo_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME oscillator_headless_smoke
    COMMAND oscillator --headless --frames 30)

set_tests_properties(oscillator_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME music_headless_smoke
    COMMAND music --headless --frames 30)

set_tests_properties(music_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME pitch_headless_smoke
    COMMAND pitch --headless --frames 30)

set_tests_properties(pitch_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME distort_headless_smoke
    COMMAND distort --headless --frames 30)

set_tests_properties(distort_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME solver_headless_smoke
    COMMAND solver --headless --frames 30)

set_tests_properties(solver_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME manybody_headless_smoke
    COMMAND manybody --headless --frames 30)

set_tests_properties(manybody_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME sleep_headless_smoke
    COMMAND sleep --headless --frames 30)

set_tests_properties(sleep_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME filter_headless_smoke
    COMMAND filter --headless --frames 30)

set_tests_properties(filter_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME capsule_headless_smoke
    COMMAND capsule --headless --frames 30)

set_tests_properties(capsule_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME boundary_headless_smoke
    COMMAND boundary --headless --frames 30)

set_tests_properties(boundary_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME contacts_headless_smoke
    COMMAND contacts --headless --frames 30)

set_tests_properties(contacts_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME ccd_headless_smoke
    COMMAND ccd --headless --frames 30)

set_tests_properties(ccd_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME motor_headless_smoke
    COMMAND motor --headless --frames 30)

set_tests_properties(motor_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME convex_headless_smoke
    COMMAND convex --headless --frames 30)

set_tests_properties(convex_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME polyline_headless_smoke
    COMMAND polyline --headless --frames 30)

set_tests_properties(polyline_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME physics3d_headless_smoke
    COMMAND physics3d --headless --frames 30)

set_tests_properties(physics3d_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME pbrballs_headless_smoke
    COMMAND pbrballs --headless --frames 30)

set_tests_properties(pbrballs_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME tonemap_headless_smoke
    COMMAND tonemap --headless --frames 30)

set_tests_properties(tonemap_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME ssao_headless_smoke
    COMMAND ssao --headless --frames 30)

set_tests_properties(ssao_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME editor_headless_smoke
    COMMAND editor --headless --frames 30)

set_tests_properties(editor_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME zomboid_headless_smoke
    COMMAND zomboid --headless --frames 30)

set_tests_properties(zomboid_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME streaming_headless_smoke
    COMMAND streaming --headless --frames 30)

set_tests_properties(streaming_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME pong_headless_smoke
    COMMAND pong --headless --frames 30)

set_tests_properties(pong_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME platformer_headless_smoke
    COMMAND platformer --headless --frames 30)

set_tests_properties(platformer_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_test(
    NAME adventure_headless_smoke
    COMMAND adventure --headless --frames 30)

set_tests_properties(adventure_headless_smoke PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")












