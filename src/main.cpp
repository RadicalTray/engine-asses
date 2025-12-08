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
	bool num[10];
	bool esc;
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
			.pos = vec3(60.0f, 25.0f, 0.0f),
			.front = glm::normalize(vec3(0.0f, -1.0f, -1.0f)),
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

	// TODO: reduce mem, use union
	float angle = 0;
	float life = 0;
	float radius = 0;
	float dmg = 0;
	float cooldown = 0;
	float timer = 0;
	float bullet_size = 0;
	float speed = 0;
	vec4 color = vec4(0); // too lazy to do a switch ()
};

// TODO:
// struct AnimatedEntity {};

const std::array<UIVertex, 4> square_uiverts = {
	UIVertex{ .pos = vec2(0, 0), .tex = vec2(0, 0) },
	UIVertex{ .pos = vec2(1, 0), .tex = vec2(1, 0) },
	UIVertex{ .pos = vec2(0, 1), .tex = vec2(0, 1) },
	UIVertex{ .pos = vec2(1, 1), .tex = vec2(1, 1) },
};

const std::array<uint, 6> square_uiindices = {
	1, 3, 2,
	2, 0, 1,
};

const float flr = 0.0f;
const float gravity = 0.0002f;

int scene = 0; // 0 = menu, 1 = game
float cam_angle = 0.0f;
vec3 floor_point = vec3(0.0f);
float acctime = 0.0f;
int money = 100;
float enemy_scaling = 0.5f;

float game_time = 0;

// ENUM ._.
int focus = 0;

