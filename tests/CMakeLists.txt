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

# Netpbm (PNM) image codec: decode P1..P6 + encode P6/P3, checked against hand-authored byte streams
# (independent of the implementation) plus an encode->decode round-trip. Pure CPU, headless.
add_executable(maz_pnm_tests render/pnm.cpp)
target_link_libraries(maz_pnm_tests PRIVATE maz::maz maz_warnings)
add_test(NAME pnm_codec COMMAND maz_pnm_tests)
set_tests_properties(pnm_codec PROPERTIES TIMEOUT 30)

# GPU-driven particle simulation + plane collision (fx::GpuParticleSystem). The CPU step is the exact
# arithmetic the compute shader runs, so bounce energy, no-sink, friction, lifecycle and the instance
# buffer are all verified headlessly here; the GPU dispatch + instanced draw are owner-verified.
add_executable(maz_gpu_particles_tests fx/gpu_particles.cpp)
target_link_libraries(maz_gpu_particles_tests PRIVATE maz::maz maz_warnings)
add_test(NAME gpu_particles COMMAND maz_gpu_particles_tests)
set_tests_properties(gpu_particles PROPERTIES TIMEOUT 30)

# Screen-space reflections (render::ssrTrace): reflection ray-march projected against a hand-built depth
# buffer — wall-hit UV/depth, sky miss, off-screen miss, thickness gate, reflect/project. The trace math is
# exactly what the SSR shader runs; producing the reflected image is the owner-verified GPU pass.
add_executable(maz_ssr_tests render/ssr.cpp)
target_link_libraries(maz_ssr_tests PRIVATE maz::maz maz_warnings)
add_test(NAME ssr_trace COMMAND maz_ssr_tests)
set_tests_properties(ssr_trace PROPERTIES TIMEOUT 30)

# One-bounce global-illumination gather (render::gatherIrradiance / bakeIndirect): cosine-weighted
# hemisphere path-trace of indirect + sky irradiance. Verified headlessly — open sky returns exactly the sky
# color, facing a bright patch beats facing away, a dark ceiling darkens the gather, energy stays bounded.
add_executable(maz_gi_tests render/gi.cpp)
target_link_libraries(maz_gi_tests PRIVATE maz::maz maz_warnings)
add_test(NAME gi_gather COMMAND maz_gi_tests)
set_tests_properties(gi_gather PROPERTIES TIMEOUT 30)

# Dynamic extension loader (maz::ext::DynamicLibrary / loadExtensionLibrary). End-to-end, NOT a mock: build
# the sample plugin as a real shared library (a MODULE), then the test dlopen/LoadLibrary's it at runtime and
# calls a class it registered across the C ABI. The plugin path is injected as MAZ_COUNTER_PLUGIN_PATH.
add_library(maz_counter_plugin MODULE ${CMAKE_SOURCE_DIR}/examples/plugins/counter_plugin.cpp)
target_link_libraries(maz_counter_plugin PRIVATE maz::maz)
set_target_properties(maz_counter_plugin PROPERTIES PREFIX "" OUTPUT_NAME "counter_plugin")

add_executable(maz_ext_dynamic_tests ext/dynamic.cpp)
target_link_libraries(maz_ext_dynamic_tests PRIVATE maz::maz maz_warnings ${CMAKE_DL_LIBS})
target_compile_definitions(maz_ext_dynamic_tests PRIVATE
    MAZ_COUNTER_PLUGIN_PATH="$<TARGET_FILE:maz_counter_plugin>")
add_dependencies(maz_ext_dynamic_tests maz_counter_plugin)
add_test(NAME ext_dynamic COMMAND maz_ext_dynamic_tests)
set_tests_properties(ext_dynamic PROPERTIES TIMEOUT 30)

# Static docs-site generator (maz::docs): Markdown subset -> linked HTML site. Verified headlessly on
# in-memory strings — heading/list/code/bold/inline-code rendering, HTML escaping, .md->.html link rewrite,
# and index + per-page + shared-nav site assembly.
add_executable(maz_docs_tests docs/site.cpp)
target_link_libraries(maz_docs_tests PRIVATE maz::maz maz_warnings)
add_test(NAME docs_site COMMAND maz_docs_tests)
set_tests_properties(docs_site PROPERTIES TIMEOUT 30)

# Web/native main-loop driver (platform::runMainLoop / runMainLoopBounded). The native path — the loop that
# runs, stops on request, and honors the iteration cap — is verified here; the __EMSCRIPTEN__ browser-callback
# path compiles only under the Emscripten toolchain (see docs/WEB_BUILD.md).
add_executable(maz_webloop_tests platform/webloop.cpp)
target_link_libraries(maz_webloop_tests PRIVATE maz::maz maz_warnings)
add_test(NAME webloop COMMAND maz_webloop_tests)
set_tests_properties(webloop PROPERTIES TIMEOUT 30)

