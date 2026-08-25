#version 440

// 分離ガウシアンぼかし (3.9.2)。
//   p0.x = radius (px)  /  p0.y = direction (0=両方向, 1=水平のみ, 2=垂直のみ)
//   subPass 0 = 水平、subPass 1 = 垂直
//
// サンプル数は半径から決めるが、4K でのフレーム予算 (3.6) を守るため
// kMaxTaps で頭打ちにし、それ以上はステップ幅を広げて近似する。

layout(location = 0) in  vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ubuf {
    vec4  p0;
    vec4  p1;
    vec4  texel;       // xy = 1/size
    int   kind;
    int   subPass;
    int   _pad0;
    int   _pad1;
} ub;

layout(binding = 1) uniform sampler2D srcTex;

const int kMaxTaps = 16;

void main()
{
    float radius    = max(ub.p0.x, 0.0);
    int   direction = int(ub.p0.y + 0.5);

    // 方向指定でこのサブパスが不要なら素通しする
    bool horizontal = (ub.subPass == 0);
    if (radius <= 0.0
        || (direction == 1 && !horizontal)
        || (direction == 2 && horizontal)) {
        fragColor = texture(srcTex, vUv);
        return;
    }

    vec2 axis = horizontal ? vec2(ub.texel.x, 0.0) : vec2(0.0, ub.texel.y);

    int   taps = int(min(float(kMaxTaps), ceil(radius)));
    float step = radius / float(max(taps, 1));
    float sigma = max(radius * 0.5, 0.0001);

    vec4  sum    = texture(srcTex, vUv);
    float weight = 1.0;

    for (int i = 1; i <= kMaxTaps; ++i) {
        if (i > taps)
            break;
        float offset = float(i) * step;
        float w = exp(-(offset * offset) / (2.0 * sigma * sigma));
        sum    += texture(srcTex, vUv + axis * offset) * w;
        sum    += texture(srcTex, vUv - axis * offset) * w;
        weight += 2.0 * w;
    }

    fragColor = sum / weight;
}
