#version 450
// Fullscreen triangle for the sky background (no vertex buffer).

layout(location = 0) out vec2 vNdc;

void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vNdc = uv * 2.0 - 1.0;
    gl_Position = vec4(vNdc, 0.0, 1.0);
}
