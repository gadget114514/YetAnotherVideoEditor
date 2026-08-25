#version 440

layout(location = 0) in vec2 aCorner;         // 0..1 の矩形頂点 (共通)

// per-instance
layout(location = 1) in vec4 iRect;           // layoutRect (x, y, w, h)
layout(location = 2) in vec4 iUv;             // atlasUv    (u, v, w, h)
layout(location = 3) in vec4 iColor;
layout(location = 4) in mat4 iTransform;      // location 4,5,6,7 を消費

layout(std140, binding = 0) uniform Ubuf {
    mat4  blockTransform;
    vec2  canvasSize;
    float blockOpacity;
    float _pad;
} ub;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;

void main()
{
    vec2 localPos  = iRect.xy + aCorner * iRect.zw;
    vec2 glyphCtr  = iRect.xy + iRect.zw * 0.5;

    // グリフ中心を原点にして変換 -> 元の位置へ戻す
    vec4 p = iTransform * vec4(localPos - glyphCtr, 0.0, 1.0);
    p.xy += glyphCtr;

    // 正規化クリップ空間へ (canvasSize 基準。Y は下向き正 -> 上向き正へ反転)
    vec2 ndc = (p.xy / ub.canvasSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    gl_Position = ub.blockTransform * gl_Position;

    vUv    = iUv.xy + aCorner * iUv.zw;
    vColor = vec4(iColor.rgb, iColor.a * ub.blockOpacity);
}
