#pragma once

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb/stb_image.h>

#include <fstream>
#include <sstream>
#include <string>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <assimp/vector2.h>
#include <assimp/vector3.h>
#include <assimp/color4.h>

#include <types.hpp>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void GLAPIENTRY debugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
std::string readFile(const char *const filepath);

GLFWwindow* init() {
	glfwInit();

	glfwWindowHint(GLFW_SAMPLES, 4);
	GLFWwindow* window = glfwCreateWindow(1600, 900, "uwu", NULL, NULL);
	if (window == NULL) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		exit(-1);
	}
	std::cerr << "GL " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << std::endl;

	// first window doesn't trigger framebufferSizeCallback
	// or has the wrong viewport for some reason
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	framebufferSizeCallback(window, width, height);

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(debugMessageCallback, 0);

	return window;
}

void deinit(GLFWwindow** window) {
	glfwTerminate();
	*window = nullptr;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

void GLAPIENTRY
debugMessageCallback(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam
) {
	// WARN: assuming message is always NUL terminated. (can be checked with `length`)
	std::string source_str, type_str, severity_str;
	switch (source) {
	case GL_DEBUG_SOURCE_API:             source_str = "api"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_str = "window system"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: source_str = "shader compiler"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     source_str = "third party"; break;
	case GL_DEBUG_SOURCE_APPLICATION:     source_str = "application"; break;
	case GL_DEBUG_SOURCE_OTHER:           source_str = "other"; break;
	}
	switch (type) {
	case GL_DEBUG_TYPE_ERROR:               type_str = "error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "deprecated"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "undefined"; break; 
	case GL_DEBUG_TYPE_PORTABILITY:         type_str = "portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "performance"; break;
	case GL_DEBUG_TYPE_MARKER:              type_str = "marker"; break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          type_str = "push group"; break;
	case GL_DEBUG_TYPE_POP_GROUP:           type_str = "pop group"; break;
	case GL_DEBUG_TYPE_OTHER:               type_str = "other"; break;
	}
	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:         severity_str = "high"; break;
	case GL_DEBUG_SEVERITY_MEDIUM:       severity_str = "medium"; break;
	case GL_DEBUG_SEVERITY_LOW:          severity_str = "low"; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: severity_str = "notification"; break;
	}
	std::cerr << "gl(" << type_str << "): " << message << std::endl;
}

uint texture2DFromFile(const char* filepath, int levels, int preferred_channels = 0) {
	stbi_set_flip_vertically_on_load(true); // opengl/glfw dum dum

	uint tex;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);

	int width, height, n_channels;
	uchar *data = stbi_load(filepath, &width, &height, &n_channels, preferred_channels);
	if (preferred_channels != 0) { n_channels = preferred_channels; }
	if (data) {
		GLenum internalformat = GL_R8, format = GL_RED;
		switch (n_channels) {
		case 1:
			internalformat = GL_R8;
			format = GL_RED;
			break;
		case 2:
			internalformat = GL_RG8;
			format = GL_RG;
			break;
		case 3:
			internalformat = GL_RGB8;
			format = GL_RGB;
			break;
		case 4:
			internalformat = GL_RGBA8;
			format = GL_RGBA;
			break;
		default:
			std::cerr << "error: " << filepath << "wtf is this format?" << std::endl;
			break;
		}
		glTextureStorage2D(tex, levels, internalformat, width, height);
		glTextureSubImage2D(tex, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glGenerateTextureMipmap(tex);
	} else {
		std::cerr << "stbi(error): " << stbi_failure_reason() << " (" << filepath << ")" << std::endl;
	}
	stbi_image_free(data);

	return tex;
}

uint createShader(const char *const vert_filename, const char *const frag_filename) {
	const std::string vert_src = readFile(vert_filename), frag_src = readFile(frag_filename);
	const char *vert_src_c = vert_src.data(), *frag_src_c = frag_src.data();

	bool quit = false;
	int result;
	int err_msg_len = 0;
	std::string err_msg;

	uint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vert_src_c, nullptr);
	glCompileShader(vertex_shader);

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result);
	glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &err_msg_len);
	if (!result){
		quit = true;
		err_msg.resize(err_msg_len);
		glGetShaderInfoLog(vertex_shader, err_msg.size(), nullptr, err_msg.data());
		std::cerr << "[vertex shader compilation error]" << std::endl
			  << err_msg
			  << "---------------------------------" << std::endl;
	}

	uint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &frag_src_c, nullptr);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result);
	glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &err_msg_len);
	if (!result){
		quit = true;
		err_msg.resize(err_msg_len);
		glGetShaderInfoLog(fragment_shader, err_msg.size(), nullptr, err_msg.data());
		std::cerr << "[fragment shader compilation error]" << std::endl
			  << err_msg
			  << "-----------------------------------" << std::endl;
	}

	uint shader = glCreateProgram();
	glAttachShader(shader, vertex_shader);
	glAttachShader(shader, fragment_shader);
	glLinkProgram(shader);

	glGetProgramiv(shader, GL_LINK_STATUS, &result);
	glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &err_msg_len);
	if (!result) {
		quit = true;
		err_msg.resize(err_msg_len);
		glGetProgramInfoLog(shader, err_msg.size(), nullptr, err_msg.data());
		std::cerr << "[shader program linking error]" << std::endl
			  << err_msg
			  << "------------------------------" << std::endl;
	}

	if (quit) exit(1);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return shader;
}

std::string readFile(const char *const filepath) {
	std::cerr << "debug: reading file " << filepath << std::endl;
	std::ifstream file;
	file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	file.open(filepath);
	std::stringstream stream;
	stream << file.rdbuf();
	file.close();
	return stream.str();
}