# Per-platform backend seam (platform::PlatformBackend + HeadlessBackend + PlatformRegistry). The headless
# backend + registry are pure CPU: capabilities, init/shutdown lifecycle, directory resolution, suspend/
# resume transitions, and backend-by-id selection are verified here (console/VR/mobile backends are the
# documented human step, see docs/PLATFORMS.md).
add_executable(maz_platform_backend_tests platform/backend.cpp)
target_link_libraries(maz_platform_backend_tests PRIVATE maz::maz maz_warnings)
add_test(NAME platform_backend COMMAND maz_platform_backend_tests)
set_tests_properties(platform_backend PROPERTIES TIMEOUT 30)

# QOA (Quite OK Audio) codec (audio::encodeQoa / decodeQoa) — a full spec-accurate encoder + decoder,
# verified headlessly by an encode->decode round-trip (mono + stereo) that checks header/metadata/sample
# count, bounded lossy error, and that the stream is smaller than 16-bit PCM. No external reference file.
add_executable(maz_qoa_tests audio/qoa.cpp)
target_link_libraries(maz_qoa_tests PRIVATE maz::maz maz_warnings)
add_test(NAME qoa_codec COMMAND maz_qoa_tests)
set_tests_properties(qoa_codec PROPERTIES TIMEOUT 30)

# G.711 mu-law / A-law companding codec (audio::encodeMuLaw / decodeMuLaw / encodeALaw / decodeALaw) — the
# ITU-T telephony/VoIP audio format (a common .wav payload). Verified against spec anchors (mu-law silence ->
# 0xFF), sign + monotonicity, and a 2:1 encode->decode round-trip with bounded log-quantization error.
add_executable(maz_g711_tests audio/g711.cpp)
target_link_libraries(maz_g711_tests PRIVATE maz::maz maz_warnings)
add_test(NAME g711_codec COMMAND maz_g711_tests)
set_tests_properties(g711_codec PROPERTIES TIMEOUT 30)

# WebSocket (RFC 6455) handshake + frame codec (net::wsAcceptKey / wsEncodeFrame / wsDecodeFrame) — the
# transport a browser/WASM build must use (raw UDP is unavailable in a browser). Verified against RFC 6455's
# own golden vectors: the canonical accept key and the canonical masked/unmasked "Hello" frames, plus a
# round-trip and the partial-buffer "need more" case. No live socket.
add_executable(maz_websocket_tests net/websocket.cpp)
target_link_libraries(maz_websocket_tests PRIVATE maz::maz maz_warnings)
add_test(NAME websocket COMMAND maz_websocket_tests)
set_tests_properties(websocket PROPERTIES TIMEOUT 30)

# Reflection-probe / cubemap CAPTURE matrices (render::cubeFaceView / cubeFaceProjection). Verified for
# self-consistency with the Cubemap.hpp sampler: each face's look axis maps back to that face and projects
# to the face center, the projection is a true 90-degree FOV, and the six frustums tile the sphere. Pure
# matrix math; the six GPU render passes are owner-verified.
add_executable(maz_cubemap_capture_tests render/cubemap_capture.cpp)
target_link_libraries(maz_cubemap_capture_tests PRIVATE maz::maz maz_warnings)
add_test(NAME cubemap_capture COMMAND maz_cubemap_capture_tests)
set_tests_properties(cubemap_capture PROPERTIES TIMEOUT 30)

# GLB (binary glTF) container parser (render::parseGlb / buildGlb) — split a .glb byte buffer into its JSON
# and BIN chunks without a file device or cgltf. Verified against hand-built buffers: build->parse
# round-trip, JSON-only files, header magic/version/length validation, spec chunk padding, unknown-chunk skip.
add_executable(maz_glb_tests render/glb.cpp)
target_link_libraries(maz_glb_tests PRIVATE maz::maz maz_warnings)
add_test(NAME glb_container COMMAND maz_glb_tests)
set_tests_properties(glb_container PROPERTIES TIMEOUT 30)

# Multi-bounce global illumination / progressive radiosity (render::bakeRadiosity) on the tested hemisphere
# gather: verifies the signature of a correct solve — total light grows across bounces but the increments
# shrink (converges), zero albedo means no indirect, and energy stays within the geometric-series bound.
add_executable(maz_radiosity_tests render/radiosity.cpp)
target_link_libraries(maz_radiosity_tests PRIVATE maz::maz maz_warnings)
add_test(NAME radiosity COMMAND maz_radiosity_tests)
set_tests_properties(radiosity PROPERTIES TIMEOUT 60)

# Vertex-cache optimization (render::optimizeVertexCache, Forsyth's algorithm): reorders triangle indices
# for GPU post-transform cache locality. Verified headlessly — the reordering is a lossless permutation of
# the exact same triangle set, and the simulated-cache ACMR drops versus the naive row-major grid order.
add_executable(maz_vertexcache_tests render/vertexcache.cpp)
target_link_libraries(maz_vertexcache_tests PRIVATE maz::maz maz_warnings)
add_test(NAME vertex_cache COMMAND maz_vertexcache_tests)
set_tests_properties(vertex_cache PROPERTIES TIMEOUT 30)

# Mesh simplification by vertex clustering (render::simplifyClustering) — grid-based decimation for LODs and
# collision hulls. Verified headlessly: output has fewer verts/tris, every triangle is non-degenerate and
# in range, the bbox is preserved within one cell, coarser cells reduce more, and a sub-spacing cell no-ops.
add_executable(maz_meshsimplify_tests render/meshsimplify.cpp)
target_link_libraries(maz_meshsimplify_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_simplify COMMAND maz_meshsimplify_tests)
set_tests_properties(mesh_simplify PROPERTIES TIMEOUT 30)

