#version 440

layout(location = 0) in  vec2 vUv;
layout(location = 1) in  vec4 vColor;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D atlasTex;

void main()
{
    vec4 g = texture(atlasTex, vUv);
    fragColor = g * vColor;      // atlas は premultiplied alpha
}
