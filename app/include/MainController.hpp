#ifndef MAIN_CONTROLLER_HPP
#define MAIN_CONTROLLER_HPP

#include <engine/core/Controller.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace app {

class MainController : public engine::core::Controller {
public:
    std::string_view name() const override;
    void initialize() override;
    void poll_events() override;
    bool loop() override;
    void update() override;
    void begin_draw() override;
    void draw_block(std::string block_name, glm::vec3 position);
    void draw_block(std::string block_name, const glm::mat4& model_matrix);
    void draw_block_with_shadows(std::string block_name, glm::vec3 position);
    void draw_regular_model(std::string model_name, glm::vec3 position);
    void draw_regular_model(std::string model_name, const glm::mat4& model_matrix);
    void draw_regular_model(std::string model_name, glm::vec3 position, glm::vec3 scale, glm::vec3 rotation = glm::vec3(0.0f));
    void draw_regular_model_with_shadows(std::string model_name, glm::vec3 position);
    void draw_regular_model_with_shadows(std::string model_name, glm::vec3 position, glm::vec3 scale, glm::vec3 rotation = glm::vec3(0.0f));
    void draw_skybox();
    void draw() override;
    void end_draw() override;
    void terminate() override;

private:
    struct BlockInfo {
        std::string model_name;
        glm::vec3 position;
        glm::mat4 model_matrix;
    };

    struct RegularModelInfo {
        std::string model_name;
        glm::vec3 position;
        glm::mat4 model_matrix;
    };

    // void draw_lucky_block(glm::vec3 position);
    void draw_block_shadow(std::string block_name, const glm::mat4& model_matrix);
    void draw_regular_model_shadow(std::string model_name, const glm::mat4& model_matrix);
    void render_all_shadows();
    void render_all_scene_blocks();
    void render_all_scene_regular_models();

    void setup_instanced_grid();
    void draw_instanced_grid();
    void draw_instanced_grid_shadows();
    void set_grid_size(int size);

    glm::mat4 calculate_block_model_matrix(glm::vec3 position);
    glm::mat4 calculate_regular_model_matrix(glm::vec3 position);

    bool m_cursor_enabled{true};
    std::vector<BlockInfo> m_blocks_to_render;
    std::vector<RegularModelInfo> m_regular_models_to_render;

    glm::vec3 m_point_light_position{5.72f, 3.66f, 7.88f};
    glm::vec3 m_point_light_ambient{0.1f, 0.1f, 0.0f};
    glm::vec3 m_point_light_diffuse{1.0f, 0.8f, 0.4f};
    glm::vec3 m_point_light_specular{1.0f, 0.9f, 0.6f};
    float m_point_light_constant{1.0f};
    float m_point_light_linear{0.14f};
    float m_point_light_quadratic{0.07f};

    glm::vec3 m_moon_direction{-0.2f, -1.0f, -0.3f};
    glm::vec3 m_moon_ambient{0.1f, 0.1f, 0.15f};
    glm::vec3 m_moon_diffuse{0.3f, 0.3f, 0.4f};
    glm::vec3 m_moon_specular{0.5f, 0.5f, 0.6f};

    bool m_spotlight_enabled{false};
    glm::vec3 m_spotlight_ambient{0.0f, 0.0f, 0.0f};
    glm::vec3 m_spotlight_diffuse{1.0f, 1.0f, 1.0f};
    glm::vec3 m_spotlight_specular{1.0f, 1.0f, 1.0f};
    float m_spotlight_constant{1.0f};
    float m_spotlight_linear{0.09f};
    float m_spotlight_quadratic{0.032f};
    float m_spotlight_cutoff{12.5f};      // Inner cone in degrees
    float m_spotlight_outer_cutoff{15.0f}; // Outer cone in degrees

    float m_shadow_strength{1.0f};

    glm::vec3 m_regular_model_position{5.0f, 0.8125f, 4.0f};

    bool m_use_instancing{true};
    int m_grid_size{15};

    glm::vec3 m_house_position{10.0f, 4.2f, 7.5f};
    glm::vec3 m_house_scale{0.355f};
    glm::vec3 m_house_rotation{0.0f,90.0f,0.0f};

    glm::vec3 m_skin_position{7.041f, 3.360f, 7.880f};
    glm::vec3 m_skin_scale{0.58f};
    glm::vec3 m_skin_rotation{0.0f, -30.0f,0.0f};

    // Animation system
    enum class AnimationState {
        IDLE,
        ROTATING_TO_ZERO,
        MOVING_FORWARD,
        WAITING,
        ROTATING_TO_LOOK_BACK,
        MOVING_BACK,
        ROTATING_TO_ORIGINAL
    };

    AnimationState m_animation_state{AnimationState::IDLE};
    float m_animation_timer{0.0f};
    float m_animation_duration{2.0f}; // 2 seconds for each phase
    glm::vec3 m_skin_original_position;
    glm::vec3 m_skin_original_rotation;
    glm::vec3 m_skin_target_position;
    glm::vec3 m_skin_target_rotation;

    void update_skin_animation(float dt);
    void start_skin_animation();
};

}

#endif