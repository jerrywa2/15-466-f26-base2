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

	//move pipes:
	for (auto &s : spawned) {
		s.transform->position += s.velocity * elapsed;
		if (s.transform->position.y < 2.0f && s.transform->position.y > -2.0f) {
			// std::cout << "pipe in range; bird z: " << bird->position.z << "; pipe z + offset: " << s.transform->position.z + s.offset << std::endl;
			if (bird->position.z > s.transform->position.z + s.offset + 2.0f || bird->position.z < s.transform->position.z + s.offset - 2.0f) {
				// std::cout << "bird destroyed\n";
				despawn(bird);
			}
		}
	}

	//spawn pipe

	//bird physics:
	{
		//flapping:
		if (space.pressed) {
			bird_velocity.z = 5.0f;
		}

		constexpr float Gravity = -16.0f;
		bird_velocity.z += Gravity * elapsed;
		bird->position += bird_velocity * elapsed;
	}
	

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
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

	//the transform itself:
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
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
