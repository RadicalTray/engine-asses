#version 430

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out VS_OUT {
	vec2 TexCoord;
} vsOut;

layout(location = 0) uniform sampler2D picture;
layout(location = 1) uniform vec2 scale;
layout(location = 2) uniform vec2 translate;
layout(location = 3) uniform float opacity;

void main() {
	gl_Position = vec4((aPos * scale) + translate, 0, 1.0f);
	vsOut.TexCoord = aTexCoord;
}
