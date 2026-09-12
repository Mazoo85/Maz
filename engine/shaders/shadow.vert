#version 450
// Depth-only pass: render casters from the light's point of view into the shadow map.
// The vertex buffer is the full mesh vertex; only position is used here.

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform Push {
    mat4 lightMVP;
} pc;

void main() {
    gl_Position = pc.lightMVP * vec4(inPos, 1.0);
}
