#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <array>
#include <random>

#include <utils.hpp>
#include <model.hpp>
#include <animation.hpp>
#include <animator.hpp>

float getAngle(vec2 u, vec2 v);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void windowSizeCallback(GLFWwindow* window, int width, int height);

struct Mouse {
	double last_xpos;
	double last_ypos;
	double sens;
	float yaw;
	float pitch;
};

struct UniformBuffer {
	mat4 model;
	mat4 model_it;
	mat4 view;
	mat4 projection;
	vec4 view_pos;
	vec4 light_pos;
	vec4 light_clr;
	vec4 ambient_clr;
	float ambient_str;
};

struct View {
	vec3 pos;
	vec3 front;
	vec3 up;
	float fov;
	float speed;
};

struct Keys {
	bool left_click;
	bool right_click;
	bool w;
	bool s;
	bool a;
	bool d;
	bool e;
	bool q;
	bool space;
	bool shift;
	bool tab;
};

enum PlayerState {
	IDLE,
	SWIM,
	WALK,
};

struct State {
	UniformBuffer ub;
	Mouse mouse;
	View view;
	ivec2 scr_res;
	int faces;
	float dt;
	Keys keys;
	PlayerState player_state;

	static State init(GLFWwindow *const window) {
		State state = {};
		state.player_state = IDLE;
		state.faces = 4;
		state.mouse = {
			.sens = 0.1f,
			.yaw = -90.0f,
		};
		state.view = {
			.pos = vec3(0.0f, 0.0f, -1.0f),
			.front = vec3(0.0f, 0.0f, -1.0f),
			.up = vec3(0.0f, 1.0f, 0.0f),
			.fov = 90.0f,
			.speed = 0.032f,
		};
		state.ub = {
			.light_pos = vec4(0.0f, 100.0f, 1.0f, 0.2f),
			.light_clr = vec4(1.0f),
			.ambient_clr = vec4(1.0f),
			.ambient_str = 0.5f,
		};
		glfwGetWindowSize(window, &state.scr_res.x, &state.scr_res.y);
		glfwGetCursorPos(window, &state.mouse.last_xpos, &state.mouse.last_ypos);
		state.updateViewProj(vec3(0.0f));
		state.updateModel(vec3(0.0f), vec3(1.0f), 0);
		return state;
	}

	void uploadUB(uint ubo) const {
		glNamedBufferSubData(ubo, 0, sizeof(UniformBuffer), &this->ub);
	}

	void updateViewProj(vec3 pos) {
		// pinned cam
		vec3 radius = 4.0f * this->view.front;
		pos.y += 2.5f;
		this->ub.view = glm::lookAt(pos - radius, pos, this->view.up);
		// free cam
		// this->ub.view = glm::lookAt(this->view.pos, this->view.pos + this->view.front, this->view.up);

		this->ub.projection = glm::perspective(glm::radians(this->view.fov), (float)this->scr_res.x / (float)this->scr_res.y, 0.1f, 10000.0f);

		this->ub.view_pos = vec4(this->view.pos, 0.0f);
	}

	void uploadModelViewProj(uint ubo) {
		glNamedBufferSubData(ubo, 0, 4*sizeof(mat4) + sizeof(vec4), &this->ub);
	}

	void uploadViewProj(uint ubo) {
		glNamedBufferSubData(ubo, offsetof(UniformBuffer, view), 2*sizeof(mat4) + sizeof(vec4), &this->ub);
	}

	void updateModel(vec3 pos, vec3 scale, float angle, float angle2 = 0.0) {
		const vec3 up = vec3(0.0f, 1.0f, 0.0f);
		const vec3 side = vec3(1.0f, 0.0f, 0.0f);

		mat4 model(1.0f);
		model = glm::translate(model, pos);
		model = glm::scale(model, scale);
		model = glm::rotate(model, angle, up);
		model = glm::rotate(model, angle2, side);

		this->ub.model = model;
		this->ub.model_it = glm::transpose(glm::inverse(model));
	}

	void uploadModel(uint ubo) const {
		glNamedBufferSubData(ubo, offsetof(UniformBuffer, model), 2*sizeof(mat4), &this->ub);
	}
};

