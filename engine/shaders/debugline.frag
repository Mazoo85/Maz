#version 450
// Debug line fragment: emit the interpolated vertex color unchanged (unlit).

layout(location = 0) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor;
}
