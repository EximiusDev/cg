#version 330 core

in vec3 fragNormal;
in vec3 fragPosition;
in vec2 fragTexCoords;
in vec4 lightVSPosition;

// propiedades del material
uniform sampler2D colorTexture; // unused for the color-encoding pass but kept for compatibility
// we expect the C++ side to set this uniform with the drawing shader if needed

// tamaño de la textura en pixels (se pasa desde C++)
uniform vec2 texSize;

out vec4 fragColor;

// pack a 24-bit integer into RGB channels (8 bits each)
vec3 pack24(int value) {
	int r = (value >> 16) & 255;
	int g = (value >> 8) & 255;
	int b = value & 255;
	return vec3(r, g, b) / 255.0;
}

void main() {
	// compute integer pixel coordinates from texture coords
	ivec2 size = ivec2(max(texSize, vec2(1.0)));
	ivec2 p = ivec2(clamp(ivec2(floor(fragTexCoords * vec2(size)) + 0), ivec2(0), size - ivec2(1)));

	int idx = p.y * size.x + p.x;

	vec3 true_buff24 = pack24(idx);
	fragColor = vec4(true_buff24, 1.0);
}