struct Entity {
	Model* model;
	Box hitbox = {};
	vec3 scale = vec3(1.0);
	vec3 velocity = vec3(0.0f);
	vec3 pos = vec3(0.0f);
	float angle = 0;
	float lifetime = 0;

	Box getBox(vec3 pos) const {
		return this->hitbox.translate(pos - this->hitbox.max/2.0f);
	}

	bool collide(const vec3& new_pos, const Entity& obj) const {
		Box new_box = this->getBox(new_pos);
		Box obj_box = obj.getBox(obj.pos);

		bool collision = true;
		// Taken from raylib
		if ((new_box.max.x >= obj_box.min.x) && (new_box.min.x <= obj_box.max.x)) {
			if ((new_box.max.y < obj_box.min.y) || (new_box.min.y > obj_box.max.y)) collision = false;
			if ((new_box.max.z < obj_box.min.z) || (new_box.min.z > obj_box.max.z)) collision = false;
		} else {
			collision = false;
		}
		return collision;

	}

	void snap(vec3& new_pos, const Entity& obj) {
		vec3 old_pos = this->pos;
		Box old_box = this->hitbox.translate(old_pos);
		Box obj_box = obj.getBox(obj.pos);

		vec3 diff = new_pos - old_pos;
		if (this->collide(new_pos, obj)) {
			if (diff.y < 0.0f) {
				// if was on top
				if (old_box.min.y >= obj_box.max.y) {
					new_pos.y = obj_box.max.y;
					this->velocity.y = 0;
				}
			}
		}
	}
};

// TODO:
// struct AnimatedEntity {};

const float flr = 0.0f;
const float gravity = 0.0002f;