int main() {
	GLFWwindow *window = init();
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
	glCreateVertexArrays(1, &vao);
	Vertex::setupVAO(vao);

	// TODO: initialize ui vertices
	//  2 for endless and quit (main menu)
	//  3 for towers
	//  2 for hp (bar)
	uint ui_vao;
	glCreateVertexArrays(1, &ui_vao);
	UIVertex::setupVAO(ui_vao);

	uint ui_b[3];
	glCreateBuffers(4, ui_b);
	uint ui_vbo = ui_b[0], ui_ebo = ui_b[1], num_vbo = ui_b[2], num_ebo = ui_b[3];
	glNamedBufferData(ui_vbo, square_uiverts.size() * sizeof(UIVertex), square_uiverts.data(), GL_STATIC_DRAW);
	glNamedBufferData(ui_ebo, square_uiindices.size() * sizeof(uint), square_uiindices.data(), GL_STATIC_DRAW);

	std::vector<UIVertex> num_vertices;
	num_vertices.reserve(10 * square_uiverts.size());
	std::vector<uint> num_indices;
	num_indices.reserve(10 * square_uiindices.size());

	for (int i = 0; i < 10; i++) {
		float x0 = (float)      i/10.0f;
		float x1 = (float)(i + 1)/10.0f;
		num_vertices.push_back(UIVertex{ .pos = vec2(0, 0), .tex = vec2(x0, 0) }); // 00
		num_vertices.push_back(UIVertex{ .pos = vec2(1, 0), .tex = vec2(x1, 0) }); // 10
		num_vertices.push_back(UIVertex{ .pos = vec2(0, 1), .tex = vec2(x0, 1) }); // 01
		num_vertices.push_back(UIVertex{ .pos = vec2(1, 1), .tex = vec2(x1, 1) }); // 11
		for (const auto idx : square_uiindices) {
			num_indices.push_back(4*i + idx);
		}
	}

	glNamedBufferData(num_vbo, num_vertices.size() * sizeof(UIVertex), num_vertices.data(), GL_STATIC_DRAW);
	glNamedBufferData(num_ebo, num_indices.size() * sizeof(uint), num_indices.data(), GL_STATIC_DRAW);

	uint ubo;
	glCreateBuffers(1, &ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
	glNamedBufferData(ubo, sizeof(UniformBuffer), &state.ub, GL_DYNAMIC_DRAW);

	// Initialize shaders
	// const uint model_vert_shader = createShader("./shaders/model.vert", "./shaders/model_vert.frag");
	const uint model_shader = createShader("./shaders/model.vert", "./shaders/model.frag");
	const uint model_color_shader = createShader("./shaders/model.vert", "./shaders/model_color.frag");
	// const uint model_anim_shader = createShader("./shaders/model_anim.vert", "./shaders/model.frag");
	const uint model_plain_shader = createShader("./shaders/model.vert", "./shaders/model_plain.frag");
	// const uint model_plain_anim_shader = createShader("./shaders/model_anim.vert", "./shaders/model_plain.frag");
	const uint ui_shader = createShader("./shaders/ui.vert", "./shaders/ui.frag");

	// const int model_anim_shader_bone_matrices = glGetUniformLocation(model_plain_anim_shader, "boneMatrices");

	Model tower_model = Model::init("./assets/fantasy_tower/scene.gltf", vao, model_color_shader);
	Model map = Model::init("./assets/low_poly_island/scene.gltf", vao, model_shader);
	Model cube = Model::init("./assets/cube.obj", vao, model_plain_shader);
	Model mouse = Model::init("./assets/mouse/mouse.gltf", vao, model_shader);
	Model cat = Model::init("./assets/cat/bleh.gltf", vao, model_shader);
	Model magic_tower_model = Model::init("./assets/wizard_tower/scene.gltf", vao, model_shader);
	Model water = Model::init("./assets/water/water.gltf", vao, model_shader);

	uint endless_focused = texture2DFromFile("./assets/ui/endless_focused.png", 1, 4);
	uint endless_idle = texture2DFromFile("./assets/ui/endless_idle.png", 1, 4);
	uint quit_focused = texture2DFromFile("./assets/ui/quit_focused.png", 1, 4);
	uint quit_idle = texture2DFromFile("./assets/ui/quit_idle.png", 1, 4);
	uint red_tower_png = texture2DFromFile("./assets/ui/red_tower.png", 1, 4);
	uint green_tower_png = texture2DFromFile("./assets/ui/green_tower.png", 1, 4);
	uint blue_tower_png = texture2DFromFile("./assets/ui/blue_tower.png", 1, 4);
	uint numbers_png = texture2DFromFile("./assets/ui/numbers.png", 1, 4);

	std::vector<Entity> enemies;

	float tower_max_hp = 250.0f;
	Entity tower = {
		.model = &tower_model,
		.scale = vec3(12.0f),
		.pos = vec3(0.0f, 12.0f, 0.0f),
		.angle = 200.0/180.0 * M_PI,
		.life = tower_max_hp,
		.color = vec4(1, 1, 1, 1),
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

		glfwPollEvents();
		float scr_x_scale = (float)state.scr_res.y / (float)state.scr_res.x; // used in ui

		if (timer <= 0.0f) {
			if (game_time > 60.0 * 1000.0) {
				enemy_scaling = 1.0f;
				timer = 1.0f;
			} else if (game_time > 90.0 * 1000.0) {
				enemy_scaling = 2.0f;
				timer = 0.5f;
			} else if (game_time > 120.0 * 1000.0) {
				enemy_scaling = 4.0f;
				timer = 0.1f;
			} else {
				enemy_scaling = 0.5f;
				timer = 2.0f;
			}
			vec3 pos = vec3(dis(gen) * 360.0 - 180.0, 0.0, dis(gen) * 360.0 - 180.0);
			auto line = tower.pos - pos;
			line.y = 0.0;
			auto dir = glm::normalize(line);
			float rat = glm::length(line)/160.0f;
			if (rat < 1.0f) {
				pos *= 1/rat;
			}
			float angle = getAngle(vec2(1.0, 0.0), vec2(dir.x, dir.z));
			float scale = dis(gen)*enemy_scaling + 1;
			enemies.push_back(Entity{
				.model = &mouse,
				.scale = vec3(scale*15.0),
				.velocity = dir * (1/scale)*0.010f,
				.pos = pos,
				.angle = angle,
				.life = scale*100.0f,
				.dmg = (scene == 1) ? (scale * 25.0f) : 0,
			});
		} else {
			timer -= state.dt / 1'000'000.0f;
		}

		if (scene == 1) { // scene == game (1)
			{ // process
				float dt_ms = state.dt/1000.0f;
				game_time += dt_ms;

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

				if (state.keys.e) cam_angle -= 0.1 * dt_ms;
				if (state.keys.q) cam_angle += 0.1 * dt_ms;
				state.view.front.x = -std::cos(cam_angle/180.0 * M_PI);
				state.view.front.y = -0.5f;
				state.view.front.z = std::sin(cam_angle/180.0 * M_PI);
				state.view.front = glm::normalize(state.view.front);

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

						auto sc = vec2(0.22);
						sc.x *= scr_x_scale;
						const auto left = -sc.x/2 - (sc.x + 0.01*scr_x_scale); // leftmost
						const auto right = -sc.x/2 + (sc.x + 0.01*scr_x_scale) + sc.x; // rightmost
						const auto bot = -1 + 0.04 - (sc.y - 0.2)/2; // botmost
						const auto top = -1 + 0.04 - (sc.y - 0.2)/2 + sc.y; // topmost

						if (left <= dev_coords.x && dev_coords.x <= right
						 &&  bot <= dev_coords.y && dev_coords.y <= top) {
							for (int i = 1; i <= 3; i++) {
								if (dev_coords.x <= left + (float)i/3.0f * (right - left)) {
									if (focus != i) {
										focus = i;
									} else {
										focus = 0;
									}
									break;
								}
							}
						} else {
							if (focus != 0) {
								bool ok = false;
								if (focus == 1 && money >= 50) {
									money -= 50;
									ok = true;
								} else if (focus == 2 && money >= 100) {
									money -= 100;
									ok = true;
								} else if (focus == 3 && money >= 250) {
									money -= 250;
									ok = true;
								}
								if (ok) {
									auto color = vec4(0);
									float radius = 50;
									float dmg = 100;
									float cooldown = 1000;
									float bullet_size = 0.5;
									float speed = 0.02;
									float life = 10;
									switch (focus) {
									case 1: // red (default)
										color = vec4(1, 0, 0, 1);
										radius = 60;
										dmg = 20;
										cooldown = 500;
										bullet_size = 0.5;
										speed = 0.06;
										life = 10;
										break;
									case 2: // green (shotgun)
										color = vec4(0, 1, 0, 1);
										radius = 25;
										dmg = 150;
										cooldown = 1000;
										bullet_size = 1.5;
										speed = 0.04;
										life = 2;
										break;
									case 3: // blue (sniper)
										color = vec4(0, 0, 1, 1);
										radius = 200;
										dmg = 500;
										cooldown = 2000;
										bullet_size = 0.4;
										speed = 1.5;
										life = 10;
										break;
									case 0:
										break; // unreachable
									default:
										break;
									}
									player_towers.push_back(Entity{
										.model = &tower_model,
										.scale = vec3(6.0f),
										.pos = floor_point + vec3(0, 6, 0),
										.life = life,
										.radius = radius,
										.dmg = dmg,
										.cooldown = cooldown,
										.bullet_size = bullet_size,
										.speed = speed,
										.color = color,
									});
								}
							}
						}
					}
					if (state.keys.right_click) {
						state.keys.right_click = false;
						focus = 0;
					}
				}

				for (auto& t : player_towers) {
					if (t.timer <= 0) {
						if (enemies.size() > 0) { // shoots
							t.timer = t.cooldown;
							vec3 bullet_pos = t.pos + vec3(0.0, t.scale.y * 0.0, 0.0);
							float dist2 = std::numeric_limits<float>::max();
							vec3 dir = vec3(0);
							for (const auto& e : enemies) {
								vec3 u = e.pos - bullet_pos;
								float u_dist2 = glm::dot(u, u);
								if (u_dist2 < dist2) {
									dir = u;
									dist2 = u_dist2;
								}
							}
							if (dist2 > t.radius*t.radius) {
								continue;
							}
							dir = glm::normalize(dir);
							vec3 velocity = dir * t.speed * dt_ms;
							projectiles.push_back(Entity{
								.model = &cube,
								.scale = vec3(t.bullet_size),
								.velocity = velocity,
								.pos = bullet_pos,
								.life = t.life,
								.dmg = t.dmg,
								.color = t.color,
							});
						}
					} else {
						t.timer -= dt_ms;
					}
				}

				for (int i = projectiles.size() - 1; i >= 0; i--) {
					auto& p = projectiles[i];
					if (p.life <= 0) {
						projectiles.erase(projectiles.begin() + i);
						continue;
					}
					p.life -= dt_ms/1000;
					p.pos += p.velocity;

					bool brk = false;
					for (auto& e : enemies) {
						vec3 u = e.pos - p.pos;
						float u_dist2 = glm::dot(u, u);
						float radius = (e.scale.x + p.scale.x)/2;
						if (u_dist2 <= radius*radius) {
							e.life -= p.dmg;
							projectiles.erase(projectiles.begin() + i);
							brk = true;
							break;
						}
					}
					if (brk) { continue; }
				}

				for (int i = enemies.size() - 1; i >= 0; i--) {
					auto& e = enemies[i];
					if (e.life <= 0.0) {
						money += e.scale.x * 2.0 + std::sqrt(glm::dot(e.velocity, e.velocity)) * 30;
						enemies.erase(enemies.begin() + i);
						continue;
					}
					e.pos += e.velocity * dt_ms;

					auto line = tower.pos - e.pos;
					line.y = 0.0;
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
					scene = 0;
					continue;
				}
			}
			{ // render
				glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				for (const auto& p : projectiles) {
					state.updateModel(p.pos, p.scale, p.angle);
					state.uploadModel(ubo);
					glProgramUniform4f(p.model->shader, 4, p.color.x, p.color.y, p.color.z, p.color.w);
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
					glProgramUniform4f(e.model->shader, 4, e.color.x, e.color.y, e.color.z, e.color.w);
					e.model->draw();
				}

				if (focus != 0) {
					state.updateModel(floor_point + vec3(0, 6, 0), vec3(6), 0);
					state.uploadModel(ubo);

					auto color = vec4(0);
					switch (focus) {
					case 0: // unreachable
						break;
					case 1:
						color = vec4(1, 0, 0, 1);
						break;
					case 2:
						color = vec4(0, 1, 0, 1);
						break;
					case 3:
						color = vec4(0, 0, 1, 1);
						break;
					default:
						break;
					}

					glProgramUniform4f(model_color_shader, 4, color.x, color.y, color.z, color.w);
					tower.model->draw();
				}

				glProgramUniform4f(tower.model->shader, 4, tower.color.x, tower.color.y, tower.color.z, tower.color.w);
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

				{
					glDisable(GL_DEPTH_TEST);

					glUseProgram(ui_shader);
					glBindVertexArray(ui_vao);
					glVertexArrayVertexBuffer(ui_vao, 0, ui_vbo, 0, sizeof(UIVertex));
					glVertexArrayElementBuffer(ui_vao, ui_ebo);

					{
						glProgramUniform1f(ui_shader, 3, 0.0); // opacity = 0

						// hp bar
						glProgramUniform2f(ui_shader, 1,  2.0,  0.01);
						glProgramUniform2f(ui_shader, 2, -1.0,  0.99);
						glProgramUniform4f(ui_shader, 4, 0, 0, 0, 1);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						glProgramUniform2f(ui_shader, 1,  2.0,  0.005);
						glProgramUniform2f(ui_shader, 2, -1.0, 0.995);
						glProgramUniform4f(ui_shader, 4, 0.3, 0.3, 0.3, 1);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						glProgramUniform2f(ui_shader, 1,  2.0*(tower.life/tower_max_hp),  0.005);
						glProgramUniform2f(ui_shader, 2, -1.0, 0.995);
						glProgramUniform4f(ui_shader, 4, 0, 1, 0, 1);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						// rgb towers borders
						const auto unfoc_color = vec4(1);
						const auto foc_color = vec4(1, 0, 0, 1);

						auto color = unfoc_color;
						auto sc = vec2(0.22);
						sc.x *= scr_x_scale;
						glProgramUniform2f(ui_shader, 1, sc.x, sc.y);

						color = focus == 2 ? foc_color : unfoc_color;
						glProgramUniform2f(ui_shader, 2, -sc.x/2, -1 + 0.04 - (sc.y - 0.2)/2);
						glProgramUniform4f(ui_shader, 4, color.x, color.y, color.z, color.w);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						color = focus == 1 ? foc_color : unfoc_color;
						glProgramUniform2f(ui_shader, 2, -sc.x/2 - (sc.x + 0.01*scr_x_scale), -1 + 0.04 - (sc.y - 0.2)/2);
						glProgramUniform4f(ui_shader, 4, color.x, color.y, color.z, color.w);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						color = focus == 3 ? foc_color : unfoc_color;
						glProgramUniform2f(ui_shader, 2, -sc.x/2 + (sc.x + 0.01*scr_x_scale), -1 + 0.04 - (sc.y - 0.2)/2);
						glProgramUniform4f(ui_shader, 4, color.x, color.y, color.z, color.w);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);
					}

					{
						glProgramUniform1f(ui_shader, 3, 1.0); // opacity = 1

						// rgb towers
						auto sc = vec2(0.2);
						sc.x *= scr_x_scale;
						glProgramUniform1i(ui_shader, 0, 0);
						glProgramUniform2f(ui_shader, 1, sc.x, sc.y);

						glBindTextureUnit(0, green_tower_png);
						glProgramUniform2f(ui_shader, 2, -sc.x/2, -1 + 0.04);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						glBindTextureUnit(0, red_tower_png);
						glProgramUniform2f(ui_shader, 2, -sc.x/2 - (sc.x + 0.03*scr_x_scale), -1 + 0.04);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						glBindTextureUnit(0, blue_tower_png);
						glProgramUniform2f(ui_shader, 2, -sc.x/2 + (sc.x + 0.03*scr_x_scale), -1 + 0.04);
						glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

						// numbers
						glVertexArrayVertexBuffer(ui_vao, 0, num_vbo, 0, sizeof(UIVertex));
						glVertexArrayElementBuffer(ui_vao, num_ebo);

						glBindTextureUnit(0, numbers_png);
						glProgramUniform1i(ui_shader, 0, 0);

						// hp numbers
						sc = vec2(0.6/8, 0.8/8);
						sc.x *= scr_x_scale;
						auto hp_str = std::to_string((uint)tower.life);
						float full_width = (float)hp_str.size()*(float)sc.x;
						for (size_t i = 0; i < hp_str.size(); i++) {
							int num = hp_str[i] - '0';
							glProgramUniform2f(ui_shader, 1, sc.x, sc.y);
							glProgramUniform2f(ui_shader, 2, -full_width/2.0f + i*sc.x,  1 - sc.y - 0.01);
							glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(num * 6 * sizeof(uint)));
						}

						auto tw_sc = vec2(0.22);
						tw_sc.x *= scr_x_scale;
						const auto left = -tw_sc.x/2 - (tw_sc.x + 0.01*scr_x_scale); // leftmost
						const auto right = -tw_sc.x/2 + (tw_sc.x + 0.01*scr_x_scale) + tw_sc.x; // rightmost
						const auto bot = -1 + 0.04 - (tw_sc.y - 0.2)/2; // botmost
						const auto top = -1 + 0.04 - (tw_sc.y - 0.2)/2 + tw_sc.y; // topmost

						auto money_str = std::to_string((uint)money);
						for (size_t i = 0; i < money_str.size(); i++) {
							int num = money_str[i] - '0';
							glProgramUniform2f(ui_shader, 1, sc.x, sc.y);
							glProgramUniform2f(ui_shader, 2, right + 0.01 + i*sc.x, bot);
							glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(num * 6 * sizeof(uint)));
						}

						auto time_str = std::to_string((uint)std::floor(game_time/1000.0f));
						full_width = (float)time_str.size()*(float)sc.x;
						for (size_t i = 0; i < time_str.size(); i++) {
							int num = time_str[i] - '0';
							glProgramUniform2f(ui_shader, 1, sc.x, sc.y);
							glProgramUniform2f(ui_shader, 2, left - full_width - 0.01 + i*sc.x, bot);
							glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(num * 6 * sizeof(uint)));
						}

						sc = vec2(0.6/8, 0.8/8);
						sc /= 2;
						sc.x *= scr_x_scale;
						std::array<std::string, 3> tower_price = {"50", "100", "250"};
						for (int i = 0; i < 3; i++) {
							float full_width = (float)tower_price[i].size()*(float)sc.x;
							float leftest = left  + (float)i/3.0f * (right - left);
							float rightest = left + (float)(i+1)/3.0f * (right - left);
							leftest += (rightest - leftest - full_width)/2;
							for (size_t j = 0; j < tower_price[i].size(); j++) {
								int num = tower_price[i][j] - '0';
								glProgramUniform2f(ui_shader, 1, sc.x, sc.y);
								glProgramUniform2f(ui_shader, 2, leftest + j*sc.x, top);
								glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(num * 6 * sizeof(uint)));
							}
						}
					}

					glEnable(GL_DEPTH_TEST);
				}

				glfwSwapBuffers(window);
			}
		} else { // scene == menu (0)
			{ // process
				float dt_ms = state.dt/1000.0f;

				acctime += dt_ms;

				float x = (2.0f*state.mouse.scr_x)/(float)state.scr_res.x - 1.0f;
				float y = 1.0f - (2.0f*state.mouse.scr_y)/(float)state.scr_res.y;
				float z = 1.0f;
				auto dev_coords = vec3(x, y, z);

				if (dev_coords.y >= 0) {
					focus = 1;
				} else if (dev_coords.y < 0) {
					focus = 2;
				}

				if (state.keys.left_click) {
					state.keys.left_click = false;
					if (focus == 1) { // endless
						focus = 0;
						game_time = 0;
						scene = 1;
						state = State::init(window);
						enemies.resize(0);
						continue;
					} else if (focus == 2) { // quit
						glfwSetWindowShouldClose(window, GLFW_TRUE);
					}
				}

				state.view.pos.x = std::sin(acctime/5000)*75.0f;
				state.view.pos.z = std::cos(acctime/5000)*75.0f;

				state.view.pos.y = std::sin(acctime/1000)*2.0f + 35.0f;

				{
					auto line = tower.pos - state.view.pos;
					line.y = 0.0;
					auto dir = glm::normalize(line);

					float ang = getAngle(vec2(0.0, 1.0), vec2(dir.x, dir.z));
					state.view.front.x = -std::cos(ang);
					state.view.front.y = -0.5f;
					state.view.front.z = std::sin(ang);
					state.view.front = glm::normalize(state.view.front);
				}

				state.updateViewProj(state.view.pos);
				state.uploadModelViewProj(ubo);

				for (int i = enemies.size() - 1; i >= 0; i--) {
					auto& e = enemies[i];
					if (e.life <= 0.0) {
						enemies.erase(enemies.begin() + i);
						continue;
					}
					e.pos += e.velocity * dt_ms;

					auto line = tower.pos - e.pos;
					line.y = 0.0;
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
			}
			{ // render
				glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				for (const auto& e : enemies) {
					state.updateModel(e.pos, e.scale, e.angle);
					state.uploadModel(ubo);
					e.model->draw();
				}

				glProgramUniform4f(tower.model->shader, 4, tower.color.x, tower.color.y, tower.color.z, tower.color.w);
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

				{
					glDisable(GL_DEPTH_TEST);

					glUseProgram(ui_shader);
					glBindVertexArray(ui_vao);
					glVertexArrayVertexBuffer(ui_vao, 0, ui_vbo, 0, sizeof(UIVertex));
					glVertexArrayElementBuffer(ui_vao, ui_ebo);

					glProgramUniform1f(ui_shader, 3, 1.0); // opacity = 1

					// ENDLESS
					auto sc = vec2(5.305084745762712, 1.0);
					sc *= 0.24;
					sc.x *= scr_x_scale;
					glBindTextureUnit(0, focus == 1 ? endless_focused : endless_idle);
					glProgramUniform1i(ui_shader, 0, 0);
					glProgramUniform2f(ui_shader, 1, sc.x, sc.y);
					glProgramUniform2f(ui_shader, 2, -sc.x/2, 0.2-sc.y/2);
					glProgramUniform4f(ui_shader, 4, 0.0, 0.0, 0.0, 0.0);
					glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

					// QUIT
					sc = vec2(2.983050847457627, 1.0);
					sc *= 0.24;
					sc.x *= scr_x_scale;
					glBindTextureUnit(0, focus == 2 ? quit_focused : quit_idle);
					glProgramUniform1i(ui_shader, 0, 0);
					glProgramUniform2f(ui_shader, 1,  sc.x,  sc.y);
					glProgramUniform2f(ui_shader, 2, -sc.x/2, -0.2-sc.y/2);
					glProgramUniform4f(ui_shader, 4, 0, 0, 0, 0);
					glDrawElements(GL_TRIANGLES, static_cast<uint>(square_uiindices.size()), GL_UNSIGNED_INT, 0);

					glEnable(GL_DEPTH_TEST);
				}
				glfwSwapBuffers(window);
			}
		}
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
	case GLFW_KEY_0:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[0] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[0] = false;
			break;
		}
		break;
	case GLFW_KEY_1:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[1] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[1] = false;
			break;
		}
		break;
	case GLFW_KEY_2:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[2] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[2] = false;
			break;
		}
		break;
	case GLFW_KEY_3:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[3] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[3] = false;
			break;
		}
		break;
	case GLFW_KEY_4:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[4] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[4] = false;
			break;
		}
		break;
	case GLFW_KEY_5:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[5] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[5] = false;
			break;
		}
		break;
	case GLFW_KEY_6:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[6] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[6] = false;
			break;
		}
		break;
	case GLFW_KEY_7:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[7] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[7] = false;
			break;
		}
		break;
	case GLFW_KEY_8:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[8] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[8] = false;
			break;
		}
		break;
	case GLFW_KEY_9:
		switch (action) {
		case GLFW_PRESS:
			state->keys.num[9] = true;
			break;
		case GLFW_RELEASE:
			state->keys.num[9] = false;
			break;
		}
		break;
	case GLFW_KEY_ESCAPE:
		switch (action) {
		case GLFW_PRESS:
			state->keys.esc = true;
			break;
		case GLFW_RELEASE:
			state->keys.esc = false;
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

// if (state.keys.left_click) {
//	float spd = 0.1f;
//	vec3 bullet_pos = player.pos + vec3(0.0, 2.0, 0.0);
//	vec3 velocity = state.view.front * spd * dt_ms;
//	if (enemies.size() > 0) {
//		float dist2 = std::numeric_limits<float>::max();
//		vec3 line = vec3(0);
//		for (const auto& e : enemies) {
//			vec3 u = e.pos - bullet_pos;
//			float u_dist2 = glm::dot(u, u);
//			if (u_dist2 < dist2) {
//				line = u;
//				dist2 = u_dist2;
//			}
//		}
//		velocity = glm::normalize(line) * spd * dt_ms;
//	}
//	projectiles.push_back({
//		.model = &cube,
//		.scale = vec3(0.2),
//		.velocity = velocity,
//		.pos = bullet_pos,
//		.life = 4.0,
//	});
//	state.keys.left_click = false;
// }
//
// if (state.keys.right_click) {
//	vec3 pos = player.pos;
//	pos.y = 0;
//	player_towers.push_back({
//		.model = &magic_tower_model,
//		.pos = pos,
//	});
//	state.keys.right_click = false;
// }
