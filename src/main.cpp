// TODO:
//  water
//  shadow mapping
//  circle hitbox around player
//
//  UI:
//   - clickable towers and powers on the bottom of the screen
//   - base hp
//   - money

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
void topDownCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void windowSizeCallback(GLFWwindow* window, int width, int height);

struct Mouse {
	double scr_x;
	double scr_y;
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

struct State {
	UniformBuffer ub;
	Mouse mouse;
	View view;
	ivec2 scr_res;
	float dt;
	Keys keys;

	static State init(GLFWwindow *const window) {
		State state = {};
		state.mouse = {
			.sens = 0.1f,
			.yaw = -90.0f,
		};
		state.view = {
			.pos = vec3(0.0f, 10.0f, 0.0f),
			.front = glm::normalize(vec3(1.0f, -1.0f, 0.0f)),
			.up = vec3(0.0f, 1.0f, 0.0f),
			.fov = 70.0f,
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
		this->ub.view = glm::lookAt(this->view.pos, this->view.pos + this->view.front, this->view.up);
		this->ub.projection = glm::perspective(glm::radians(this->view.fov), (float)this->scr_res.x / (float)this->scr_res.y, 0.1f, 10000.0f);
		this->ub.view_pos = vec4(this->view.pos, 0.0f);
	}

	void uploadModelViewProj(uint ubo) const {
		glNamedBufferSubData(ubo, 0, 4*sizeof(mat4) + sizeof(vec4), &this->ub);
	}

	void uploadViewProj(uint ubo) const {
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
	vec3 scale = vec3(1.0);
	vec3 velocity = vec3(0.0f);
	vec3 pos = vec3(0.0f);
	float angle = 0;
	float life = 0;
	float radius = 0;
	float dmg = 0;
};

// TODO:
// struct AnimatedEntity {};

const float flr = 0.0f;
const float gravity = 0.0002f;

// TODO:
float cam_angle = 0.0f;
vec3 floor_point = vec3(0.0f);

int main() {
	GLFWwindow *window = init();
	double scroll_xoffset;
	State state = State::init(window);
	glfwSetWindowUserPointer(window, reinterpret_cast<void *>(&state));
	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, topDownCursorPosCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetWindowSizeCallback(window, windowSizeCallback);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);

	// Initialize buffers
	uint vao;
	uint ubo;
	glCreateVertexArrays(1, &vao);
	glCreateBuffers(1, &ubo);

	Vertex::setupVAO(vao);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
	glNamedBufferData(ubo, sizeof(UniformBuffer), &state.ub, GL_DYNAMIC_DRAW);

	// Initialize shaders
	const uint model_vert_shader = createShader("./shaders/model.vert", "./shaders/model_vert.frag");
	const uint model_shader = createShader("./shaders/model.vert", "./shaders/model.frag");
	const uint model_anim_shader = createShader("./shaders/model_anim.vert", "./shaders/model.frag");
	const uint model_plain_shader = createShader("./shaders/model.vert", "./shaders/model_plain.frag");
	const uint model_plain_anim_shader = createShader("./shaders/model_anim.vert", "./shaders/model_plain.frag");

	const int model_anim_shader_bone_matrices = glGetUniformLocation(model_plain_anim_shader, "boneMatrices");

	Model tower_model = Model::init("./assets/fantasy_tower/scene.gltf", vao, model_shader);
	Model map = Model::init("./assets/low_poly_island/scene.gltf", vao, model_shader);
	Model cube = Model::init("./assets/cube.obj", vao, model_plain_shader);
	Model mouse = Model::init("./assets/mouse/mouse.gltf", vao, model_shader);
	Model cat = Model::init("./assets/cat/bleh.gltf", vao, model_shader);
	Model magic_tower_model = Model::init("./assets/wizard_tower/scene.gltf", vao, model_shader);
	Model water = Model::init("./assets/water/water.gltf", vao, model_shader);

	std::vector<Entity> enemies;

	Entity tower = {
		.model = &tower_model,
		.scale = vec3(12.0f),
		.pos = vec3(0.0f, 12.0f, 0.0f),
		.angle = 200.0/180.0 * M_PI,
		.life = 100.0f,
	};

	std::vector<Entity> projectiles;
	std::vector<Entity> player_towers;

	CubeMap cube_map = CubeMap::init();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);

