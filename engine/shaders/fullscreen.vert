#version 450
// Fullscreen triangle generated from gl_VertexIndex (no vertex buffer needed).
// Scaffolding for the Phase 3 pipeline; not yet bound by the M0 clear-only renderer.

layout(location = 0) out vec2 vUV;

void main() {
    vUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);
}
