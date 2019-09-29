#include "RollMode.hpp"
#include "DrawLines.hpp"
#include "LitColorTextureProgram.hpp"
#include "Mesh.hpp"
#include "Sprite.hpp"
#include "DrawSprites.hpp"
#include "data_path.hpp"
#include "Sound.hpp"
#include "collide.hpp"
#include "gl_errors.hpp"
#include <sstream>

//for glm::pow(quaternion, float):
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <iostream>

//used for lookup later:
Scene::Transform *ball_transform;

struct MeshCollider {
    MeshCollider(Scene::Transform *transform_, Mesh const &mesh_, MeshBuffer const &buffer_) : transform(transform_), mesh(&mesh_), buffer(&buffer_) { }
    Scene::Transform *transform;
    Mesh const *mesh;
    MeshBuffer const *buffer;
};

std::vector< MeshCollider > mesh_colliders;

GLuint roll_meshes_for_lit_color_texture_program = 0;

Load< SpriteAtlas > trade_font_atlas(LoadTagDefault, []() -> SpriteAtlas const * {
	return new SpriteAtlas(data_path("trade-font"));
});

//Load the meshes used in Sphere Roll levels:
Load< MeshBuffer > roll_meshes(LoadTagDefault, []() -> MeshBuffer * {
    MeshBuffer *ret = new MeshBuffer(data_path("basketball.pnct"));

    //Build vertex array object for the program we're using to shade these meshes:
    roll_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);

    return ret;
});

Load< Scene > roll_scene(LoadTagLate, []() -> Scene const * {
    Scene *ret = new Scene();

    ret->load(data_path("basketball.scene"), [](Scene &scene,
            Scene::Transform *transform, std::string const &mesh_name){
        Mesh const *mesh = &roll_meshes->lookup(mesh_name);
        scene.drawables.emplace_back(transform);
        Scene::Drawable::Pipeline &pipeline = scene.drawables.back().pipeline;

        //set up drawable to draw mesh from buffer:
        pipeline = lit_color_texture_program_pipeline;
        pipeline.vao = roll_meshes_for_lit_color_texture_program;
        pipeline.type = mesh->type;
        pipeline.start = mesh->start;
        pipeline.count = mesh->count;

        if (mesh_name != "Sphere") {
            mesh_colliders.emplace_back(transform, *mesh, *roll_meshes);
        } else {
            ball_transform = transform;
        }
    });

    return ret;
});

RollMode::RollMode() {
    ball_vel.x = 0.f;
    ball_vel.y = 0.f;
    ball_vel.z = 0.f;
    *ball_transform = *roll_scene->cameras[level].transform;
    ball_transform->position.z -= 1.5f;
    ball_transform->scale.x = 0.3f;
    ball_transform->scale.y = 0.3f;
    ball_transform->scale.z = 0.3f;
}

RollMode::~RollMode() {
}

bool RollMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    if (evt.type == SDL_MOUSEBUTTONDOWN && ball_ready && ti > 0.f) {
        if (!start)
            start = true;

        ball_ready = false;
        float v = 12.f, w = 800.f, h = 600.f;
        float x = evt.button.x;
        float y = evt.button.y;

        auto pos = roll_scene->cameras[level].transform->position;
        auto diff = glm::vec3(0, x/w*13.8f-6.9f, 13.f-y/h*5.5f) - pos;
        if (level == 2) {
            diff = glm::vec3(-(x/w*13.8f-6.9f)*0.7, (x/w*13.8f-5.8f)*0.7, 13.f-y/h*5.5f) - pos;
        } else if (level == 1) {
            diff = glm::vec3((x/w*13.8f-6.9f)*0.7, (x/w*13.8f-6.9f)*0.7, 13.f-y/h*5.5f) - pos;
        }
        auto r = diff / sqrt(diff.x * diff.x + diff.y * diff.y + diff.z *
                                                                 diff.z);
        ball_vel = v * r;
        return true;
    }
	
	return false;
}

