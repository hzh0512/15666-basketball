#pragma once

#include "Mode.hpp"
#include "DrawLines.hpp"
#include "Scene.hpp"

#include <memory>

struct RollMode : Mode {
	RollMode();
	virtual ~RollMode();

	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void restart();

	int level = 0, score = 0;
	bool start = false, ball_ready = true, hit = false;
	glm::vec3 ball_vel;
	float ball_angle = 0.0f, ball_angle_vel = 2.0f;
	float ti = 60.f;
};
