#version 440

// 色系ビデオフィルタ (3.9.2)。
//   kind 0 : yave.filter.colorAdjust  p0 = (brightness, contrast, saturation, gamma)
//   kind 1 : yave.filter.mono         p0.x = amount
//   kind 2 : yave.filter.sepia        p0.x = amount
//
// params のスロット順は VideoFilter.cpp の resolveFilterParams() と一致させること。

layout(location = 0) in  vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ubuf {
    vec4  p0;          // params[0..3]
    vec4  p1;          // params[4..7]
    vec4  texel;       // xy = 1/size, zw = 未使用
    int   kind;
    int   subPass;
    int   _pad0;
    int   _pad1;
} ub;

layout(binding = 1) uniform sampler2D srcTex;

const vec3 kLuma = vec3(0.2126, 0.7152, 0.0722);

void main()
{
    vec4 c = texture(srcTex, vUv);

    if (ub.kind == 0) {
        float brightness = ub.p0.x;
        float contrast   = ub.p0.y;
        float saturation = ub.p0.z;
        float gamma      = max(ub.p0.w, 0.001);

        vec3 rgb = c.rgb + vec3(brightness);
        rgb = (rgb - 0.5) * contrast + 0.5;
        float lum = dot(rgb, kLuma);
        rgb = mix(vec3(lum), rgb, saturation);
        rgb = pow(max(rgb, vec3(0.0)), vec3(1.0 / gamma));
        c.rgb = clamp(rgb, 0.0, 1.0);
    }
    else if (ub.kind == 1) {
        float amount = clamp(ub.p0.x, 0.0, 1.0);
        c.rgb = mix(c.rgb, vec3(dot(c.rgb, kLuma)), amount);
    }
    else if (ub.kind == 2) {
        float amount = clamp(ub.p0.x, 0.0, 1.0);
        vec3 sepia = vec3(
            dot(c.rgb, vec3(0.393, 0.769, 0.189)),
            dot(c.rgb, vec3(0.349, 0.686, 0.168)),
            dot(c.rgb, vec3(0.272, 0.534, 0.131)));
        c.rgb = mix(c.rgb, clamp(sepia, 0.0, 1.0), amount);
    }

    fragColor = c;
}