void RollMode::update(float elapsed) {
	//NOTE: turn this on to fly the sphere instead of rolling it -- makes collision debugging easier
	if (ball_transform->position.z < 0.5f) {
        ball_vel.x = 0.f;
        ball_vel.y = 0.f;
        ball_vel.z = 0.f;
        *ball_transform = *roll_scene->cameras[level].transform;
        ball_transform->position.z -= 1.5f;
        ball_transform->scale.x = 0.3f;
        ball_transform->scale.y = 0.3f;
        ball_transform->scale.z = 0.3f;
	    ball_ready = true;
	    hit = false;
	}

	if (start && ti > 0.f) {
	    ti = ti - elapsed;
	    if (ti <= 0.f) {
	        ti = 0.f;
	    }
	}

	const auto goal = glm::vec3(0.7f, 0.f, 4.3f);
	if (!hit && glm::distance(ball_transform->position, goal) < 0.5f) {
	    hit = true;
	    score++;
	    level = (level + 1) % 3;
	}

    if (ball_ready) {
        return;
    }
	{ //player motion:
        const glm::vec3 rot_axis = glm::vec3(0.5, 0.5, 0.707);
		glm::vec3 &position = ball_transform->position;
        ball_angle += elapsed * ball_angle_vel;
        ball_angle_vel *= pow(0.5, elapsed / 1.f);
        ball_transform->rotation = glm::quat(cos(ball_angle), sin(ball_angle)*rot_axis.x,
                                   sin(ball_angle)*rot_axis.y, sin(ball_angle)*rot_axis.z);
        ball_vel.z -= 9.8f * elapsed;
		
		//collide against level:
		float remain = elapsed;
		for (int32_t iter = 0; iter < 10; ++iter) {
			if (remain == 0.0f) break;

			float sphere_radius = 0.3f;
			glm::vec3 sphere_sweep_from = position;
			glm::vec3 sphere_sweep_to = position + ball_vel * remain;

			glm::vec3 sphere_sweep_min = glm::min(sphere_sweep_from, sphere_sweep_to) - glm::vec3(sphere_radius);
			glm::vec3 sphere_sweep_max = glm::max(sphere_sweep_from, sphere_sweep_to) + glm::vec3(sphere_radius);

			bool collided = false;
			float collision_t = 1.0f;
			glm::vec3 collision_at = glm::vec3(0.0f);
			glm::vec3 collision_out = glm::vec3(0.0f);
			for (auto const &collider : mesh_colliders) {
				glm::mat4x3 collider_to_world = collider.transform->make_local_to_world();

				{ //Early discard:
					// check if AABB of collider overlaps AABB of swept sphere:

					//(1) compute bounding box of collider in world space:
					glm::vec3 local_center = 0.5f * (collider.mesh->max + collider.mesh->min);
					glm::vec3 local_radius = 0.5f * (collider.mesh->max - collider.mesh->min);

					glm::vec3 world_min = collider_to_world * glm::vec4(local_center, 1.0f)
						- glm::abs(local_radius.x * collider_to_world[0])
						- glm::abs(local_radius.y * collider_to_world[1])
						- glm::abs(local_radius.z * collider_to_world[2]);
					glm::vec3 world_max = collider_to_world * glm::vec4(local_center, 1.0f)
						+ glm::abs(local_radius.x * collider_to_world[0])
						+ glm::abs(local_radius.y * collider_to_world[1])
						+ glm::abs(local_radius.z * collider_to_world[2]);

					//(2) check if bounding boxes overlap:
					bool can_skip = !collide_AABB_vs_AABB(sphere_sweep_min, sphere_sweep_max, world_min, world_max);

					if (can_skip) {
						//AABBs do not overlap; skip detailed check:
						continue;
					}
				}

				//Full (all triangles) test:
				assert(collider.mesh->type == GL_TRIANGLES); //only have code for TRIANGLES not other primitive types
				for (GLuint v = 0; v + 2 < collider.mesh->count; v += 3) {
					//get vertex positions from associated positions buffer:
					//  (and transform to world space)
					glm::vec3 a = collider_to_world * glm::vec4(collider.buffer->positions[collider.mesh->start+v+0], 1.0f);
					glm::vec3 b = collider_to_world * glm::vec4(collider.buffer->positions[collider.mesh->start+v+1], 1.0f);
					glm::vec3 c = collider_to_world * glm::vec4(collider.buffer->positions[collider.mesh->start+v+2], 1.0f);
					//check triangle:
					bool did_collide = collide_swept_sphere_vs_triangle(
						sphere_sweep_from, sphere_sweep_to, sphere_radius,
						a,b,c,
						&collision_t, &collision_at, &collision_out);

					if (did_collide) {
						collided = true;
					}
				}
			}

			if (!collided) {
				position = sphere_sweep_to;
				remain = 0.0f;
				break;
			} else {
				position = glm::mix(sphere_sweep_from, sphere_sweep_to, collision_t);
				float d = glm::dot(ball_vel, collision_out);
				if (d < 0.0f) {
                    ball_vel -= (1.1f * d) * collision_out;
				}
				remain = (1.0f - collision_t) * remain;
			}
		}
	}
}

void RollMode::draw(glm::uvec2 const &drawable_size) {
	//--- actual drawing ---
	glClearColor(0.45f, 0.45f, 0.50f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	roll_scene->draw(roll_scene->cameras[level]);

	{ //help text overlay:
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		DrawSprites draw(*trade_font_atlas, glm::vec2(0,0), glm::vec2(320, 200), drawable_size, DrawSprites::AlignPixelPerfect);

		if (!start)
		{
			std::string help_text = "click to start";
			glm::vec2 min, max;
			draw.get_text_extents(help_text, glm::vec2(0.0f, 0.0f), 1.0f, &min, &max);
			float x = std::round(160.0f - (0.5f * (max.x + min.x)));
			draw.draw_text(help_text, glm::vec2(x, 1.0f), 1.0f, glm::u8vec4(0x00,0x00,0x00,0xff));
			draw.draw_text(help_text, glm::vec2(x, 2.0f), 1.0f, glm::u8vec4(0xff,0xff,0xff,0xff));
		}

        std::ostringstream stringStream;
        stringStream.precision(1);
        stringStream.str("");
        stringStream << "Time: " << std::fixed << ti << "s";

        DrawSprites draw2(*trade_font_atlas, glm::vec2(0,0), glm::vec2(800,
                                                                       600), drawable_size, DrawSprites::AlignSloppy);
        draw2.draw_text(stringStream.str(), glm::vec2(20.f, 560.0f), 2.f,
                        glm::u8vec4(0x00,0x00,0x00,0xff));

        stringStream.precision(0);
        stringStream.str("");
        stringStream << "Score: " << score;
        draw2.draw_text(stringStream.str(), glm::vec2(650.f, 560.0f), 2.f,
                        glm::u8vec4(0x00,0x00,0x00,0xff));

        if (ti == 0.f) {
            stringStream.str("");
            stringStream << "You got " << score << "!";
            draw2.draw_text(stringStream.str(), glm::vec2(250.f, 300.0f), 4.f,
                            glm::u8vec4(0xff,0x00,0x00,0xff));
        }

	}

	GL_ERRORS();
}