	float timer = 0.0f;
	auto start = chrono::steady_clock::now();
	while (!glfwWindowShouldClose(window)) {
		auto now = chrono::steady_clock::now();
		state.dt = chrono::duration_cast<chrono::microseconds>(now - start).count();
		start = now;

		if (timer <= 0.0f) {
			vec3 pos = vec3(dis(gen) * 360.0 - 180.0, 0.0, dis(gen) * 360.0 - 180.0);
			float scale = dis(gen);
			enemies.push_back({ .model = &mouse, .scale = vec3(scale*15.0 + 1.0), .pos = pos, .life = scale*100.0f, .dmg = scale*10.0f });
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

			if (dir != vec3(0.0)) {
				state.view.pos += glm::normalize(dir) * state.view.speed * dt_ms;
			}

			if (state.keys.space) state.view.pos += state.view.up * state.view.speed * dt_ms;
			if (state.keys.shift) state.view.pos -= state.view.up * state.view.speed * dt_ms;
			state.view.pos.y = std::clamp(state.view.pos.y, 10.0f, 100.0f);

			if (state.keys.e) cam_angle += 0.1 * dt_ms;
			if (state.keys.q) cam_angle -= 0.1 * dt_ms;
			state.view.front.x = -std::cos(cam_angle/180.0 * M_PI);
			state.view.front.y = -0.5f;
			state.view.front.z = std::sin(cam_angle/180.0 * M_PI);
			state.view.front = glm::normalize(state.view.front);

			// TODO: tower bullet cooldown
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
						.life = 1.0,
						.dmg = 1.0,
					});
				}
			}

			for (int i = projectiles.size() - 1; i >= 0; i--) {
				// TODO: check collision
				auto& p = projectiles[i];
				if (p.life <= 0) {
					// swap remove is faster but
					// c++ stl fucking sucks and doesn't have the api for it
					projectiles.erase(projectiles.begin() + i);
					continue;
				}
				p.life -= dt_ms/1000;
				p.pos += p.velocity;

				for (auto& e : enemies) {
					// TODO: account for scaled up hitbox
					vec3 u = e.pos - p.pos;
					float u_dist2 = glm::dot(u, u);
					float radius = 1.0f;
					if (u_dist2 < radius*radius) {
						e.life -= 1.0f;
						projectiles.erase(projectiles.begin() + i);
					}
				}
			}

			for (int i = enemies.size() - 1; i >= 0; i--) {
				auto& e = enemies[i];
				if (e.life <= 0.0) {
					enemies.erase(enemies.begin() + i);
					continue;
				}
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
					tower.life -= e.dmg;
					enemies.erase(enemies.begin() + i);
					continue;
				}
			}

			if (tower.life <= 0) {
				exit(0);
			}

			// view mat used in calculating mouse pos
			state.updateViewProj(state.view.pos);
			state.uploadModelViewProj(ubo);

			{ // calc mouse pos to floor
				float x = (2.0f*state.mouse.scr_x)/(float)state.scr_res.x - 1.0f;
				float y = 1.0f - (2.0f*state.mouse.scr_y)/(float)state.scr_res.y;
				float z = 1.0f;

				auto dev_coords = vec3(x, y, z);
				auto view = state.ub.view;
				auto proj = state.ub.projection;
				auto view_proj = proj*view;
				auto view_proj_inv = glm::inverse(view_proj);

				auto near_quat = view_proj_inv*vec4(dev_coords.x, dev_coords.y, 0.0f, 1.0f);
				auto near = vec3(near_quat.x, near_quat.y, near_quat.z)/near_quat.w;
				auto far_quat = view_proj_inv*vec4(dev_coords.x, dev_coords.y, 1.0f, 1.0f);
				auto far = vec3(far_quat.x, far_quat.y, far_quat.z)/far_quat.w;

				auto direction = glm::normalize(far - near);
				auto origin = state.view.pos;
				floor_point = origin + ((flr - origin.y)/direction.y)*direction;

				if (state.keys.left_click) {
					state.keys.left_click = false;
					player_towers.push_back(Entity{
						.model = &tower_model,
						.scale = vec3(6.0f),
						.pos = floor_point + vec3(0, 6, 0),
					});
				}

				// if (state.keys.left_click) {
				// 	float spd = 0.1f;
				// 	vec3 bullet_pos = player.pos + vec3(0.0, 2.0, 0.0);
				// 	vec3 velocity = state.view.front * spd * dt_ms;
				// 	if (enemies.size() > 0) {
				// 		float dist2 = std::numeric_limits<float>::max();
				// 		vec3 line = vec3(0);
				// 		for (const auto& e : enemies) {
				// 			vec3 u = e.pos - bullet_pos;
				// 			float u_dist2 = glm::dot(u, u);
				// 			if (u_dist2 < dist2) {
				// 				line = u;
				// 				dist2 = u_dist2;
				// 			}
				// 		}
				// 		velocity = glm::normalize(line) * spd * dt_ms;
				// 	}
				// 	projectiles.push_back({
				// 		.model = &cube,
				// 		.scale = vec3(0.2),
				// 		.velocity = velocity,
				// 		.pos = bullet_pos,
				// 		.life = 4.0,
				// 	});
				// 	state.keys.left_click = false;
				// }
				//
				// if (state.keys.right_click) {
				// 	vec3 pos = player.pos;
				// 	pos.y = 0;
				// 	player_towers.push_back({
				// 		.model = &magic_tower_model,
				// 		.pos = pos,
				// 	});
				// 	state.keys.right_click = false;
				// }
			}
		}
		{ // render
			glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
				state.updateModel(e.pos, e.scale, e.angle);
				state.uploadModel(ubo);
				e.model->draw();
			}

			state.updateModel(floor_point + vec3(0, 6, 0), vec3(6), 0);
			state.uploadModel(ubo);
			tower.model->draw();

			state.updateModel(tower.pos, tower.scale, tower.angle);
			state.uploadModel(ubo);
			tower.model->draw();

			state.updateModel(vec3(0.0f, -2.0f, 0.0f), vec3(32.0f, 1.0f, 32.0f), 0);
			state.uploadModel(ubo);
			map.draw();

			state.updateModel(vec3(0, -1.0, 0), vec3(1024.0f, 1.0f, 1024.0f), 0);
			state.uploadModel(ubo);
			water.draw();

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

void topDownCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	State *state = reinterpret_cast<State*>(glfwGetWindowUserPointer(window));
	state->mouse.scr_x = xpos;
	state->mouse.scr_y = ypos;
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
