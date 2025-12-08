#version 430

in VS_OUT {
	vec2 TexCoord;
} vsOut;

layout(location = 0) out vec4 FragColor;

layout(location = 0) uniform sampler2D picture;
layout(location = 1) uniform vec2 scale;
layout(location = 2) uniform vec2 translate;
layout(location = 3) uniform float opacity;
layout(location = 4) uniform vec4 u_color;

void main() {
	vec4 color = mix(u_color, texture(picture, vsOut.TexCoord), opacity);
	FragColor = color;
}
