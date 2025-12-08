#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

#define MAX_BONE_INFLUENCE 4
#define MAX_BONE_MATRICES 128

using glm::mat4, glm::vec2, glm::vec3, glm::vec4, glm::uvec2, glm::ivec2;
namespace chrono = std::chrono;

typedef unsigned char uchar;
typedef uint32_t uint;
typedef size_t usize;

struct BoneInfo {
	// in Animator.bone_matrices
	int id;

	glm::mat4 offset;
};

struct Vertex {
	vec3 pos;
	vec3 norm;
	vec2 tex;
	vec3 tan;
	vec3 bitan;
	std::array<int, MAX_BONE_INFLUENCE> bone_ids;
	std::array<float, MAX_BONE_INFLUENCE> weights;
	vec4 clr;

	static void setupVAO(uint vao) {
		glEnableVertexArrayAttrib(vao,  0);
		glVertexArrayAttribFormat(vao,  0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));
		glVertexArrayAttribBinding(vao, 0, 0);

		glEnableVertexArrayAttrib(vao,  1);
		glVertexArrayAttribFormat(vao,  1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, norm));
		glVertexArrayAttribBinding(vao, 1, 0);

		glEnableVertexArrayAttrib(vao,  2);
		glVertexArrayAttribFormat(vao,  2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, tex));
		glVertexArrayAttribBinding(vao, 2, 0);

		glEnableVertexArrayAttrib(vao,  3);
		glVertexArrayAttribFormat(vao,  3, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tan));
		glVertexArrayAttribBinding(vao, 3, 0);

		glEnableVertexArrayAttrib(vao,  4);
		glVertexArrayAttribFormat(vao,  4, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, bitan));
		glVertexArrayAttribBinding(vao, 4, 0);

		glEnableVertexArrayAttrib(vao,  5);
		glVertexArrayAttribIFormat(vao,  5, MAX_BONE_INFLUENCE, GL_INT, offsetof(Vertex, bone_ids));
		glVertexArrayAttribBinding(vao, 5, 0);

		glEnableVertexArrayAttrib(vao,  6);
		glVertexArrayAttribFormat(vao,  6, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, offsetof(Vertex, weights));
		glVertexArrayAttribBinding(vao, 6, 0);

		glEnableVertexArrayAttrib(vao,  7);
		glVertexArrayAttribFormat(vao,  7, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, clr));
		glVertexArrayAttribBinding(vao, 7, 0);
	}
};

struct UIVertex {
	vec2 pos;
	vec2 tex;

	static void setupVAO(uint vao) {
		glEnableVertexArrayAttrib(vao,  0);
		glVertexArrayAttribFormat(vao,  0, 2, GL_FLOAT, GL_FALSE, offsetof(UIVertex, pos));
		glVertexArrayAttribBinding(vao, 0, 0);

		glEnableVertexArrayAttrib(vao,  1);
		glVertexArrayAttribFormat(vao,  1, 2, GL_FLOAT, GL_FALSE, offsetof(UIVertex, tex));
		glVertexArrayAttribBinding(vao, 1, 0);
	}
};