add_executable(maz_gif_tests render/gif.cpp)
target_link_libraries(maz_gif_tests PRIVATE maz::maz maz_warnings)
add_test(NAME gif_codec COMMAND maz_gif_tests)
set_tests_properties(gif_codec PROPERTIES TIMEOUT 30)

add_executable(maz_meshsimplifyquadric_tests render/meshsimplifyquadric.cpp)
target_link_libraries(maz_meshsimplifyquadric_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_simplify_quadric COMMAND maz_meshsimplifyquadric_tests)
set_tests_properties(mesh_simplify_quadric PROPERTIES TIMEOUT 30)

add_executable(maz_overdraw_tests render/overdraw.cpp)
target_link_libraries(maz_overdraw_tests PRIVATE maz::maz maz_warnings)
add_test(NAME overdraw_optimize COMMAND maz_overdraw_tests)
set_tests_properties(overdraw_optimize PROPERTIES TIMEOUT 30)

add_executable(maz_meshtopology_tests render/meshtopology.cpp)
target_link_libraries(maz_meshtopology_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_topology COMMAND maz_meshtopology_tests)
set_tests_properties(mesh_topology PROPERTIES TIMEOUT 30)

add_executable(maz_meshcomponents_tests render/meshcomponents.cpp)
target_link_libraries(maz_meshcomponents_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_components COMMAND maz_meshcomponents_tests)
set_tests_properties(mesh_components PROPERTIES TIMEOUT 30)

add_executable(maz_meshquantize_tests render/meshquantize.cpp)
target_link_libraries(maz_meshquantize_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_quantize COMMAND maz_meshquantize_tests)
set_tests_properties(mesh_quantize PROPERTIES TIMEOUT 30)

add_executable(maz_meshplanarregions_tests render/meshplanarregions.cpp)
target_link_libraries(maz_meshplanarregions_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_planar_regions COMMAND maz_meshplanarregions_tests)
set_tests_properties(mesh_planar_regions PROPERTIES TIMEOUT 30)

add_executable(maz_meshmassproperties_tests render/meshmassproperties.cpp)
target_link_libraries(maz_meshmassproperties_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_mass_properties COMMAND maz_meshmassproperties_tests)
set_tests_properties(mesh_mass_properties PROPERTIES TIMEOUT 30)

add_executable(maz_meshao_tests render/meshao.cpp)
target_link_libraries(maz_meshao_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_ambient_occlusion COMMAND maz_meshao_tests)
set_tests_properties(mesh_ambient_occlusion PROPERTIES TIMEOUT 30)

add_executable(maz_meshsdf_tests render/meshsdf.cpp)
target_link_libraries(maz_meshsdf_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_sdf COMMAND maz_meshsdf_tests)
set_tests_properties(mesh_sdf PROPERTIES TIMEOUT 30)

add_executable(maz_meshhardedges_tests render/meshhardedges.cpp)
target_link_libraries(maz_meshhardedges_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_hard_edges COMMAND maz_meshhardedges_tests)
set_tests_properties(mesh_hard_edges PROPERTIES TIMEOUT 30)

add_executable(maz_uvdensity_tests render/uvdensity.cpp)
target_link_libraries(maz_uvdensity_tests PRIVATE maz::maz maz_warnings)
add_test(NAME uv_density COMMAND maz_uvdensity_tests)
set_tests_properties(uv_density PROPERTIES TIMEOUT 30)

add_executable(maz_trianglequality_tests render/trianglequality.cpp)
target_link_libraries(maz_trianglequality_tests PRIVATE maz::maz maz_warnings)
add_test(NAME triangle_quality COMMAND maz_trianglequality_tests)
set_tests_properties(triangle_quality PROPERTIES TIMEOUT 30)

add_executable(maz_meshboundaryloops_tests render/meshboundaryloops.cpp)
target_link_libraries(maz_meshboundaryloops_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_boundary_loops COMMAND maz_meshboundaryloops_tests)
set_tests_properties(mesh_boundary_loops PROPERTIES TIMEOUT 30)

add_executable(maz_meshcurvature_tests render/meshcurvature.cpp)
target_link_libraries(maz_meshcurvature_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_curvature COMMAND maz_meshcurvature_tests)
set_tests_properties(mesh_curvature PROPERTIES TIMEOUT 30)

add_executable(maz_meshvoxelize_tests render/meshvoxelize.cpp)
target_link_libraries(maz_meshvoxelize_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_voxelize COMMAND maz_meshvoxelize_tests)
set_tests_properties(mesh_voxelize PROPERTIES TIMEOUT 30)

add_executable(maz_meshcleanup_tests render/meshcleanup.cpp)
target_link_libraries(maz_meshcleanup_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_cleanup COMMAND maz_meshcleanup_tests)
set_tests_properties(mesh_cleanup PROPERTIES TIMEOUT 30)

