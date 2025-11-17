#pragma once
#include <string>
#include <vector>

#include <glad/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/scene.h>

#include <utils.hpp>

struct Texture {
	// from glCreateTextures()
	uint id;

	aiTextureType type;
};

struct TextureInfo {
	Texture texture;
	std::string path;
};

void loadMaterialTextures(
	std::vector<Texture>& textures,
	std::vector<TextureInfo>& textures_loaded,
	aiMaterial *mat,
	aiTextureType type,
	const std::string& directory
);

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint> indices;
	std::vector<Texture> textures;
	uint vbo;
	uint ebo;

	static Mesh init(aiMesh *mesh, const aiScene *scene, std::vector<TextureInfo>& textures_loaded, std::map<std::string, BoneInfo>& bone_info_map, const std::string& directory) {
		std::vector<Vertex> vertices;
		vertices.reserve(mesh->mNumVertices);
		for (uint i = 0; i < mesh->mNumVertices; i++) {
			Vertex vertex = {};
			vertex.bone_ids.fill(-1);
			vertex.weights.fill(0);

			vertex.pos = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

			if (mesh->HasVertexColors(0)) {
				vertex.clr = glmFromAssimpVec4(mesh->mColors[0][i]);
			}

			if (mesh->HasNormals()) {
				vertex.norm = glmFromAssimpVec3(mesh->mNormals[i]);
			}

			if (mesh->HasTextureCoords(0)) {
				vertex.tex = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
			}

			if (mesh->HasTangentsAndBitangents()) {
				vertex.tan = glmFromAssimpVec3(mesh->mTangents[i]);
				vertex.bitan = glmFromAssimpVec3(mesh->mBitangents[i]);
			}

			vertices.push_back(vertex);
		}

		std::vector<uint> indices;
		indices.reserve(mesh->mNumFaces * 3);
		for (uint i = 0; i < mesh->mNumFaces; i++) {
			aiFace face = mesh->mFaces[i];
			for (uint j = 0; j < face.mNumIndices; j++) {
				indices.push_back(face.mIndices[j]);
			}
		}

		std::vector<Texture> textures;
		textures.reserve(4);
		aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
		loadMaterialTextures(textures, textures_loaded, material, aiTextureType_DIFFUSE, directory);
		loadMaterialTextures(textures, textures_loaded, material, aiTextureType_SPECULAR, directory);
		loadMaterialTextures(textures, textures_loaded, material, aiTextureType_NORMALS, directory);
		loadMaterialTextures(textures, textures_loaded, material, aiTextureType_HEIGHT, directory);

		for (uint bone_index = 0; bone_index < mesh->mNumBones; bone_index++) {
			// bone_id is guaranteed to be initialized. if only c++ had a return block ._.
			int bone_id;
			std::string bone_name = mesh->mBones[bone_index]->mName.C_Str();
			if (bone_info_map.contains(bone_name)) {
				bone_id = bone_info_map[bone_name].id;
			} else {
				bone_id = bone_info_map.size();
				bone_info_map[bone_name] = {
					.id = bone_id,
					.offset = glmFromAssimpMat4(mesh->mBones[bone_index]->mOffsetMatrix),
				};
			}

			auto weights = mesh->mBones[bone_index]->mWeights;
			for (uint weight_index = 0; weight_index < mesh->mBones[bone_index]->mNumWeights; weight_index++) {
				uint vertex_id = weights[weight_index].mVertexId;
				assert(vertex_id <= vertices.size());
				for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
					if (vertices[vertex_id].bone_ids[i] < 0) {
						assert(bone_id < MAX_BONE_MATRICES);
						vertices[vertex_id].bone_ids[i] = bone_id;
						vertices[vertex_id].weights[i] = weights[weight_index].mWeight;
						break;
					}
				}
			}
		}

		uint b[2];
		glCreateBuffers(2, b);
		uint vbo = b[0], ebo = b[1];
		glNamedBufferData(vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
		glNamedBufferData(ebo, indices.size() * sizeof(uint), indices.data(), GL_STATIC_DRAW);

		return {
			.vertices = vertices,
			.indices = indices,
			.textures = textures,
			.vbo = vbo,
			.ebo = ebo,
		};
	}

	// TODO:
	//  - test
	//  - handle multiple textures
	//
	// NOTE: assuming we're using 1 VAO for the whole program
	//
	// BUG: BREAKS IF THERE MUITIPLE TEXTURES OF THE SAME TYPE (unimplemented lol)
	void draw(uint vao, uint shader) const {
		glUseProgram(shader);
		glBindVertexArray(vao);

		// THUNK: does this really need to be an array? can't a struct suffice?
		for (usize i = 0; i < this->textures.size(); i++) {
			int loc = 0;
			switch (this->textures[i].type) {
			case aiTextureType_DIFFUSE:
				loc = 0;
				break;
			case aiTextureType_SPECULAR:
				loc = 1;
				break;
			case aiTextureType_NORMALS:
				loc = 2;
				break;
			case aiTextureType_HEIGHT:
				loc = 3;
				break;
			default:
				loc = 0;
				break;
			}
			glBindTextureUnit(i, this->textures[i].id);
			glProgramUniform1i(shader, loc, i);
		}

		glVertexArrayVertexBuffer(vao, 0, this->vbo, 0, sizeof(Vertex));
		glVertexArrayElementBuffer(vao, this->ebo);
		glDrawElements(GL_TRIANGLES, static_cast<uint>(indices.size()), GL_UNSIGNED_INT, 0);
	}
};

void loadMaterialTextures(
	std::vector<Texture>& textures,
	std::vector<TextureInfo>& textures_loaded,
	aiMaterial *mat,
	aiTextureType type,
	const std::string& directory
) {
	for (uint i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);
		bool loaded = false;
		for (uint j = 0; j < textures_loaded.size(); j++) {
			if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
				textures.push_back(textures_loaded[j].texture);
				loaded = true;
				break;
			}
		}
		if (!loaded) {
			TextureInfo texture_info = {
				.texture = {
					.id = texture2DFromFile((directory + '/' + str.C_Str()).c_str(), 1),
					.type = type,
				},
				.path = str.C_Str(),
			};
			textures.push_back(texture_info.texture);
			textures_loaded.push_back(texture_info);
		}
	}
}
