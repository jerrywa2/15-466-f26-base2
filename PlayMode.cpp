#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint main_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > main_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("bird.pnct"));
	main_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

GLuint pipe_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > pipe_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("pipe.pnct"));
	pipe_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > main_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("bird.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = main_meshes->lookup(mesh_name);
		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline = lit_color_texture_program_pipeline;
		drawable.pipeline.vao = main_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

PlayMode::PlayMode() : scene(*main_scene) {

	for (auto &transform : scene.transforms) {	
		if (transform.name == "Bird") bird = &transform;
	}
	if (bird == nullptr) throw std::runtime_error("Bird not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	
	spawn(*pipe_meshes, pipe_meshes_for_lit_color_texture_program, "Cylinder", glm::vec3(0.0f, 10.0f, -5.3f));//, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.2f, 1.2f, 1.2f));
	spawned.back().velocity = glm::vec3(0.0f, -5.0f, 0.0f);

	spawn(*pipe_meshes, pipe_meshes_for_lit_color_texture_program, "Cylinder", glm::vec3(0.0f, 20.0f, -3.3f));//, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.2f, 1.2f, 1.2f));
	spawned.back().velocity = glm::vec3(0.0f, -5.0f, 0.0f);

	spawn(*pipe_meshes, pipe_meshes_for_lit_color_texture_program, "Cylinder", glm::vec3(0.0f, 30.0f, -6.3f));//, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.2f, 1.2f, 1.2f));
	spawned.back().velocity = glm::vec3(0.0f, -5.0f, 0.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_SPACE) {
			space.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//oscillation presets:
	constexpr float Amplitude = 1.5f;
	constexpr float Period = 3.0f;
	osc_time += elapsed;

	//move pipes:
	for (auto &s : spawned) {
		s.transform->position += s.velocity * elapsed;
		if (s.transform->position.y < 2.0f && s.transform->position.y > -2.0f) {
			// std::cout << "pipe in range; bird z: " << bird->position.z << "; pipe z + offset: " << s.transform->position.z + s.offset << std::endl;
			if (bird->position.z > s.transform->position.z + s.offset + 2.0f || bird->position.z < s.transform->position.z + s.offset - 2.0f) {
				// std::cout << "bird destroyed\n";
				game_over = true;
				despawn(bird);
			}
		}
		if (!s.scored && !game_over && s.transform->position.y < -2.0f){
			s.scored = true;
			score++;
		}
		if (s.transform->position.y < -10.0f) {
			despawn(s.transform);
		}
		//oscillate pipes
		if (s.oscillate) {
			s.transform->position.z = s.baseZ + Amplitude * std::sin(osc_time * 2.0f * float(M_PI) / Period + s.phase);
		}
	}


	//spawn pipes:
	if (spawnCD <= 0.0f) {
		static std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution< float > dist(-3.0f, 3.0f);
		std::uniform_real_distribution< float > distPhase(0, 2 * float(M_PI));
		auto newPipe = spawn(*pipe_meshes, pipe_meshes_for_lit_color_texture_program, "Cylinder", glm::vec3(0.0f, 40.0f + (0.6f * (game_velocity - 5)), -5.3f + dist(rng)));//, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.2f, 1.2f, 1.2f));
		spawned.back().velocity = glm::vec3(0.0f, -game_velocity, 0.0f);
		spawned.back().oscillate = dist(rng) > 1.0f; //randomly determine if the pipe will oscillate or not (one thirds of the time)
		spawned.back().baseZ = newPipe->position.z;
		spawned.back().phase = distPhase(rng);

		game_velocity += 0.5f;
		spawnCD = 2.0f;
	} else {
		spawnCD -= elapsed;
	}

	//bird physics:
	if (space.pressed && space.downs == 0) {
		bird_velocity.z = 7.0f; //flap
	}
	constexpr float Gravity = -24.0f;
	bird_velocity.z += Gravity * elapsed;
	bird->position += bird_velocity * elapsed;

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	space.downs = 0;
}

Scene::Transform *PlayMode::spawn(MeshBuffer const &buffer, GLuint vao,
	std::string const &mesh_name,
	glm::vec3 const &position, glm::quat const &rotation, glm::vec3 const &scale) {

	Mesh const &mesh = buffer.lookup(mesh_name); //throws if name is wrong

	scene.transforms.emplace_back();
	Scene::Transform *transform = &scene.transforms.back();
	transform->name = mesh_name;
	transform->position = position;
	transform->rotation = rotation;
	transform->scale = scale;

	scene.drawables.emplace_back(transform);
	Scene::Drawable &drawable = scene.drawables.back();

	drawable.pipeline = lit_color_texture_program_pipeline;
	drawable.pipeline.vao = vao;
	drawable.pipeline.type = mesh.type;
	drawable.pipeline.start = mesh.start;
	drawable.pipeline.count = mesh.count;

	spawned.emplace_back(SpawnedObject{transform, mesh_name});
	return transform;
}

void PlayMode::despawn(Scene::Transform *transform) {
	if (!transform) return;

	assert(!camera || camera->transform != transform);

	//reparent children
	for (auto &t : scene.transforms) {
		if (t.parent == transform) t.parent = transform->parent;
	}

	//erase anything referring to this transform
	scene.drawables.remove_if([transform](Scene::Drawable const &d){ return d.transform == transform; });
	scene.cameras.remove_if([transform](Scene::Camera const &c){ return c.transform == transform; });
	scene.lights.remove_if([transform](Scene::Light const &l){ return l.transform == transform; });

	//the transform itself
	scene.transforms.remove_if([transform](Scene::Transform const &t){ return &t == transform; });

	spawned.erase(
		std::remove_if(spawned.begin(), spawned.end(),
			[transform](SpawnedObject const &s){ return s.transform == transform; }),
		spawned.end());
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("SPACE TO FLAP",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("SCORE: " + std::to_string(score),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H + 0.2f, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		if (game_over)
		lines.draw_text("GAME OVER",
			glm::vec3(-aspect + 18 * H, -1.0 + 12 * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
	}
}
