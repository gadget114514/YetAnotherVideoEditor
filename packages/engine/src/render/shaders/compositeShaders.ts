export const quadVertShader = `#version 300 es
precision highp float;

layout(location = 0) in vec2 aPosition;
out vec2 vUv;

void main() {
    vUv = aPosition * 0.5 + 0.5;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
`;

export const compositeFragShader = `#version 300 es
precision highp float;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uSrcTex; // Current layer texture
uniform sampler2D uDstTex; // Background composite result
uniform float uOpacity;
uniform int uBlendMode;
uniform vec4 uCropRect;    // x, y, w, h in 0..1
uniform mat4 uTransform;

vec3 blend(int mode, vec3 base, vec3 src) {
    if (mode == 0) return src;                                     // Normal
    if (mode == 1) return base + src;                              // Add
    if (mode == 2) return base * src;                              // Multiply
    if (mode == 3) return 1.0 - (1.0 - base) * (1.0 - src);        // Screen
    if (mode == 4) return mix(2.0 * base * src,                    // Overlay
                              1.0 - 2.0 * (1.0 - base) * (1.0 - src),
                              step(0.5, base));
    if (mode == 5) return min(base, src);                          // Darken
    if (mode == 6) return max(base, src);                          // Lighten
    if (mode == 7) return abs(base - src);                         // Difference
    if (mode == 8) return base + src - 2.0 * base * src;           // Exclusion
    return src;
}

void main() {
    // Apply 2D transform to UV coordinates
    vec4 transformedPos = uTransform * vec4((vUv - 0.5) * 2.0, 0.0, 1.0);
    vec2 layerUv = transformedPos.xy * 0.5 + 0.5;

    vec4 d = texture(uDstTex, vUv);

    // Discard if transformed UV is out of [0, 1] bounds
    if (layerUv.x < 0.0 || layerUv.x > 1.0 || layerUv.y < 0.0 || layerUv.y > 1.0) {
        fragColor = d;
        return;
    }

    // Apply crop rect
    vec2 srcUv = uCropRect.xy + layerUv * uCropRect.zw;
    vec4 s = texture(uSrcTex, srcUv);
    s.a *= uOpacity;

    if (uBlendMode == 9) { // AlphaMask: src luminance multiplies dst alpha
        float lum = dot(s.rgb, vec3(0.2126, 0.7152, 0.0722));
        fragColor = vec4(d.rgb, d.a * lum);
        return;
    }

    vec3 blended = blend(uBlendMode, d.rgb, s.rgb);
    // Standard alpha blending over dst
    fragColor = vec4(mix(d.rgb, blended, s.a), max(d.a, s.a));
}
`;