int main() {
	GLFWwindow *window = init();
	State state = State::init(window);
	glfwSetWindowUserPointer(window, reinterpret_cast<void *>(&state));
	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetWindowSizeCallback(window, windowSizeCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);

	// Initialize buffers
	uint vao;
	uint ubo;
	glCreateVertexArrays(1, &vao);
	glCreateBuffers(1, &ubo);

	// TODO: make this createVAO
	Vertex::setupVAO(vao);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
	glNamedBufferData(ubo, sizeof(UniformBuffer), &state.ub, GL_DYNAMIC_DRAW);

	// Initialize shaders
	const uint model_vert_shader = createShader("./shaders/model.vert", "./shaders/model_vert.frag");
	const uint model_shader = createShader("./shaders/model.vert", "./shaders/model.frag");
	const uint model_anim_shader = createShader("./shaders/model_anim.vert", "./shaders/model.frag");
	const uint model_plain_shader = createShader("./shaders/model.vert", "./shaders/model_plain.frag");
	const uint model_plain_anim_shader = createShader("./shaders/model_anim.vert", "./shaders/model_plain.frag");

	// TODO: move this inside model
	const int model_anim_shader_bone_matrices = glGetUniformLocation(model_plain_anim_shader, "boneMatrices");

	Model model = Model::init("./assets/Dancing Twerk.dae", vao, model_plain_anim_shader);
	auto dance_anim = Animation::init("./assets/Dancing Twerk.dae", model.bone_info_map);
	auto swim_anim = Animation::init("./assets/Swimming.dae", model.bone_info_map);
	auto walk_anim = Animation::init("./assets/Walking.dae", model.bone_info_map);
	auto animator = Animator::init(&dance_anim);
	assert(animator.bone_matrices.size() <= MAX_BONE_MATRICES);

	Model tower_model = Model::init("./assets/fantasy_tower/scene.gltf", vao, model_shader);
	Model map = Model::init("./assets/low_poly_island/scene.gltf", vao, model_shader);
	Model cube = Model::init("./assets/cube.obj", vao, model_plain_shader);
	Model mouse = Model::init("./assets/mouse/mouse.gltf", vao, model_shader);
	Model cat = Model::init("./assets/cat/bleh.gltf", vao, model_shader);
	Model magic_tower_model = Model::init("./assets/wizard_tower/scene.gltf", vao, model_shader);

	std::vector<Entity> enemies;

	Entity player = {
		.model = &model,
		.hitbox = { .min = vec3(0.0f), .max = vec3(0.4f) },
	};

	Entity tower = {
		.model = &tower_model,
		.hitbox = { .min = vec3(0.0f), .max = vec3(0.4f) },
		.scale = vec3(12.0f),
		.pos = vec3(0.0f, 12.0f, 0.0f),
		.angle = 200.0/180.0 * M_PI,
	};

	std::vector<Entity> projectiles;
	std::vector<Entity> player_towers;

	CubeMap cube_map = CubeMap::init();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	// glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	float timer = 0.0f;
	auto start = chrono::steady_clock::now();
	while (!glfwWindowShouldClose(window)) {
		auto now = chrono::steady_clock::now();
		state.dt = chrono::duration_cast<chrono::microseconds>(now - start).count();
		start = now;

		if (timer <= 0.0f) {
			vec3 pos = vec3(dis(gen) * 360.0 - 180.0, 0.0, dis(gen) * 360.0 - 180.0);
			enemies.push_back({ .model = &mouse, .hitbox = { .min = vec3(0.0f), .max = vec3(0.4f) }, .scale = vec3(dis(gen)*15.0 + 1.0), .pos = pos });
			timer = 0.4f;
		} else {
			timer -= state.dt / 1'000'000.0f;
		}

		{ // process
			glfwPollEvents();
			float dt_ms = state.dt/1000.0f;

			vec3 forward = state.view.front;
			forward.y = 0.0f;
			forward = glm::normalize(forward);
			vec3 right = glm::normalize(glm::cross(state.view.front, state.view.up));
			vec3 dir = vec3(0);
			if (state.keys.w) dir += forward;
			if (state.keys.s) dir -= forward;
			if (state.keys.a) dir -= right;
			if (state.keys.d) dir += right;
			if (state.keys.space) player.velocity.y += 2.00f*gravity*dt_ms;

			player.velocity.x = 0;
			player.velocity.z = 0;
                        player.velocity.y -= gravity*dt_ms;

			if (dir != vec3(0.0)) {
				vec3 vel = glm::normalize(dir)*state.view.speed*dt_ms;
				player.velocity.x = vel.x;
				player.velocity.z = vel.z;
			}

			vec3 new_pos = player.pos + player.velocity;

			if (new_pos.y < flr) {
				new_pos.y = flr;
				player.velocity.y = 0.0f;
			}

			for (usize i = 0; i < enemies.size(); i++) {
			}
			player.pos = new_pos;

			switch (state.player_state) {
			case IDLE:
				if (player.velocity.x != 0) {
					if (new_pos.y == flr) {
						state.player_state = WALK;
						animator.playAnimation(&walk_anim);
					} else {
						state.player_state = SWIM;
						animator.playAnimation(&walk_anim);
					}
				}
				break;
			case SWIM:
				if (player.velocity.x == 0) {
					state.player_state = IDLE;
					animator.playAnimation(&walk_anim);
				} else if (player.pos.y == flr) {
					state.player_state = WALK;
					animator.playAnimation(&walk_anim);
				}
				break;
			case WALK:
				if (player.velocity.x == 0) {
					state.player_state = IDLE;
					animator.playAnimation(&walk_anim);
				} else if (player.pos.y > flr) {
					state.player_state = SWIM;
					animator.playAnimation(&swim_anim);
				}
				break;
			}
			animator.updateAnimation(state.dt/1000000.0f);

			if (state.keys.left_click) {
				float spd = 0.1f;
				vec3 bullet_pos = player.pos + vec3(0.0, 2.0, 0.0);
				vec3 velocity = state.view.front * spd * dt_ms;
				if (enemies.size() > 0) {
					float dist2 = std::numeric_limits<float>::max();
					vec3 line = vec3(0);
					for (const auto& e : enemies) {
						vec3 u = e.pos - bullet_pos;
						float u_dist2 = glm::dot(u, u);
						if (u_dist2 < dist2) {
							line = u;
							dist2 = u_dist2;
						}
					}
					velocity = glm::normalize(line) * spd * dt_ms;
				}
				projectiles.push_back({
					.model = &cube,
					.scale = vec3(0.2),
					.velocity = velocity,
					.pos = bullet_pos,
					.lifetime = 4.0,
				});
				state.keys.left_click = false;
			}

			if (state.keys.right_click) {
				vec3 pos = player.pos;
				pos.y = 0;
				player_towers.push_back({
					.model = &magic_tower_model,
					.pos = pos,
				});
				state.keys.right_click = false;
			}

			{
				for (const auto& t : player_towers) {
					float spd = 0.1f;
					if (enemies.size() > 0) {
						vec3 bullet_pos = t.pos + vec3(0.0, 10.0, 0.0);
						vec3 velocity = vec3(1.0, 0.0, 0.0);
						float dist2 = std::numeric_limits<float>::max();
						vec3 line = vec3(0);
						for (const auto& e : enemies) {
							vec3 u = e.pos - bullet_pos;
							float u_dist2 = glm::dot(u, u);
							if (u_dist2 < dist2) {
								line = u;
								dist2 = u_dist2;
							}
						}
						velocity = glm::normalize(line) * spd * dt_ms;
						projectiles.push_back({
							.model = &cube,
							.scale = vec3(0.2),
							.velocity = velocity,
							.pos = bullet_pos,
							.lifetime = 1.0,
						});
					}
				}
			}

			for (int i = projectiles.size() - 1; i >= 0; i--) {
				// TODO: check collision
				auto& p = projectiles[i];
				p.pos += p.velocity;
				if (p.lifetime <= 0) {
					// swap remove is faster but
					// c++ stl fucking sucks and doesn't have the api for it
					projectiles.erase(projectiles.begin() + i);
				} else {
					p.lifetime -= dt_ms/1000;
				}
			}

			for (int i = enemies.size() - 1; i >= 0; i--) {
				auto& e = enemies[i];
				auto line = tower.pos - e.pos;
				line.y = 0.0;
				auto dir = glm::normalize(line);
				e.angle = getAngle(vec2(1.0, 0.0), vec2(dir.x, dir.z));
				e.pos += dir * 0.004f * dt_ms;

				float dist2 = glm::dot(line, line);
				float radius = 4.0f;
				if (dist2 < radius*radius) {
					// swap remove is faster but
					// c++ stl fucking sucks and doesn't have the api for it
					enemies.erase(enemies.begin() + i);
				}
			}
		}
		{ // render
			glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// render player model
			const auto& transforms = animator.bone_matrices;
			glProgramUniformMatrix4fv(model_plain_anim_shader, model_anim_shader_bone_matrices, transforms.size(), false, glm::value_ptr(transforms[0]));

			state.updateModel(player.pos, player.scale, getAngle(vec2(1.0f, 0.0f), vec2(state.view.front.x, state.view.front.z)));
			state.updateViewProj(player.pos);
			state.uploadModelViewProj(ubo);
			// player.model->draw();

			for (const auto& p : projectiles) {
				state.updateModel(p.pos, p.scale, p.angle);
				state.uploadModel(ubo);
				p.model->draw();
			}

			for (const auto& e : enemies) {
				state.updateModel(e.pos, e.scale, e.angle);
				state.uploadModel(ubo);
				e.model->draw();
			}

			for (const auto& e : player_towers) {
				state.updateModel(e.pos, e.scale, e.angle, -M_PI/2);
				state.uploadModel(ubo);
				e.model->draw();
			}

			state.updateModel(tower.pos, tower.scale, tower.angle);
			state.uploadModel(ubo);
			tower.model->draw();

			state.updateModel(vec3(0.0f, -2.0f, 0.0f), vec3(32.0f, 1.0f, 32.0f), 0);
			state.uploadModel(ubo);
			map.draw();

			// render cube map
			mat4 view = glm::lookAt(vec3(0.0), state.view.front, state.view.up);
			glNamedBufferSubData(ubo, offsetof(UniformBuffer, view), sizeof(mat4), glm::value_ptr(view));
			cube_map.draw();
		}
		glfwSwapBuffers(window);
	}

	// TODO: free stuff
	// glDeleteVertexArrays(, );
	// glDeleteBuffers(, );
	// glDeleteProgram(model_plain_shader);
	// glDeleteProgram(model_plain_anim_shader);

	deinit(&window);
	return 0;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	State *state = reinterpret_cast<State*>(glfwGetWindowUserPointer(window));
	switch (key) {
	case GLFW_KEY_W:
		switch (action) {
		case GLFW_PRESS:
			state->keys.w = true;
			break;
		case GLFW_RELEASE:
			state->keys.w = false;
			break;
		}
		break;
	case GLFW_KEY_S:
		switch (action) {
		case GLFW_PRESS:
			state->keys.s = true;
			break;
		case GLFW_RELEASE:
			state->keys.s = false;
			break;
		}
		break;
	case GLFW_KEY_A:
		switch (action) {
		case GLFW_PRESS:
			state->keys.a = true;
			break;
		case GLFW_RELEASE:
			state->keys.a = false;
			break;
		}
		break;
	case GLFW_KEY_D:
		switch (action) {
		case GLFW_PRESS:
			state->keys.d = true;
			break;
		case GLFW_RELEASE:
			state->keys.d = false;
			break;
		}
		break;
	case GLFW_KEY_E:
		switch (action) {
		case GLFW_PRESS:
			state->keys.e = true;
			break;
		case GLFW_RELEASE:
			state->keys.e = false;
			break;
		}
		break;
	case GLFW_KEY_Q:
		switch (action) {
		case GLFW_PRESS:
			state->keys.q = true;
			break;
		case GLFW_RELEASE:
			state->keys.q = false;
			break;
		}
		break;
	case GLFW_KEY_SPACE:
		switch (action) {
		case GLFW_PRESS:
			state->keys.space = true;
			break;
		case GLFW_RELEASE:
			state->keys.space = false;
			break;
		}
		break;
	case GLFW_KEY_LEFT_SHIFT:
		switch (action) {
		case GLFW_PRESS:
			state->keys.shift = true;
			break;
		case GLFW_RELEASE:
			state->keys.shift = false;
			break;
		}
		break;
	case GLFW_KEY_TAB:
		switch (action) {
		case GLFW_PRESS:
			state->keys.tab = true;
			break;
		case GLFW_RELEASE:
			state->keys.tab = false;
			break;
		}
		break;
	}
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	State *state = reinterpret_cast<State*>(glfwGetWindowUserPointer(window));
	switch (button) {
	case GLFW_MOUSE_BUTTON_LEFT:
		switch (action) {
		case GLFW_PRESS:
			state->keys.left_click = true;
			break;
		case GLFW_RELEASE:
			state->keys.left_click = false;
			break;
		}
		break;
	case GLFW_MOUSE_BUTTON_RIGHT:
		switch (action) {
		case GLFW_PRESS:
			state->keys.right_click = true;
			break;
		case GLFW_RELEASE:
			state->keys.right_click = false;
			break;
		}
		break;
	}
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	State *state = reinterpret_cast<State*>(glfwGetWindowUserPointer(window));
	double xoffset =  (xpos - state->mouse.last_xpos) * state->mouse.sens;
	double yoffset = -(ypos - state->mouse.last_ypos) * state->mouse.sens;
	state->mouse.last_xpos = xpos;
	state->mouse.last_ypos = ypos;

	state->mouse.yaw = state->mouse.yaw + xoffset;
	state->mouse.pitch = std::clamp(state->mouse.pitch + yoffset, -89.0d, 89.0d);
	state->view.front = glm::normalize(vec3(
		std::cos(glm::radians(state->mouse.yaw)) * std::cos(glm::radians(state->mouse.pitch)),
		std::sin(glm::radians(state->mouse.pitch)),
		std::sin(glm::radians(state->mouse.yaw)) * std::cos(glm::radians(state->mouse.pitch))
	));
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	// State *state = reinterpret_cast<State*>(glfwGetWindowUserPointer(window));
}

void windowSizeCallback(GLFWwindow* window, int width, int height) {
	State *state = reinterpret_cast<State*>(glfwGetWindowUserPointer(window));
	state->scr_res = ivec2(width, height);
}

float getAngle(vec2 u, vec2 v) {
	return std::atan2(u.x*v.x + u.y*v.y, u.x*v.y - u.y*v.x);
}
