#version 440

// フィルタ / トランジション用のフルスクリーン三角形 (3.9 / 3.10)。
// layer_blend 系と違い変換を掛けない。UV は頂点位置から直に作る。

layout(location = 0) in vec2 aPos;        // クリップ空間 (-1,-1) (3,-1) (-1,3)

layout(location = 0) out vec2 vUv;

void main()
{
    vUv = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
