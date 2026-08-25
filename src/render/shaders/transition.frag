#version 440

// クリップ境界のトランジション (3.10)。
// mode は Transition.cpp の transitionShaderMode() と一致させること。
//   0 = dissolve / 1 = fadeToBlack / 2 = wipe / 3 = slide / 4 = push
//
// progress: 0 = from が全面、1 = to が全面

layout(location = 0) in  vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ubuf {
    vec4  params;      // mode 依存 (wipe: x=angle[rad], y=softness / slide,push: x=direction)
    vec4  fillColor;   // fadeToBlack の中間色
    float progress;
    int   mode;
    int   _pad0;
    int   _pad1;
} ub;

layout(binding = 1) uniform sampler2D fromTex;
layout(binding = 2) uniform sampler2D toTex;

vec2 directionVector(float dirIndex)
{
    int d = int(dirIndex + 0.5);
    if (d == 1) return vec2(-1.0,  0.0);   // 右から左
    if (d == 2) return vec2( 0.0,  1.0);   // 下から上
    if (d == 3) return vec2( 0.0, -1.0);   // 上から下
    return vec2(1.0, 0.0);                 // 左から右 (既定)
}

void main()
{
    float t = clamp(ub.progress, 0.0, 1.0);

    if (ub.mode == 1) {
        // fadeToBlack: 前半で中間色へ、後半で中間色から出る
        vec4 mid = ub.fillColor;
        if (t < 0.5)
            fragColor = mix(texture(fromTex, vUv), mid, t * 2.0);
        else
            fragColor = mix(mid, texture(toTex, vUv), (t - 0.5) * 2.0);
        return;
    }

    if (ub.mode == 2) {
        // wipe: 角度方向の座標で閾値を切る
        float angle    = ub.params.x;
        float softness = max(ub.params.y, 0.0001);
        vec2  dir      = vec2(cos(angle), sin(angle));
        // 画面中心基準の投影を 0..1 へ正規化する
        float proj = dot(vUv - 0.5, dir) * 0.5 + 0.5;
        float edge = smoothstep(t - softness, t + softness, proj);
        fragColor = mix(texture(toTex, vUv), texture(fromTex, vUv), edge);
        return;
    }

    if (ub.mode == 3 || ub.mode == 4) {
        vec2 dir = directionVector(ub.params.x);
        // slide: to が上に乗って入ってくる。push: from も一緒に押し出される。
        vec2 toUv   = vUv + dir * (1.0 - t);
        vec2 fromUv = (ub.mode == 4) ? vUv + dir * (-t) : vUv;

        bool toInside = all(greaterThanEqual(toUv, vec2(0.0)))
                     && all(lessThanEqual(toUv, vec2(1.0)));
        vec4 fromCol = texture(fromTex, clamp(fromUv, 0.0, 1.0));
        vec4 toCol   = texture(toTex,   clamp(toUv,   0.0, 1.0));
        fragColor = toInside ? toCol : fromCol;
        return;
    }

    // dissolve (既定)
    fragColor = mix(texture(fromTex, vUv), texture(toTex, vUv), t);
}
