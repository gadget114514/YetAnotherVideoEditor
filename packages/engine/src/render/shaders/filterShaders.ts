export const filterColorFragShader = `#version 300 es
precision highp float;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uTex;
uniform float uBrightness;  // -1..1 (default 0)
uniform float uContrast;    // 0..2  (default 1)
uniform float uSaturation;  // 0..2  (default 1)
uniform float uGamma;       // 0.1..4 (default 1)
uniform float uMono;        // 0..1 (default 0)
uniform float uSepia;       // 0..1 (default 0)

void main() {
    vec4 c = texture(uTex, vUv);
    vec3 rgb = c.rgb;

    // 1. Brightness
    rgb += uBrightness;

    // 2. Contrast
    rgb = (rgb - 0.5) * uContrast + 0.5;

    // 3. Saturation
    float luma = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    rgb = mix(vec3(luma), rgb, uSaturation);

    // 4. Gamma
    if (uGamma != 1.0 && uGamma > 0.0) {
        rgb = pow(max(rgb, vec3(0.0)), vec3(1.0 / uGamma));
    }

    // 5. Mono
    if (uMono > 0.0) {
        float m = dot(rgb, vec3(0.299, 0.587, 0.114));
        rgb = mix(rgb, vec3(m), uMono);
    }

    // 6. Sepia
    if (uSepia > 0.0) {
        vec3 sepiaTone = vec3(
            dot(rgb, vec3(0.393, 0.769, 0.189)),
            dot(rgb, vec3(0.349, 0.686, 0.168)),
            dot(rgb, vec3(0.272, 0.534, 0.131))
        );
        rgb = mix(rgb, sepiaTone, uSepia);
    }

    fragColor = vec4(clamp(rgb, 0.0, 1.0), c.a);
}
`;

export const filterBlurFragShader = `#version 300 es
precision highp float;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uTex;
uniform vec2 uDirection; // (1.0/w, 0.0) or (0.0, 1.0/h)
uniform float uRadius;

void main() {
    if (uRadius <= 0.0) {
        fragColor = texture(uTex, vUv);
        return;
    }

    vec4 sum = vec4(0.0);
    // 9-tap separable Gaussian weights
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

    sum += texture(uTex, vUv) * weights[0];
    for (int i = 1; i < 5; i++) {
        vec2 offset = uDirection * float(i) * (uRadius / 4.0);
        sum += texture(uTex, vUv + offset) * weights[i];
        sum += texture(uTex, vUv - offset) * weights[i];
    }

    fragColor = sum;
}
`;
