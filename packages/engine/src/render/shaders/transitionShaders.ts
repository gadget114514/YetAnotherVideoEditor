export const transitionFragShader = `#version 300 es
precision highp float;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uFromTex;
uniform sampler2D uToTex;
uniform float uProgress;    // 0.0 .. 1.0
uniform int uType;          // 0=Dissolve, 1=FadeToBlack, 2=Wipe, 3=Slide, 4=Push
uniform vec4 uColor;        // e.g. vec4(0, 0, 0, 1) for FadeToBlack
uniform float uWipeAngle;   // radians
uniform float uWipeSoftness;
uniform vec2 uDirection;    // for slide/push (-1..1)

void main() {
    float p = clamp(uProgress, 0.0, 1.0);

    if (uType == 0) {
        // Dissolve (Crossfade)
        vec4 fromC = texture(uFromTex, vUv);
        vec4 toC = texture(uToTex, vUv);
        fragColor = mix(fromC, toC, p);
        return;
    }

    if (uType == 1) {
        // Fade to Color (Black)
        if (p < 0.5) {
            vec4 fromC = texture(uFromTex, vUv);
            fragColor = mix(fromC, uColor, p * 2.0);
        } else {
            vec4 toC = texture(uToTex, vUv);
            fragColor = mix(uColor, toC, (p - 0.5) * 2.0);
        }
        return;
    }

    if (uType == 2) {
        // Wipe
        vec2 dir = vec2(cos(uWipeAngle), sin(uWipeAngle));
        float coord = dot(vUv - 0.5, dir) + 0.5;
        float softness = max(0.001, uWipeSoftness);
        float mask = smoothstep(p - softness, p + softness, coord);

        vec4 fromC = texture(uFromTex, vUv);
        vec4 toC = texture(uToTex, vUv);
        fragColor = mix(toC, fromC, mask);
        return;
    }

    if (uType == 3) {
        // Slide: next clip slides over current clip
        vec2 toUv = vUv - uDirection * (1.0 - p);
        if (toUv.x >= 0.0 && toUv.x <= 1.0 && toUv.y >= 0.0 && toUv.y <= 1.0) {
            fragColor = texture(uToTex, toUv);
        } else {
            fragColor = texture(uFromTex, vUv);
        }
        return;
    }

    if (uType == 4) {
        // Push: previous clip is pushed out by next clip
        vec2 fromUv = vUv + uDirection * p;
        vec2 toUv = vUv - uDirection * (1.0 - p);

        if (fromUv.x >= 0.0 && fromUv.x <= 1.0 && fromUv.y >= 0.0 && fromUv.y <= 1.0) {
            fragColor = texture(uFromTex, fromUv);
        } else if (toUv.x >= 0.0 && toUv.x <= 1.0 && toUv.y >= 0.0 && toUv.y <= 1.0) {
            fragColor = texture(uToTex, toUv);
        } else {
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
        return;
    }

    fragColor = texture(uFromTex, vUv);
}
`;