add_executable(maz_meshgeodesic_tests render/meshgeodesic.cpp)
target_link_libraries(maz_meshgeodesic_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_geodesic COMMAND maz_meshgeodesic_tests)
set_tests_properties(mesh_geodesic PROPERTIES TIMEOUT 30)

add_executable(maz_meshslice_tests render/meshslice.cpp)
target_link_libraries(maz_meshslice_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_slice COMMAND maz_meshslice_tests)
set_tests_properties(mesh_slice PROPERTIES TIMEOUT 30)

add_executable(maz_meshstrip_tests render/meshstrip.cpp)
target_link_libraries(maz_meshstrip_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_strip COMMAND maz_meshstrip_tests)
set_tests_properties(mesh_strip PROPERTIES TIMEOUT 30)

add_executable(maz_meshtopologysummary_tests render/meshtopologysummary.cpp)
target_link_libraries(maz_meshtopologysummary_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_topology_summary COMMAND maz_meshtopologysummary_tests)
set_tests_properties(mesh_topology_summary PROPERTIES TIMEOUT 30)

add_executable(maz_meshcontainment_tests render/meshcontainment.cpp)
target_link_libraries(maz_meshcontainment_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_containment COMMAND maz_meshcontainment_tests)
set_tests_properties(mesh_containment PROPERTIES TIMEOUT 30)

add_executable(maz_meshmirror_tests render/meshmirror.cpp)
target_link_libraries(maz_meshmirror_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_mirror COMMAND maz_meshmirror_tests)
set_tests_properties(mesh_mirror PROPERTIES TIMEOUT 30)

add_executable(maz_meshsharpedges_tests render/meshsharpedges.cpp)
target_link_libraries(maz_meshsharpedges_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_sharp_edges COMMAND maz_meshsharpedges_tests)
set_tests_properties(mesh_sharp_edges PROPERTIES TIMEOUT 30)

add_executable(maz_meshfacet_tests render/meshfacet.cpp)
target_link_libraries(maz_meshfacet_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_facet COMMAND maz_meshfacet_tests)
set_tests_properties(mesh_facet PROPERTIES TIMEOUT 30)

add_executable(maz_meshuvseams_tests render/meshuvseams.cpp)
target_link_libraries(maz_meshuvseams_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_uv_seams COMMAND maz_meshuvseams_tests)
set_tests_properties(mesh_uv_seams PROPERTIES TIMEOUT 30)

add_executable(maz_meshuvproject_tests render/meshuvproject.cpp)
target_link_libraries(maz_meshuvproject_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_uv_project COMMAND maz_meshuvproject_tests)
set_tests_properties(mesh_uv_project PROPERTIES TIMEOUT 30)

add_executable(maz_meshuvradial_tests render/meshuvradial.cpp)
target_link_libraries(maz_meshuvradial_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_uv_radial COMMAND maz_meshuvradial_tests)
set_tests_properties(mesh_uv_radial PROPERTIES TIMEOUT 30)

add_executable(maz_meshvertexcolorao_tests render/meshvertexcolorao.cpp)
target_link_libraries(maz_meshvertexcolorao_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_vertex_color_ao COMMAND maz_meshvertexcolorao_tests)
set_tests_properties(mesh_vertex_color_ao PROPERTIES TIMEOUT 30)

add_executable(maz_meshstats_tests render/meshstats.cpp)
target_link_libraries(maz_meshstats_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_stats COMMAND maz_meshstats_tests)
set_tests_properties(mesh_stats PROPERTIES TIMEOUT 30)

add_executable(maz_meshsurfacesample_tests render/meshsurfacesample.cpp)
target_link_libraries(maz_meshsurfacesample_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_surface_sample COMMAND maz_meshsurfacesample_tests)
set_tests_properties(mesh_surface_sample PROPERTIES TIMEOUT 30)

add_executable(maz_meshvertexcolorcavity_tests render/meshvertexcolorcavity.cpp)
target_link_libraries(maz_meshvertexcolorcavity_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_vertex_color_cavity COMMAND maz_meshvertexcolorcavity_tests)
set_tests_properties(mesh_vertex_color_cavity PROPERTIES TIMEOUT 30)

add_executable(maz_meshthickness_tests render/meshthickness.cpp)
target_link_libraries(maz_meshthickness_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_thickness COMMAND maz_meshthickness_tests)
set_tests_properties(mesh_thickness PROPERTIES TIMEOUT 30)

add_executable(maz_meshfeaturelines_tests render/meshfeaturelines.cpp)
target_link_libraries(maz_meshfeaturelines_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_feature_lines COMMAND maz_meshfeaturelines_tests)
set_tests_properties(mesh_feature_lines PROPERTIES TIMEOUT 30)

add_executable(maz_meshweldauto_tests render/meshweldauto.cpp)
target_link_libraries(maz_meshweldauto_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_weld_auto COMMAND maz_meshweldauto_tests)
set_tests_properties(mesh_weld_auto PROPERTIES TIMEOUT 30)

add_executable(maz_meshsymmetry_tests render/meshsymmetry.cpp)
target_link_libraries(maz_meshsymmetry_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_symmetry COMMAND maz_meshsymmetry_tests)
set_tests_properties(mesh_symmetry PROPERTIES TIMEOUT 30)

