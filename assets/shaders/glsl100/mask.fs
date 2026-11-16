#version 100

// WebGL / OpenGL ES 2.0 version of glsl330/mask.fs
precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform float cutoff;
uniform vec4 col1;
uniform vec4 col2;

void main() {
    vec4 texColor = texture2D(texture0, fragTexCoord)*fragColor;

    if (fragTexCoord.y < cutoff) {
        gl_FragColor = col1 * vec4(1.0, 1.0, 1.0, texColor.a);
    } else {
        gl_FragColor = col2 * vec4(1.0, 1.0, 1.0, texColor.a);
    }
}
