#version 440

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ubuf {
    int colorSpace;      // 1=BT709 limited, 2=BT709 full, 3=BT2020
    float _pad0;
    float _pad1;
    float _pad2;
} ub;

layout(binding = 1) uniform sampler2D yTex;
layout(binding = 2) uniform sampler2D uTex;
layout(binding = 3) uniform sampler2D vTex;

vec3 convert(vec3 yuv)
{
    if (ub.colorSpace == 3) {
        // BT.2020 limited
        const mat3 m = mat3(
            1.16438356, 1.16438356, 1.16438356,
            0.00000000, -0.18732427, 2.14177232,
            1.67867411, -0.65042427, 0.00000000);
        return m * (yuv - vec3(16.0 / 255.0));
    }
    if (ub.colorSpace == 2) {
        // BT.709 full
        const mat3 m = mat3(
            1.00000000, 1.00000000, 1.00000000,
            0.00000000, -0.18732427, 1.85560000,
            1.57480000, -0.46812427, 0.00000000);
        return m * yuv;
    }
    // BT.709 limited (既定)
    const mat3 m = mat3(
        1.16438356, 1.16438356, 1.16438356,
        0.00000000, -0.21324861, 2.11240179,
        1.79274107, -0.53290933, 0.00000000);
    return m * (yuv - vec3(16.0 / 255.0));
}

vec3 toneMapPq(vec3 c)
{
    // 簡易 PQ -> SDR (Reinhard 風)。実運用は BT.2390 EETF を使う。
    return c / (c + vec3(1.0));
}

void main()
{
    float y = texture(yTex, vUv).r;
    float u = texture(uTex, vUv).r;
    float v = texture(vTex, vUv).r;

    vec3 rgb = clamp(convert(vec3(y, u, v)), 0.0, 1.0);

    if (ub.colorSpace == 3)
        rgb = toneMapPq(rgb);   ///< HDR ソースは SDR へトーンマップ

    fragColor = vec4(rgb, 1.0);
}