add_executable(maz_meshvalence_tests render/meshvalence.cpp)
target_link_libraries(maz_meshvalence_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_valence COMMAND maz_meshvalence_tests)
set_tests_properties(mesh_valence PROPERTIES TIMEOUT 30)

add_executable(maz_meshwinding_tests render/meshwinding.cpp)
target_link_libraries(maz_meshwinding_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_winding COMMAND maz_meshwinding_tests)
set_tests_properties(mesh_winding PROPERTIES TIMEOUT 30)

add_executable(maz_meshdegenerate_tests render/meshdegenerate.cpp)
target_link_libraries(maz_meshdegenerate_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_degenerate COMMAND maz_meshdegenerate_tests)
set_tests_properties(mesh_degenerate PROPERTIES TIMEOUT 30)

add_executable(maz_meshvertexcolorsmooth_tests render/meshvertexcolorsmooth.cpp)
target_link_libraries(maz_meshvertexcolorsmooth_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_vertex_color_smooth COMMAND maz_meshvertexcolorsmooth_tests)
set_tests_properties(mesh_vertex_color_smooth PROPERTIES TIMEOUT 30)

add_executable(maz_meshsolidity_tests render/meshsolidity.cpp)
target_link_libraries(maz_meshsolidity_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_solidity COMMAND maz_meshsolidity_tests)
set_tests_properties(mesh_solidity PROPERTIES TIMEOUT 30)

add_executable(maz_meshwatertight_tests render/meshwatertight.cpp)
target_link_libraries(maz_meshwatertight_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_watertight COMMAND maz_meshwatertight_tests)
set_tests_properties(mesh_watertight PROPERTIES TIMEOUT 30)

add_executable(maz_meshholefill_tests render/meshholefill.cpp)
target_link_libraries(maz_meshholefill_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_hole_fill COMMAND maz_meshholefill_tests)
set_tests_properties(mesh_hole_fill PROPERTIES TIMEOUT 30)

add_executable(maz_meshdominantplane_tests render/meshdominantplane.cpp)
target_link_libraries(maz_meshdominantplane_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_dominant_plane COMMAND maz_meshdominantplane_tests)
set_tests_properties(mesh_dominant_plane PROPERTIES TIMEOUT 30)

add_executable(maz_meshrecenter_tests render/meshrecenter.cpp)
target_link_libraries(maz_meshrecenter_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_recenter COMMAND maz_meshrecenter_tests)
set_tests_properties(mesh_recenter PROPERTIES TIMEOUT 30)

add_executable(maz_meshnormalize_tests render/meshnormalize.cpp)
target_link_libraries(maz_meshnormalize_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_normalize COMMAND maz_meshnormalize_tests)
set_tests_properties(mesh_normalize PROPERTIES TIMEOUT 30)

add_executable(maz_meshtransform_tests render/meshtransform.cpp)
target_link_libraries(maz_meshtransform_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_transform COMMAND maz_meshtransform_tests)
set_tests_properties(mesh_transform PROPERTIES TIMEOUT 30)

add_executable(maz_meshmerge_tests render/meshmerge.cpp)
target_link_libraries(maz_meshmerge_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_merge COMMAND maz_meshmerge_tests)
set_tests_properties(mesh_merge PROPERTIES TIMEOUT 30)

add_executable(maz_meshflip_tests render/meshflip.cpp)
target_link_libraries(maz_meshflip_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_flip COMMAND maz_meshflip_tests)
set_tests_properties(mesh_flip PROPERTIES TIMEOUT 30)

add_executable(maz_meshvertexcolorgradient_tests render/meshvertexcolorgradient.cpp)
target_link_libraries(maz_meshvertexcolorgradient_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_vertex_color_gradient COMMAND maz_meshvertexcolorgradient_tests)
set_tests_properties(mesh_vertex_color_gradient PROPERTIES TIMEOUT 30)

add_executable(maz_meshreindex_tests render/meshreindex.cpp)
target_link_libraries(maz_meshreindex_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_reindex COMMAND maz_meshreindex_tests)
set_tests_properties(mesh_reindex PROPERTIES TIMEOUT 30)

add_executable(maz_meshpoissonprune_tests render/meshpoissonprune.cpp)
target_link_libraries(maz_meshpoissonprune_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_poisson_prune COMMAND maz_meshpoissonprune_tests)
set_tests_properties(mesh_poisson_prune PROPERTIES TIMEOUT 30)

add_executable(maz_meshsnapgrid_tests render/meshsnapgrid.cpp)
target_link_libraries(maz_meshsnapgrid_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_snap_grid COMMAND maz_meshsnapgrid_tests)
set_tests_properties(mesh_snap_grid PROPERTIES TIMEOUT 30)

add_executable(maz_meshexplode_tests render/meshexplode.cpp)
target_link_libraries(maz_meshexplode_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_explode COMMAND maz_meshexplode_tests)
set_tests_properties(mesh_explode PROPERTIES TIMEOUT 30)

add_executable(maz_meshsolidify_tests render/meshsolidify.cpp)
target_link_libraries(maz_meshsolidify_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_solidify COMMAND maz_meshsolidify_tests)
set_tests_properties(mesh_solidify PROPERTIES TIMEOUT 30)

