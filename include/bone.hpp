#pragma once

/* Container for bone data */

#include <vector>

#include <assimp/scene.h>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <utils.hpp>

struct KeyPosition {
	glm::vec3 pos;
	float timestamp;
};

struct KeyRotation {
	glm::quat rot;
	float timestamp;
};

struct KeyScale {
	glm::vec3 scale;
	float timestamp;
};

struct Bone {
	std::vector<KeyPosition> positions;
	std::vector<KeyRotation> rotations;
	std::vector<KeyScale> scales;
	glm::mat4 local_transform;
	std::string name;
	int id;

	static Bone init(const std::string& name, int id, const aiNodeAnim* channel) {
		std::vector<KeyPosition> positions;
		positions.reserve(channel->mNumRotationKeys);
		for (uint i = 0; i < channel->mNumRotationKeys; i++) {
			positions.push_back({
				.pos = glmFromAssimpVec3(channel->mPositionKeys[i].mValue),
				.timestamp = (float)channel->mPositionKeys[i].mTime,
			});
		}

		std::vector<KeyRotation> rotations;
		rotations.reserve(channel->mNumRotationKeys);
		for (uint i = 0; i < channel->mNumRotationKeys; i++) {
			rotations.push_back({
				.rot = glmFromAssimpQuat(channel->mRotationKeys[i].mValue),
				.timestamp = (float)channel->mRotationKeys[i].mTime,
			});
		}

		std::vector<KeyScale> scales;
		scales.reserve(channel->mNumScalingKeys);
		for (uint i = 0; i < channel->mNumScalingKeys; i++) {
			scales.push_back({
				.scale = glmFromAssimpVec3(channel->mScalingKeys[i].mValue),
				.timestamp = (float)channel->mScalingKeys[i].mTime,
			});
		}

		return {
			.positions = positions,
			.rotations = rotations,
			.scales = scales,
			.local_transform = mat4(1.0f),
			.name = name,
			.id = id,
		};
	}
	
	void update(float t) {
		this->local_transform = interpolatePos(t) * interpolateRot(t) * interpolateScale(t);
	}

	glm::mat4 interpolatePos(float t) const {
		if (this->positions.size() == 1) {
			return glm::translate(glm::mat4(1.0f), this->positions[0].pos);
		}

		usize idx = this->getPosIdx(t);
		usize next_idx = idx + 1;
		float factor = getFactor(this->positions[idx].timestamp, this->positions[next_idx].timestamp, t);
		glm::vec3 final_pos = glm::mix(this->positions[idx].pos, this->positions[next_idx].pos, factor);
		return glm::translate(glm::mat4(1.0f), final_pos);
	}

	glm::mat4 interpolateRot(float t) const {
		if (this->rotations.size() == 1) {
			return glm::toMat4(glm::normalize(this->rotations[0].rot));
		}

		usize idx = this->getRotIdx(t);
		usize next_idx = idx + 1;
		float factor = getFactor(this->rotations[idx].timestamp, this->rotations[next_idx].timestamp, t);
		glm::quat final_rot = glm::normalize(glm::slerp(this->rotations[idx].rot, this->rotations[next_idx].rot, factor));
		return glm::toMat4(final_rot);

	}

	glm::mat4 interpolateScale(float t) const {
		if (this->scales.size() == 1) {
			return glm::scale(glm::mat4(1.0f), this->scales[0].scale);
		}

		usize idx = this->getScaleIdx(t);
		usize next_idx = idx + 1;
		float factor = getFactor(this->scales[idx].timestamp, this->scales[next_idx].timestamp, t);
		glm::vec3 final_scale = glm::mix(this->scales[idx].scale, this->scales[next_idx].scale, factor);
		return glm::scale(glm::mat4(1.0f), final_scale);
	}

	usize getPosIdx(float t) const {
		for (usize i = 0; i < this->positions.size() - 1; i++) {
			if (t < this->positions[i + 1].timestamp) {
				return i;
			}
		}
		assert(0);
	}

	int getRotIdx(float t) const {
		for (usize i = 0; i < this->rotations.size() - 1; i++) {
			if (t < this->rotations[i + 1].timestamp) {
				return i;
			}
		}
		assert(0);
	}

	int getScaleIdx(float t) const {
		for (usize i = 0; i < this->scales.size() - 1; i++) {
			if (t < this->scales[i + 1].timestamp) {
				return i;
			}
		}
		assert(0);
	}
};
