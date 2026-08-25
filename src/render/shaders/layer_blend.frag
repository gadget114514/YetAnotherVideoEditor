#version 440

layout(location = 0) in  vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ubuf {
    mat4  transform;
    vec4  cropRect;        // x, y, w, h  (0..1)
    float opacity;
    int   blendMode;
    int   colorSpace;      // 0=RGB(full), 1=BT709 limited, 2=BT709 full, 3=BT2020
    float _pad;
} ub;

layout(binding = 1) uniform sampler2D srcTex;      // 上に乗せるレイヤー
layout(binding = 2) uniform sampler2D dstTex;      // ここまでの合成結果

vec3 blend(int mode, vec3 base, vec3 src)
{
    if (mode == 0)  return src;                                    // Normal
    if (mode == 1)  return base + src;                             // Add
    if (mode == 2)  return base * src;                             // Multiply
    if (mode == 3)  return 1.0 - (1.0 - base) * (1.0 - src);       // Screen
    if (mode == 4)  return mix(2.0 * base * src,
                               1.0 - 2.0 * (1.0 - base) * (1.0 - src),
                               step(0.5, base));                   // Overlay
    if (mode == 5)  return min(base, src);                         // Darken
    if (mode == 6)  return max(base, src);                         // Lighten
    if (mode == 7) {                                               // ColorDodge
        return base / max(1.0 - src, vec3(0.001));
    }
    if (mode == 8) {                                               // ColorBurn
        return 1.0 - min((1.0 - base) / max(src, vec3(0.001)), vec3(1.0));
    }
    if (mode == 9)  return abs(base - src);                        // Difference
    if (mode == 10) return base + src - 2.0 * base * src;          // Exclusion
    return src;
}

vec3 yuvToRgb(vec3 yuv, int colorSpace)
{
    if (colorSpace == 1) {
        // BT.709 limited
        const mat3 m = mat3(
            1.16438356, 1.16438356, 1.16438356,
            0.00000000, -0.21324861, 2.11240179,
            1.79274107, -0.53290933, 0.00000000);
        return m * (yuv - vec3(16.0 / 255.0));
    }
    if (colorSpace == 3) {
        // BT.2020 limited (近似)
        const mat3 m = mat3(
            1.16438356, 1.16438356, 1.16438356,
            0.00000000, -0.18732427, 2.14177232,
            1.67867411, -0.65042427, 0.00000000);
        return m * (yuv - vec3(16.0 / 255.0));
    }
    return yuv;   ///< RGB のまま
}

void main()
{
    vec2 uv = ub.cropRect.xy + vUv * ub.cropRect.zw;
    vec4 s  = texture(srcTex, uv);
    vec4 d  = texture(dstTex, vUv);

    s.a *= ub.opacity;

    if (ub.blendMode == 11) {                    // AlphaMask: src の輝度を dst のαに適用
        float lum = dot(s.rgb, vec3(0.2126, 0.7152, 0.0722));
        fragColor = vec4(d.rgb, d.a * lum);
        return;
    }

    vec3 blended = blend(ub.blendMode, d.rgb, s.rgb);
    fragColor    = vec4(mix(d.rgb, blended, s.a), max(d.a, s.a));
}