add_executable(maz_meshdisplace_tests render/meshdisplace.cpp)
target_link_libraries(maz_meshdisplace_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_displace COMMAND maz_meshdisplace_tests)
set_tests_properties(mesh_displace PROPERTIES TIMEOUT 30)

add_executable(maz_meshtwist_tests render/meshtwist.cpp)
target_link_libraries(maz_meshtwist_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_twist COMMAND maz_meshtwist_tests)
set_tests_properties(mesh_twist PROPERTIES TIMEOUT 30)

add_executable(maz_meshtaper_tests render/meshtaper.cpp)
target_link_libraries(maz_meshtaper_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_taper COMMAND maz_meshtaper_tests)
set_tests_properties(mesh_taper PROPERTIES TIMEOUT 30)

add_executable(maz_meshspherify_tests render/meshspherify.cpp)
target_link_libraries(maz_meshspherify_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_spherify COMMAND maz_meshspherify_tests)
set_tests_properties(mesh_spherify PROPERTIES TIMEOUT 30)

add_executable(maz_meshbend_tests render/meshbend.cpp)
target_link_libraries(maz_meshbend_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_bend COMMAND maz_meshbend_tests)
set_tests_properties(mesh_bend PROPERTIES TIMEOUT 30)

add_executable(maz_meshripple_tests render/meshripple.cpp)
target_link_libraries(maz_meshripple_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_ripple COMMAND maz_meshripple_tests)
set_tests_properties(mesh_ripple PROPERTIES TIMEOUT 30)

add_executable(maz_meshboundingcylinder_tests render/meshboundingcylinder.cpp)
target_link_libraries(maz_meshboundingcylinder_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_bounding_cylinder COMMAND maz_meshboundingcylinder_tests)
set_tests_properties(mesh_bounding_cylinder PROPERTIES TIMEOUT 30)

add_executable(maz_meshflatten_tests render/meshflatten.cpp)
target_link_libraries(maz_meshflatten_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_flatten COMMAND maz_meshflatten_tests)
set_tests_properties(mesh_flatten PROPERTIES TIMEOUT 30)

add_executable(maz_meshcomponentcolor_tests render/meshcomponentcolor.cpp)
target_link_libraries(maz_meshcomponentcolor_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_component_color COMMAND maz_meshcomponentcolor_tests)
set_tests_properties(mesh_component_color PROPERTIES TIMEOUT 30)

add_executable(maz_meshprincipalaxes_tests render/meshprincipalaxes.cpp)
target_link_libraries(maz_meshprincipalaxes_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_principal_axes COMMAND maz_meshprincipalaxes_tests)
set_tests_properties(mesh_principal_axes PROPERTIES TIMEOUT 30)

add_executable(maz_meshinset_tests render/meshinset.cpp)
target_link_libraries(maz_meshinset_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_inset COMMAND maz_meshinset_tests)
set_tests_properties(mesh_inset PROPERTIES TIMEOUT 30)

add_executable(maz_meshcurvaturecolor_tests render/meshcurvaturecolor.cpp)
target_link_libraries(maz_meshcurvaturecolor_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_curvature_color COMMAND maz_meshcurvaturecolor_tests)
set_tests_properties(mesh_curvature_color PROPERTIES TIMEOUT 30)

add_executable(maz_meshextrude_tests render/meshextrude.cpp)
target_link_libraries(maz_meshextrude_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_extrude COMMAND maz_meshextrude_tests)
set_tests_properties(mesh_extrude PROPERTIES TIMEOUT 30)

add_executable(maz_meshslicelayers_tests render/meshslicelayers.cpp)
target_link_libraries(maz_meshslicelayers_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_slice_layers COMMAND maz_meshslicelayers_tests)
set_tests_properties(mesh_slice_layers PROPERTIES TIMEOUT 30)

add_executable(maz_meshrevolve_tests render/meshrevolve.cpp)
target_link_libraries(maz_meshrevolve_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_revolve COMMAND maz_meshrevolve_tests)
set_tests_properties(mesh_revolve PROPERTIES TIMEOUT 30)

add_executable(maz_meshsweep_tests render/meshsweep.cpp)
target_link_libraries(maz_meshsweep_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_sweep COMMAND maz_meshsweep_tests)
set_tests_properties(mesh_sweep PROPERTIES TIMEOUT 30)

add_executable(maz_meshheightfield_tests render/meshheightfield.cpp)
target_link_libraries(maz_meshheightfield_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_heightfield COMMAND maz_meshheightfield_tests)
set_tests_properties(mesh_heightfield PROPERTIES TIMEOUT 30)

add_executable(maz_meshskin_tests render/meshskin.cpp)
target_link_libraries(maz_meshskin_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_skin COMMAND maz_meshskin_tests)
set_tests_properties(mesh_skin PROPERTIES TIMEOUT 30)

add_executable(maz_meshicosphere_tests render/meshicosphere.cpp)
target_link_libraries(maz_meshicosphere_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_icosphere COMMAND maz_meshicosphere_tests)
set_tests_properties(mesh_icosphere PROPERTIES TIMEOUT 30)

