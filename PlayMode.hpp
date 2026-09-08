#include "Mode.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, space;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	
	//camera:
	Scene::Camera *camera = nullptr;

	//bird:
	Scene::Transform *bird = nullptr;
	glm::vec3 bird_velocity = glm::vec3(0.0f);

	//objects placed at runtime:
	struct SpawnedObject {
		Scene::Transform *transform = nullptr;
		std::string mesh_name;
		glm::vec3 velocity = glm::vec3(0.0f);
		bool scored = false;
		float offset = 5.3f;
		float baseZ = 0.0f;
		bool oscillate = false;
		float phase = 0.0f;
	};
	std::vector< SpawnedObject > spawned;

	//game parameters:
	std::uint32_t score = 0;
	float spawnCD = 0.0f;
	float osc_time = 0.0f;
	float game_velocity = 5.0f;
	bool game_over = false;

	Scene::Transform *spawn(MeshBuffer const &buffer, GLuint vao,
		std::string const &mesh_name,
		glm::vec3 const &position = glm::vec3(0.0f),
		glm::quat const &rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec3 const &scale = glm::vec3(1.0f));
	void despawn(Scene::Transform *transform);

};