glm::mat4 glmFromAssimpMat4(const aiMatrix4x4& src) {
	glm::mat4 m;
	m[0][0] = src.a1; m[1][0] = src.a2; m[2][0] = src.a3; m[3][0] = src.a4;
	m[0][1] = src.b1; m[1][1] = src.b2; m[2][1] = src.b3; m[3][1] = src.b4;
	m[0][2] = src.c1; m[1][2] = src.c2; m[2][2] = src.c3; m[3][2] = src.c4;
	m[0][3] = src.d1; m[1][3] = src.d2; m[2][3] = src.d3; m[3][3] = src.d4;
	return m;
}

glm::vec2 glmFromAssimpVec2(const aiVector2D& src) {
	return glm::vec2(src.x, src.y);
}

glm::vec3 glmFromAssimpVec3(const aiVector3D& src) {
	return glm::vec3(src.x, src.y, src.z);
}

glm::vec4 glmFromAssimpVec4(const aiColor4D& src) {
	return glm::vec4(src.r, src.g, src.b, src.a);
}

glm::quat glmFromAssimpQuat(const aiQuaternion& src) {
	return glm::quat(src.w, src.x, src.y, src.z);
}

struct CubeMap {
	std::vector<vec3> vertices;
	std::vector<uint> indices;
	uint vao;
	uint vbo;
	uint ebo;
	uint shader;
	uint tex;

	static CubeMap init() {
		CubeMap cube_map = {};

		glCreateVertexArrays(1, &cube_map.vao);
		glEnableVertexArrayAttrib(cube_map.vao,  0);
		glVertexArrayAttribFormat(cube_map.vao,  0, 3, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribBinding(cube_map.vao, 0, 0);

		cube_map.shader = createShader("./shaders/cube_map.vert", "./shaders/cube_map.frag");
		glProgramUniform1i(cube_map.shader, 0, 0);

		const char *paths[6] = {
			"assets/envmap_miramar/miramar_ft.tga",
			"assets/envmap_miramar/miramar_bk.tga",
			"assets/envmap_miramar/miramar_up.tga",
			"assets/envmap_miramar/miramar_dn.tga",
			"assets/envmap_miramar/miramar_rt.tga",
			"assets/envmap_miramar/miramar_lf.tga",
		};

		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cube_map.tex);
		glTextureParameteri(cube_map.tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(cube_map.tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(cube_map.tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cube_map.tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cube_map.tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		bool texture_allocated = false;

		stbi_set_flip_vertically_on_load(false);
		for (int i = 0; i < 6; i++) {
			int width, height, n_channels;
			uchar *data = stbi_load(paths[i], &width, &height, &n_channels, 3);
			if (data) {
				if (!texture_allocated) {
					glTextureStorage2D(cube_map.tex, 1, GL_RGB8, width, height);
					texture_allocated = true;
				}
				glTextureSubImage3D(cube_map.tex, 0, 0, 0, i, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
			} else {
				std::cerr << "stbi(error): " << stbi_failure_reason() << " (" << paths[i] << ")" << std::endl;
			}
			stbi_image_free(data);
		}

		uint flags = 0;
		flags |= aiProcess_Triangulate;

		const char *path = "assets/cube.obj";
		std::cerr << "assimp(info): loading " << path << std::endl;
		Assimp::Importer imp;
		const aiScene *scene = imp.ReadFile(path, flags);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::cerr << "assimp(error): " << imp.GetErrorString() << std::endl;
			return {}; // TODO: handle error
		}
		cube_map.processNode(scene->mRootNode, scene);

		glCreateBuffers(2, &cube_map.vbo);
		glNamedBufferData(cube_map.vbo, cube_map.vertices.size() * sizeof(vec3), cube_map.vertices.data(), GL_STATIC_DRAW);
		glNamedBufferData(cube_map.ebo, cube_map.indices.size() * sizeof(uint), cube_map.indices.data(), GL_STATIC_DRAW);

		return cube_map;
	}

	void processNode(aiNode* node, const aiScene* scene) {
		for (uint i = 0; i < node->mNumMeshes; i++) {
			aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

			this->vertices.reserve(this->vertices.size() + mesh->mNumVertices);
			for (uint i = 0; i < mesh->mNumVertices; i++) {
				this->vertices.push_back(vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z));
			}

			this->indices.reserve(this->indices.size() + mesh->mNumFaces * 3);
			for (uint i = 0; i < mesh->mNumFaces; i++) {
				aiFace face = mesh->mFaces[i];
				for (uint j = 0; j < face.mNumIndices; j++) {
					this->indices.push_back(face.mIndices[j]);
				}
			}
		}
		for (uint i = 0; i < node->mNumChildren; i++) {
			processNode(node->mChildren[i], scene);
		}
	}

	void draw() const {
		int old_cull_face_mode;
		glGetIntegerv(GL_CULL_FACE_MODE, &old_cull_face_mode);
		int old_depth_func;
		glGetIntegerv(GL_DEPTH_FUNC, &old_depth_func);

		glCullFace(GL_FRONT);
		glDepthFunc(GL_LEQUAL);

		glUseProgram(this->shader);
		glBindVertexArray(this->vao);

		// uniform has already been set to texture 0 in init()
		glBindTextureUnit(0, this->tex);

		glVertexArrayVertexBuffer(this->vao, 0, this->vbo, 0, sizeof(vec3));
		glVertexArrayElementBuffer(this->vao, this->ebo);
		glDrawElements(GL_TRIANGLES, static_cast<uint>(this->indices.size()), GL_UNSIGNED_INT, 0);

		glCullFace(old_cull_face_mode);
		glDepthFunc(old_depth_func);
	}
};

float getFactor(float last, float next, float x) {
	return (x - last) / (next - last);
}