add_executable(maz_meshsilhouette_tests render/meshsilhouette.cpp)
target_link_libraries(maz_meshsilhouette_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_silhouette COMMAND maz_meshsilhouette_tests)
set_tests_properties(mesh_silhouette PROPERTIES TIMEOUT 30)

add_executable(maz_meshclosestpoint_tests render/meshclosestpoint.cpp)
target_link_libraries(maz_meshclosestpoint_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_closest_point COMMAND maz_meshclosestpoint_tests)
set_tests_properties(mesh_closest_point PROPERTIES TIMEOUT 30)

add_executable(maz_mesharrow_tests render/mesharrow.cpp)
target_link_libraries(maz_mesharrow_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_arrow COMMAND maz_mesharrow_tests)
set_tests_properties(mesh_arrow PROPERTIES TIMEOUT 30)

add_executable(maz_meshextrudepolygon_tests render/meshextrudepolygon.cpp)
target_link_libraries(maz_meshextrudepolygon_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_extrude_polygon COMMAND maz_meshextrudepolygon_tests)
set_tests_properties(mesh_extrude_polygon PROPERTIES TIMEOUT 30)

add_executable(maz_meshprojectedarea_tests render/meshprojectedarea.cpp)
target_link_libraries(maz_meshprojectedarea_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_projected_area COMMAND maz_meshprojectedarea_tests)
set_tests_properties(mesh_projected_area PROPERTIES TIMEOUT 30)

add_executable(maz_shapes2d_tests render/shapes2d.cpp)
target_link_libraries(maz_shapes2d_tests PRIVATE maz::maz maz_warnings)
add_test(NAME shapes2d COMMAND maz_shapes2d_tests)
set_tests_properties(shapes2d PROPERTIES TIMEOUT 30)

add_executable(maz_shapes2dgear_tests render/shapes2dgear.cpp)
target_link_libraries(maz_shapes2dgear_tests PRIVATE maz::maz maz_warnings)
add_test(NAME shapes2d_gear COMMAND maz_shapes2dgear_tests)
set_tests_properties(shapes2d_gear PROPERTIES TIMEOUT 30)

add_executable(maz_meshextrudescaled_tests render/meshextrudescaled.cpp)
target_link_libraries(maz_meshextrudescaled_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_extrude_scaled COMMAND maz_meshextrudescaled_tests)
set_tests_properties(mesh_extrude_scaled PROPERTIES TIMEOUT 30)

add_executable(maz_imagepatterns_tests render/imagepatterns.cpp)
target_link_libraries(maz_imagepatterns_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_patterns COMMAND maz_imagepatterns_tests)
set_tests_properties(image_patterns PROPERTIES TIMEOUT 30)

add_executable(maz_imagenormalmap_tests render/imagenormalmap.cpp)
target_link_libraries(maz_imagenormalmap_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_normalmap COMMAND maz_imagenormalmap_tests)
set_tests_properties(image_normalmap PROPERTIES TIMEOUT 30)

add_executable(maz_meshwireframe_tests render/meshwireframe.cpp)
target_link_libraries(maz_meshwireframe_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_wireframe COMMAND maz_meshwireframe_tests)
set_tests_properties(mesh_wireframe PROPERTIES TIMEOUT 30)

add_executable(maz_shapes2dpie_tests render/shapes2dpie.cpp)
target_link_libraries(maz_shapes2dpie_tests PRIVATE maz::maz maz_warnings)
add_test(NAME shapes2d_pie COMMAND maz_shapes2dpie_tests)
set_tests_properties(shapes2d_pie PROPERTIES TIMEOUT 30)

add_executable(maz_meshextrudering_tests render/meshextrudering.cpp)
target_link_libraries(maz_meshextrudering_tests PRIVATE maz::maz maz_warnings)
add_test(NAME mesh_extrude_ring COMMAND maz_meshextrudering_tests)
set_tests_properties(mesh_extrude_ring PROPERTIES TIMEOUT 30)

add_executable(maz_imagenoise_tests render/imagenoise.cpp)
target_link_libraries(maz_imagenoise_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_noise COMMAND maz_imagenoise_tests)
set_tests_properties(image_noise PROPERTIES TIMEOUT 30)

add_executable(maz_imagecellular_tests render/imagecellular.cpp)
target_link_libraries(maz_imagecellular_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_cellular COMMAND maz_imagecellular_tests)
set_tests_properties(image_cellular PROPERTIES TIMEOUT 30)

add_executable(maz_imagegradientmap_tests render/imagegradientmap.cpp)
target_link_libraries(maz_imagegradientmap_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_gradient_map COMMAND maz_imagegradientmap_tests)
set_tests_properties(image_gradient_map PROPERTIES TIMEOUT 30)

add_executable(maz_imageblend_tests render/imageblend.cpp)
target_link_libraries(maz_imageblend_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_blend COMMAND maz_imageblend_tests)
set_tests_properties(image_blend PROPERTIES TIMEOUT 30)

add_executable(maz_imageadjust_tests render/imageadjust.cpp)
target_link_libraries(maz_imageadjust_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_adjust COMMAND maz_imageadjust_tests)
set_tests_properties(image_adjust PROPERTIES TIMEOUT 30)

