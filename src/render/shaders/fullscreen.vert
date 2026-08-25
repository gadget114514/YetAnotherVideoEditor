#version 440

layout(location = 0) in vec2 aCorner;         // 0..1 の矩形頂点 (共通)

layout(std140, binding = 0) uniform Ubuf {
    mat4  mvp;
} ub;

layout(location = 0) out vec2 vUv;

void main()
{
    vUv = vec2(aCorner.x, 1.0 - aCorner.y);
    gl_Position = ub.mvp * vec4(aCorner, 0.0, 1.0);
}
