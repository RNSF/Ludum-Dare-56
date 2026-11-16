#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform float cutoff;
uniform vec4 col1;
uniform vec4 col2;

// Output fragment color
out vec4 finalColor;


void main() {
    vec4 texColor = texture(texture0, fragTexCoord)*fragColor;

	if (fragTexCoord.y < cutoff) {
		finalColor = col1 * vec4(1.0, 1.0, 1.0, texColor.a);
	} else {
		finalColor = col2 * vec4(1.0, 1.0, 1.0, texColor.a);
	}
	
}