add_executable(maz_imagebrick_tests render/imagebrick.cpp)
target_link_libraries(maz_imagebrick_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_brick COMMAND maz_imagebrick_tests)
set_tests_properties(image_brick PROPERTIES TIMEOUT 30)

add_executable(maz_interceptaim_tests game/interceptaim.cpp)
target_link_libraries(maz_interceptaim_tests PRIVATE maz::maz maz_warnings)
add_test(NAME intercept_aim COMMAND maz_interceptaim_tests)
set_tests_properties(intercept_aim PROPERTIES TIMEOUT 30)

add_executable(maz_imagecolorblind_tests render/imagecolorblind.cpp)
target_link_libraries(maz_imagecolorblind_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_colorblind COMMAND maz_imagecolorblind_tests)
set_tests_properties(image_colorblind PROPERTIES TIMEOUT 30)

add_executable(maz_contrastratio_tests render/contrastratio.cpp)
target_link_libraries(maz_contrastratio_tests PRIVATE maz::maz maz_warnings)
add_test(NAME contrast_ratio COMMAND maz_contrastratio_tests)
set_tests_properties(contrast_ratio PROPERTIES TIMEOUT 30)

add_executable(maz_imagemarblewood_tests render/imagemarblewood.cpp)
target_link_libraries(maz_imagemarblewood_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_marble_wood COMMAND maz_imagemarblewood_tests)
set_tests_properties(image_marble_wood PROPERTIES TIMEOUT 30)

add_executable(maz_numberformat_ordinal_tests core/numberformat_ordinal.cpp)
target_link_libraries(maz_numberformat_ordinal_tests PRIVATE maz::maz maz_warnings)
add_test(NAME numberformat_ordinal COMMAND maz_numberformat_ordinal_tests)
set_tests_properties(numberformat_ordinal PROPERTIES TIMEOUT 30)

add_executable(maz_dice_tests game/dice.cpp)
target_link_libraries(maz_dice_tests PRIVATE maz::maz maz_warnings)
add_test(NAME dice COMMAND maz_dice_tests)
set_tests_properties(dice PROPERTIES TIMEOUT 30)

add_executable(maz_uuid_tests core/uuid.cpp)
target_link_libraries(maz_uuid_tests PRIVATE maz::maz maz_warnings)
add_test(NAME uuid COMMAND maz_uuid_tests)
set_tests_properties(uuid PROPERTIES TIMEOUT 30)

add_executable(maz_musictheory_tests audio/musictheory.cpp)
target_link_libraries(maz_musictheory_tests PRIVATE maz::maz maz_warnings)
add_test(NAME musictheory COMMAND maz_musictheory_tests)
set_tests_properties(musictheory PROPERTIES TIMEOUT 30)

add_executable(maz_musicscales_tests audio/musicscales.cpp)
target_link_libraries(maz_musicscales_tests PRIVATE maz::maz maz_warnings)
add_test(NAME musicscales COMMAND maz_musicscales_tests)
set_tests_properties(musicscales PROPERTIES TIMEOUT 30)

add_executable(maz_colorharmony_tests render/colorharmony.cpp)
target_link_libraries(maz_colorharmony_tests PRIVATE maz::maz maz_warnings)
add_test(NAME color_harmony COMMAND maz_colorharmony_tests)
set_tests_properties(color_harmony PROPERTIES TIMEOUT 30)

add_executable(maz_cubicbezier_tests anim/cubicbezier.cpp)
target_link_libraries(maz_cubicbezier_tests PRIVATE maz::maz maz_warnings)
add_test(NAME cubic_bezier COMMAND maz_cubicbezier_tests)
set_tests_properties(cubic_bezier PROPERTIES TIMEOUT 30)

add_executable(maz_imagevoronoi_tests render/imagevoronoi.cpp)
target_link_libraries(maz_imagevoronoi_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_voronoi COMMAND maz_imagevoronoi_tests)
set_tests_properties(image_voronoi PROPERTIES TIMEOUT 30)

add_executable(maz_spring_tests core/spring.cpp)
target_link_libraries(maz_spring_tests PRIVATE maz::maz maz_warnings)
add_test(NAME spring COMMAND maz_spring_tests)
set_tests_properties(spring PROPERTIES TIMEOUT 30)

add_executable(maz_spread_tests game/spread.cpp)
target_link_libraries(maz_spread_tests PRIVATE maz::maz maz_warnings)
add_test(NAME spread COMMAND maz_spread_tests)
set_tests_properties(spread PROPERTIES TIMEOUT 30)

add_executable(maz_viewcone_tests game/viewcone.cpp)
target_link_libraries(maz_viewcone_tests PRIVATE maz::maz maz_warnings)
add_test(NAME viewcone COMMAND maz_viewcone_tests)
set_tests_properties(viewcone PROPERTIES TIMEOUT 30)

add_executable(maz_imagelineargradient_tests render/imagelineargradient.cpp)
target_link_libraries(maz_imagelineargradient_tests PRIVATE maz::maz maz_warnings)
add_test(NAME image_linear_gradient COMMAND maz_imagelineargradient_tests)
set_tests_properties(image_linear_gradient PROPERTIES TIMEOUT 30)

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












