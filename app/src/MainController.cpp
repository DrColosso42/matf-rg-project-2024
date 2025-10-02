#include <MainController.hpp>
#include <engine/platform/PlatformController.hpp>
#include <engine/graphics/GraphicsController.hpp>
#include <engine/graphics/ShadowController.hpp>
#include <engine/graphics/InstancingController.hpp>
#include <engine/resources/ResourcesController.hpp>
#include <engine/graphics/OpenGL.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <imgui.h>
namespace app {

std::string_view MainController::name() const {
    return "MainController";
}

void MainController::initialize() {
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto camera = graphics->camera();

    camera->MouseSensitivity = 0.3f;
    camera->MovementSpeed = 1.5f;

    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();
    engine::graphics::PointLightShadowParams shadow_params;
    shadow_params.position = m_point_light_position;
    shadow_params.far_plane = 25.0f;
    shadow_params.resolution = 1024;
    shadow_controller->register_point_light("main_light", shadow_params);

    // Setup instancing for the grid
    setup_instanced_grid();

    spdlog::info("{}::initialize", name());
}

void MainController::poll_events() {
    const auto platform = engine::core::Controller::get<engine::platform::PlatformController>();
    if (platform->key(engine::platform::KEY_F1).state() == engine::platform::Key::State::JustPressed) {
        m_cursor_enabled = !m_cursor_enabled;
        platform->set_enable_cursor(m_cursor_enabled);
    }

    if (platform->key(engine::platform::KEY_F).state() == engine::platform::Key::State::JustPressed) {
        m_spotlight_enabled = !m_spotlight_enabled;
    }

    if (platform->key(engine::platform::KEY_M).state() == engine::platform::Key::State::JustPressed) {
        start_skin_animation();
    }
}

bool MainController::loop() {
    auto platform = engine::core::Controller::get<engine::platform::PlatformController>();
    if (platform->key(engine::platform::KeyId::KEY_ESCAPE).is_down()) {
        return false;
    }
    return true;
}

void MainController::update() {
    auto platform = engine::core::Controller::get<engine::platform::PlatformController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto camera = graphics->camera();

    float dt = platform->dt();
    if (platform->key(engine::platform::KEY_W).state() == engine::platform::Key::State::Pressed) {
        camera->move_camera(engine::graphics::Camera::Movement::FORWARD, dt);
    }
    if (platform->key(engine::platform::KEY_S).state() == engine::platform::Key::State::Pressed) {
        camera->move_camera(engine::graphics::Camera::Movement::BACKWARD, dt);
    }
    if (platform->key(engine::platform::KEY_A).state() == engine::platform::Key::State::Pressed) {
        camera->move_camera(engine::graphics::Camera::Movement::LEFT, dt);
    }
    if (platform->key(engine::platform::KEY_D).state() == engine::platform::Key::State::Pressed) {
        camera->move_camera(engine::graphics::Camera::Movement::RIGHT, dt);
    }
    if (platform->key(engine::platform::KEY_SPACE).state() == engine::platform::Key::State::Pressed) {
        camera->move_camera(engine::graphics::Camera::Movement::UP, dt);
    }
    if (platform->key(engine::platform::KEY_LEFT_SHIFT).state() == engine::platform::Key::State::Pressed) {
        camera->move_camera(engine::graphics::Camera::Movement::DOWN, dt);
    }
    if (!m_cursor_enabled) {
        auto mouse = platform->mouse();
        camera->rotate_camera(mouse.dx, mouse.dy);
        camera->zoom(mouse.scroll);
    }

    // Update skin animation
    update_skin_animation(dt);
}

void MainController::begin_draw() {
    engine::graphics::OpenGL::enable_depth_testing();
    engine::graphics::OpenGL::clear_buffers();
}
void MainController::draw_block(std::string block_name, glm::vec3 position) {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    engine::resources::Model * block = resources->model(block_name);
    engine::resources::Shader* shader = resources->shader("grass");

    shader->use();

    shader->set_int("texture_diffuse1", 0);
    shader->set_int("depthMap", 1);

    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.5));

    glm::mat4 view = graphics->camera()->view_matrix();
    glm::mat4 projection = graphics->projection_matrix();

    shader->set_mat4("model", modelMatrix);
    shader->set_mat4("view", view);
    shader->set_mat4("projection", projection);

    shader->set_vec3("viewPos", graphics->camera()->Position);

    shader->set_vec3("dirLight.direction", m_moon_direction);
    shader->set_vec3("dirLight.ambient", m_moon_ambient);
    shader->set_vec3("dirLight.diffuse", m_moon_diffuse);
    shader->set_vec3("dirLight.specular", m_moon_specular);

    shader->set_vec3("pointLight.position", m_point_light_position);
    shader->set_vec3("pointLight.ambient", m_point_light_ambient);
    shader->set_vec3("pointLight.diffuse", m_point_light_diffuse);
    shader->set_vec3("pointLight.specular", m_point_light_specular);
    shader->set_float("pointLight.constant", m_point_light_constant);
    shader->set_float("pointLight.linear", m_point_light_linear);
    shader->set_float("pointLight.quadratic", m_point_light_quadratic);

    if (m_spotlight_enabled) {
        auto camera = graphics->camera();
        shader->set_vec3("spotLight.position", camera->Position);
        shader->set_vec3("spotLight.direction", camera->Front);
        shader->set_vec3("spotLight.ambient", m_spotlight_ambient);
        shader->set_vec3("spotLight.diffuse", m_spotlight_diffuse);
        shader->set_vec3("spotLight.specular", m_spotlight_specular);
        shader->set_float("spotLight.constant", m_spotlight_constant);
        shader->set_float("spotLight.linear", m_spotlight_linear);
        shader->set_float("spotLight.quadratic", m_spotlight_quadratic);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(m_spotlight_cutoff)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(m_spotlight_outer_cutoff)));
    } else {
        shader->set_vec3("spotLight.position", glm::vec3(0.0f));
        shader->set_vec3("spotLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader->set_vec3("spotLight.ambient", glm::vec3(0.0f));
        shader->set_vec3("spotLight.diffuse", glm::vec3(0.0f));
        shader->set_vec3("spotLight.specular", glm::vec3(0.0f));
        shader->set_float("spotLight.constant", 1.0f);
        shader->set_float("spotLight.linear", 0.0f);
        shader->set_float("spotLight.quadratic", 0.0f);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(0.0f)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(0.0f)));
    }

    shadow_controller->bind_shadow_maps(shader, m_shadow_strength);

    shader->set_float("material.shininess", 32.0f);

    block->draw(shader);
}

void MainController::draw_block(std::string block_name, const glm::mat4& model_matrix) {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    engine::resources::Model * block = resources->model(block_name);
    engine::resources::Shader* shader = resources->shader("grass");

    shader->use();

    shader->set_int("texture_diffuse1", 0);
    shader->set_int("depthMap", 1);

    glm::mat4 view = graphics->camera()->view_matrix();
    glm::mat4 projection = graphics->projection_matrix();

    shader->set_mat4("model", model_matrix);
    shader->set_mat4("view", view);
    shader->set_mat4("projection", projection);

    shader->set_vec3("viewPos", graphics->camera()->Position);

    shader->set_vec3("dirLight.direction", m_moon_direction);
    shader->set_vec3("dirLight.ambient", m_moon_ambient);
    shader->set_vec3("dirLight.diffuse", m_moon_diffuse);
    shader->set_vec3("dirLight.specular", m_moon_specular);

    shader->set_vec3("pointLight.position", m_point_light_position);
    shader->set_vec3("pointLight.ambient", m_point_light_ambient);
    shader->set_vec3("pointLight.diffuse", m_point_light_diffuse);
    shader->set_vec3("pointLight.specular", m_point_light_specular);
    shader->set_float("pointLight.constant", m_point_light_constant);
    shader->set_float("pointLight.linear", m_point_light_linear);
    shader->set_float("pointLight.quadratic", m_point_light_quadratic);

    if (m_spotlight_enabled) {
        auto camera = graphics->camera();
        shader->set_vec3("spotLight.position", camera->Position);
        shader->set_vec3("spotLight.direction", camera->Front);
        shader->set_vec3("spotLight.ambient", m_spotlight_ambient);
        shader->set_vec3("spotLight.diffuse", m_spotlight_diffuse);
        shader->set_vec3("spotLight.specular", m_spotlight_specular);
        shader->set_float("spotLight.constant", m_spotlight_constant);
        shader->set_float("spotLight.linear", m_spotlight_linear);
        shader->set_float("spotLight.quadratic", m_spotlight_quadratic);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(m_spotlight_cutoff)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(m_spotlight_outer_cutoff)));
    } else {
        shader->set_vec3("spotLight.position", glm::vec3(0.0f));
        shader->set_vec3("spotLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader->set_vec3("spotLight.ambient", glm::vec3(0.0f));
        shader->set_vec3("spotLight.diffuse", glm::vec3(0.0f));
        shader->set_vec3("spotLight.specular", glm::vec3(0.0f));
        shader->set_float("spotLight.constant", 1.0f);
        shader->set_float("spotLight.linear", 0.0f);
        shader->set_float("spotLight.quadratic", 0.0f);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(0.0f)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(0.0f)));
    }

    shadow_controller->bind_shadow_maps(shader, m_shadow_strength);

    shader->set_float("material.shininess", 32.0f);

    block->draw(shader);
}

void MainController::end_draw() {
    auto platform = engine::core::Controller::get<engine::platform::PlatformController>();
    platform->swap_buffers();
}
// void MainController::draw_lucky_block(glm::vec3 position) {
//     auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
//     auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
//
//     engine::resources::Model * lucky = resources->model("lucky");
//     engine::resources::Shader* shader = resources->shader("lucky");
//
//     shader->use();
//
//     glm::mat4 modelMatrix = glm::mat4(1.0f);
//     modelMatrix = glm::translate(modelMatrix, position);
//     modelMatrix = glm::scale(modelMatrix, glm::vec3(0.05));
//
//     glm::mat4 view = graphics->camera()->view_matrix();
//     glm::mat4 projection = graphics->projection_matrix();
//
//     shader->set_mat4("model", modelMatrix);
//     shader->set_mat4("view", view);
//     shader->set_mat4("projection", projection);
//
//     lucky->draw(shader);
// }

void MainController::draw_skybox() {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();

    engine::resources::Skybox* skybox = resources->skybox("minecraft_sky");
    engine::resources::Shader* shader = resources->shader("skybox");

    graphics->draw_skybox(shader, skybox);
}

void MainController::draw() {
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    m_blocks_to_render.clear();
    m_regular_models_to_render.clear();
    draw_regular_model_with_shadows("torch", m_point_light_position, glm::vec3(0.35f), glm::vec3(0.0f));
    // draw_block_with_shadows("lamp", m_point_light_position);

    // Add grid blocks only if not using instancing
    if (!m_use_instancing) {
        for (int i = 0; i < m_grid_size; ++i) {
            for (int j = 0; j < m_grid_size; ++j) {
                draw_block_with_shadows("mcblock", glm::vec3(i, -1.0f, j));
            }
        }
    }
    // int M = 3;
    // for (int i = 0; i < M; ++i) {
    //     for (int j = 0; j < M; ++j) {
    //         for (int k = 0; k < M; ++k) {
    //             if (k != 1)
    //                 draw_block_with_shadows("plank", glm::vec3(i+1, j, k+1));
    //             else if (k == 1) {
    //                 if (j == 2)
    //                     draw_block_with_shadows("plank", glm::vec3(i+1, j, k+1));
    //                 else if (i == 0) {
    //                     draw_block_with_shadows("plank", glm::vec3(i+1, j, k+1));
    //                 }
    //             }
    //         }
    //     }
    // }
    //
    //
    // int fSize = 4;
    // for (int i = 0; i < fSize; ++i) {
    //         draw_regular_model_with_shadows("fence", glm::vec3(1.33*i+4,0,1), glm::vec3(0.66f), glm::vec3(0,0,0));
    //
    // }


    draw_regular_model_with_shadows("house", m_house_position, m_house_scale, m_house_rotation);

    draw_regular_model_with_shadows("skin", m_skin_position, m_skin_scale, m_skin_rotation);

    draw_regular_model_with_shadows("tree", glm::vec3(13.0f,-1.5f,-2.0f),glm::vec3(3.5f), glm::vec3(0.0f));

    draw_regular_model_with_shadows("tree", glm::vec3(2.0f,-1.5f,12.0f),glm::vec3(3.5f), glm::vec3(0.0f));

    draw_regular_model_with_shadows("tree", glm::vec3(-1.0f,-1.5f,2.0f),glm::vec3(3.5f), glm::vec3(0.0f));

    draw_regular_model_with_shadows("cat", glm::vec3(0.5f,0.0f,4.0f),glm::vec3(0.05f), glm::vec3(0.0f));


    draw_regular_model_with_shadows("creature", glm::vec3(5.3f,0.3f,2.0f),glm::vec3(0.05f), glm::vec3(0.0f,60.0f,0.0f));



    shadow_controller->update_light_position("main_light", m_point_light_position);
    shadow_controller->begin_shadow_pass();
    shadow_controller->render_shadows_for_light("main_light", [this]() {
        render_all_shadows();
        if (m_use_instancing) {
            draw_instanced_grid_shadows();
        }
    });
    shadow_controller->end_shadow_pass();

    draw_skybox();
    render_all_scene_blocks();
    render_all_scene_regular_models();

    // Draw instanced grid in scene rendering
    if (m_use_instancing) {
        draw_instanced_grid();
    }
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    graphics->begin_gui();

    ImGui::Begin("Point Light Controls");
    ImGui::Text("Position");
    glm::vec3 old_position = m_point_light_position;
    ImGui::SliderFloat3("Position", &m_point_light_position.x, -10.0f, 20.0f);
    if (old_position != m_point_light_position) {
        shadow_controller->update_light_position("main_light", m_point_light_position);
    }

    ImGui::Separator();
    ImGui::Text("Colors");
    ImGui::ColorEdit3("Ambient##point", &m_point_light_ambient.x);
    ImGui::ColorEdit3("Diffuse##point", &m_point_light_diffuse.x);
    ImGui::ColorEdit3("Specular##point", &m_point_light_specular.x);

    ImGui::Separator();
    ImGui::Text("Attenuation");
    ImGui::SliderFloat("Constant", &m_point_light_constant, 0.1f, 2.0f);
    ImGui::SliderFloat("Linear", &m_point_light_linear, 0.001f, 1.0f);
    ImGui::SliderFloat("Quadratic", &m_point_light_quadratic, 0.001f, 1.0f);
    ImGui::End();

    ImGui::Begin("Moon Light Controls");
    ImGui::Text("Direction");
    ImGui::SliderFloat3("Direction", &m_moon_direction.x, -1.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("Colors");
    ImGui::ColorEdit3("Ambient##moon", &m_moon_ambient.x);
    ImGui::ColorEdit3("Diffuse##moon", &m_moon_diffuse.x);
    ImGui::ColorEdit3("Specular##moon", &m_moon_specular.x);
    ImGui::End();
    ImGui::Begin("Spotlight Controls");
    ImGui::Text("Spotlight follows camera direction");
    ImGui::Checkbox("Enable Spotlight (F)", &m_spotlight_enabled);

    if (m_spotlight_enabled) {
        ImGui::Separator();
        ImGui::Text("Colors");
        ImGui::ColorEdit3("Ambient##spot", &m_spotlight_ambient.x);
        ImGui::ColorEdit3("Diffuse##spot", &m_spotlight_diffuse.x);
        ImGui::ColorEdit3("Specular##spot", &m_spotlight_specular.x);

        ImGui::Separator();
        ImGui::Text("Attenuation");
        ImGui::SliderFloat("Constant##spot", &m_spotlight_constant, 0.1f, 2.0f);
        ImGui::SliderFloat("Linear##spot", &m_spotlight_linear, 0.001f, 1.0f);
        ImGui::SliderFloat("Quadratic##spot", &m_spotlight_quadratic, 0.001f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Shape");
        ImGui::SliderFloat("Inner Cutoff", &m_spotlight_cutoff, 5.0f, 45.0f);
        ImGui::SliderFloat("Outer Cutoff", &m_spotlight_outer_cutoff, 10.0f, 50.0f);

        if (m_spotlight_outer_cutoff < m_spotlight_cutoff) {
            m_spotlight_outer_cutoff = m_spotlight_cutoff + 1.0f;
        }
    }
    ImGui::End();

    ImGui::Begin("Shadow Controls");
    ImGui::Text("Shadow Settings");
    ImGui::SliderFloat("Shadow Strength", &m_shadow_strength, 0.0f, 2.0f);


    ImGui::Separator();
    ImGui::Text("Info");
    ImGui::Text("Lights: %zu", shadow_controller->get_light_count());
    if (auto params = shadow_controller->get_light_params("main_light")) {
        ImGui::Text("Shadow Resolution: %dx%d", params->resolution, params->resolution);
        ImGui::Text("Far Plane: %.1f", params->far_plane);
    }
    ImGui::End();

    ImGui::Begin("Skin Controls");
    ImGui::Text("Position");
    ImGui::SliderFloat3("Skin Position", &m_skin_position.x, -10.0f, 20.0f);

    ImGui::Separator();
    ImGui::Text("Scale");
    ImGui::SliderFloat3("Skin Scale", &m_skin_scale.x, 0.1f, 3.0f);

    if (ImGui::Button("Uniform Scale##skin")) {
        float avg_scale = (m_skin_scale.x + m_skin_scale.y + m_skin_scale.z) / 3.0f;
        m_skin_scale = glm::vec3(avg_scale);
    }

    ImGui::Separator();
    ImGui::Text("Rotation (degrees)");
    ImGui::SliderFloat3("Skin Rotation", &m_skin_rotation.x, -180.0f, 180.0f);

    ImGui::Separator();
    ImGui::End();


    ImGui::Begin("Instancing Controls");
    ImGui::Text("Grid Rendering Mode");
    bool instancing_changed = ImGui::Checkbox("Use Instancing", &m_use_instancing);

    if (instancing_changed && m_use_instancing) {
        setup_instanced_grid();
    }

    ImGui::Separator();
    ImGui::Text("Grid Configuration");

    int old_grid_size = m_grid_size;
    ImGui::SliderInt("Grid Size", &m_grid_size, 1, 100);

    if (old_grid_size != m_grid_size) {
        set_grid_size(m_grid_size);
    }

    ImGui::Separator();
    ImGui::Text("Total Grid Blocks: %d", m_grid_size * m_grid_size);
    ImGui::End();

    graphics->end_gui();

}

void MainController::draw_block_shadow(std::string block_name, const glm::mat4& model_matrix) {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    engine::resources::Model * block = resources->model("mcblock");

    const auto* depth_shader = shadow_controller->get_depth_shader();
    if (depth_shader) {
        depth_shader->set_mat4("model", model_matrix);

        block->draw(depth_shader);
    }
}

void MainController::draw_block_with_shadows(std::string block_name, glm::vec3 position) {
    glm::mat4 model_matrix = calculate_block_model_matrix(position);
    m_blocks_to_render.push_back({block_name, position, model_matrix});
}

void MainController::render_all_shadows() {
    for (const auto& block : m_blocks_to_render) {
        draw_block_shadow(block.model_name, block.model_matrix);
    }
    for (const auto& model : m_regular_models_to_render) {
        draw_regular_model_shadow(model.model_name, model.model_matrix);
    }
}

void MainController::render_all_scene_blocks() {
    for (const auto& block : m_blocks_to_render) {
        draw_block(block.model_name, block.model_matrix);
    }
}

void MainController::draw_regular_model(std::string model_name, glm::vec3 position) {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    engine::resources::Model * model = resources->model(model_name);
    engine::resources::Shader* shader = resources->shader("skin");

    shader->use();

    shader->set_int("texture_diffuse1", 0);
    shader->set_int("depthMap", 1);

    glm::mat4 modelMatrix = glm::mat4(1.0f);

    modelMatrix = glm::translate(modelMatrix, position);

    glm::mat4 view = graphics->camera()->view_matrix();
    glm::mat4 projection = graphics->projection_matrix();

    shader->set_mat4("model", modelMatrix);
    shader->set_mat4("view", view);
    shader->set_mat4("projection", projection);

    shader->set_vec3("viewPos", graphics->camera()->Position);

    shader->set_vec3("dirLight.direction", m_moon_direction);
    shader->set_vec3("dirLight.ambient", m_moon_ambient);
    shader->set_vec3("dirLight.diffuse", m_moon_diffuse);
    shader->set_vec3("dirLight.specular", m_moon_specular);

    shader->set_vec3("pointLight.position", m_point_light_position);
    shader->set_vec3("pointLight.ambient", m_point_light_ambient);
    shader->set_vec3("pointLight.diffuse", m_point_light_diffuse);
    shader->set_vec3("pointLight.specular", m_point_light_specular);
    shader->set_float("pointLight.constant", m_point_light_constant);
    shader->set_float("pointLight.linear", m_point_light_linear);
    shader->set_float("pointLight.quadratic", m_point_light_quadratic);

    if (m_spotlight_enabled) {
        auto camera = graphics->camera();
        shader->set_vec3("spotLight.position", camera->Position);
        shader->set_vec3("spotLight.direction", camera->Front);
        shader->set_vec3("spotLight.ambient", m_spotlight_ambient);
        shader->set_vec3("spotLight.diffuse", m_spotlight_diffuse);
        shader->set_vec3("spotLight.specular", m_spotlight_specular);
        shader->set_float("spotLight.constant", m_spotlight_constant);
        shader->set_float("spotLight.linear", m_spotlight_linear);
        shader->set_float("spotLight.quadratic", m_spotlight_quadratic);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(m_spotlight_cutoff)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(m_spotlight_outer_cutoff)));
    } else {
        shader->set_vec3("spotLight.position", glm::vec3(0.0f));
        shader->set_vec3("spotLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader->set_vec3("spotLight.ambient", glm::vec3(0.0f));
        shader->set_vec3("spotLight.diffuse", glm::vec3(0.0f));
        shader->set_vec3("spotLight.specular", glm::vec3(0.0f));
        shader->set_float("spotLight.constant", 1.0f);
        shader->set_float("spotLight.linear", 0.0f);
        shader->set_float("spotLight.quadratic", 0.0f);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(0.0f)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(0.0f)));
    }

    shadow_controller->bind_shadow_maps(shader, m_shadow_strength);

    shader->set_float("material.shininess", 32.0f);

    model->draw(shader);
}

void MainController::draw_regular_model(std::string model_name, const glm::mat4& model_matrix) {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    engine::resources::Model * model = resources->model(model_name);
    engine::resources::Shader* shader = resources->shader("skin");

    shader->use();

    shader->set_int("texture_diffuse1", 0);
    shader->set_int("depthMap", 1);

    glm::mat4 view = graphics->camera()->view_matrix();
    glm::mat4 projection = graphics->projection_matrix();

    shader->set_mat4("model", model_matrix);
    shader->set_mat4("view", view);
    shader->set_mat4("projection", projection);

    shader->set_vec3("viewPos", graphics->camera()->Position);

    shader->set_vec3("dirLight.direction", m_moon_direction);
    shader->set_vec3("dirLight.ambient", m_moon_ambient);
    shader->set_vec3("dirLight.diffuse", m_moon_diffuse);
    shader->set_vec3("dirLight.specular", m_moon_specular);

    shader->set_vec3("pointLight.position", m_point_light_position);
    shader->set_vec3("pointLight.ambient", m_point_light_ambient);
    shader->set_vec3("pointLight.diffuse", m_point_light_diffuse);
    shader->set_vec3("pointLight.specular", m_point_light_specular);
    shader->set_float("pointLight.constant", m_point_light_constant);
    shader->set_float("pointLight.linear", m_point_light_linear);
    shader->set_float("pointLight.quadratic", m_point_light_quadratic);

    if (m_spotlight_enabled) {
        auto camera = graphics->camera();
        shader->set_vec3("spotLight.position", camera->Position);
        shader->set_vec3("spotLight.direction", camera->Front);
        shader->set_vec3("spotLight.ambient", m_spotlight_ambient);
        shader->set_vec3("spotLight.diffuse", m_spotlight_diffuse);
        shader->set_vec3("spotLight.specular", m_spotlight_specular);
        shader->set_float("spotLight.constant", m_spotlight_constant);
        shader->set_float("spotLight.linear", m_spotlight_linear);
        shader->set_float("spotLight.quadratic", m_spotlight_quadratic);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(m_spotlight_cutoff)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(m_spotlight_outer_cutoff)));
    } else {
        shader->set_vec3("spotLight.position", glm::vec3(0.0f));
        shader->set_vec3("spotLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader->set_vec3("spotLight.ambient", glm::vec3(0.0f));
        shader->set_vec3("spotLight.diffuse", glm::vec3(0.0f));
        shader->set_vec3("spotLight.specular", glm::vec3(0.0f));
        shader->set_float("spotLight.constant", 1.0f);
        shader->set_float("spotLight.linear", 0.0f);
        shader->set_float("spotLight.quadratic", 0.0f);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(0.0f)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(0.0f)));
    }

    shadow_controller->bind_shadow_maps(shader, m_shadow_strength);

    shader->set_float("material.shininess", 32.0f);

    model->draw(shader);
}

void MainController::draw_regular_model_shadow(std::string model_name, const glm::mat4& model_matrix) {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    engine::resources::Model * model = resources->model(model_name);

    const auto* depth_shader = shadow_controller->get_depth_shader();
    if (depth_shader) {
        depth_shader->set_mat4("model", model_matrix);

        model->draw(depth_shader);
    }
}

void MainController::draw_regular_model_with_shadows(std::string model_name, glm::vec3 position) {

    glm::mat4 model_matrix = calculate_regular_model_matrix(position);
    model_matrix = glm::scale(model_matrix, glm::vec3(0.8f));
    model_matrix = glm::rotate(model_matrix, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    m_regular_models_to_render.push_back({model_name, position, model_matrix});
}

void MainController::draw_regular_model(std::string model_name, glm::vec3 position, glm::vec3 scale, glm::vec3 rotation) {
    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();

    engine::resources::Model* model = resources->model(model_name);
    engine::resources::Shader* shader = resources->shader("skin");

    shader->use();

    shader->set_int("texture_diffuse1", 0);
    shader->set_int("depthMap", 1);

    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::scale(modelMatrix, scale);

    modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 view = graphics->camera()->view_matrix();
    glm::mat4 projection = graphics->projection_matrix();

    shader->set_mat4("model", modelMatrix);
    shader->set_mat4("view", view);
    shader->set_mat4("projection", projection);

    shader->set_vec3("viewPos", graphics->camera()->Position);

    shader->set_vec3("dirLight.direction", m_moon_direction);
    shader->set_vec3("dirLight.ambient", m_moon_ambient);
    shader->set_vec3("dirLight.diffuse", m_moon_diffuse);
    shader->set_vec3("dirLight.specular", m_moon_specular);

    shader->set_vec3("pointLight.position", m_point_light_position);
    shader->set_vec3("pointLight.ambient", m_point_light_ambient);
    shader->set_vec3("pointLight.diffuse", m_point_light_diffuse);
    shader->set_vec3("pointLight.specular", m_point_light_specular);
    shader->set_float("pointLight.constant", m_point_light_constant);
    shader->set_float("pointLight.linear", m_point_light_linear);
    shader->set_float("pointLight.quadratic", m_point_light_quadratic);

    if (m_spotlight_enabled) {
        auto camera = graphics->camera();
        shader->set_vec3("spotLight.position", camera->Position);
        shader->set_vec3("spotLight.direction", camera->Front);
        shader->set_vec3("spotLight.ambient", m_spotlight_ambient);
        shader->set_vec3("spotLight.diffuse", m_spotlight_diffuse);
        shader->set_vec3("spotLight.specular", m_spotlight_specular);
        shader->set_float("spotLight.constant", m_spotlight_constant);
        shader->set_float("spotLight.linear", m_spotlight_linear);
        shader->set_float("spotLight.quadratic", m_spotlight_quadratic);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(m_spotlight_cutoff)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(m_spotlight_outer_cutoff)));
    } else {
        shader->set_vec3("spotLight.position", glm::vec3(0.0f));
        shader->set_vec3("spotLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader->set_vec3("spotLight.ambient", glm::vec3(0.0f));
        shader->set_vec3("spotLight.diffuse", glm::vec3(0.0f));
        shader->set_vec3("spotLight.specular", glm::vec3(0.0f));
        shader->set_float("spotLight.constant", 1.0f);
        shader->set_float("spotLight.linear", 0.0f);
        shader->set_float("spotLight.quadratic", 0.0f);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(0.0f)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(0.0f)));
    }

    shadow_controller->bind_shadow_maps(shader, m_shadow_strength);

    shader->set_float("material.shininess", 32.0f);

    model->draw(shader);
}

void MainController::draw_regular_model_with_shadows(std::string model_name, glm::vec3 position, glm::vec3 scale, glm::vec3 rotation) {
    glm::mat4 model_matrix = calculate_regular_model_matrix(position);
    model_matrix = glm::scale(model_matrix, scale);

    model_matrix = glm::rotate(model_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model_matrix = glm::rotate(model_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model_matrix = glm::rotate(model_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    m_regular_models_to_render.push_back({model_name, position, model_matrix});
}

void MainController::render_all_scene_regular_models() {
    for (const auto& model : m_regular_models_to_render) {
        draw_regular_model(model.model_name, model.model_matrix);
    }
}

glm::mat4 MainController::calculate_block_model_matrix(glm::vec3 position) {
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.5));
    return modelMatrix;
}

glm::mat4 MainController::calculate_regular_model_matrix(glm::vec3 position) {
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    return modelMatrix;
}

void MainController::setup_instanced_grid() {
    auto instancing = engine::core::Controller::get<engine::graphics::InstancingController>();

    auto transforms = std::make_shared<engine::graphics::InstancedTransforms>();

    for (int i = -2; i < m_grid_size; ++i) {
        for (int j = -2; j < m_grid_size; ++j) {
            glm::mat4 model_matrix = calculate_block_model_matrix(glm::vec3(i, -1.0f, j));
            transforms->add_transform(model_matrix);
        }
    }

    instancing->register_instance_group("grid_blocks", transforms);
}

void MainController::draw_instanced_grid() {
    if (!m_use_instancing) return;

    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto graphics = engine::core::Controller::get<engine::graphics::GraphicsController>();
    auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();
    auto instancing = engine::core::Controller::get<engine::graphics::InstancingController>();

    engine::resources::Model* block = resources->model("mcblock");
    engine::resources::Shader* shader = resources->shader("grass_instanced");

    shader->use();

    shader->set_int("texture_diffuse1", 0);
    shader->set_int("depthMap", 1);

    glm::mat4 view = graphics->camera()->view_matrix();
    glm::mat4 projection = graphics->projection_matrix();

    shader->set_mat4("view", view);
    shader->set_mat4("projection", projection);

    shader->set_vec3("viewPos", graphics->camera()->Position);

    shader->set_vec3("dirLight.direction", m_moon_direction);
    shader->set_vec3("dirLight.ambient", m_moon_ambient);
    shader->set_vec3("dirLight.diffuse", m_moon_diffuse);
    shader->set_vec3("dirLight.specular", m_moon_specular);

    shader->set_vec3("pointLight.position", m_point_light_position);
    shader->set_vec3("pointLight.ambient", m_point_light_ambient);
    shader->set_vec3("pointLight.diffuse", m_point_light_diffuse);
    shader->set_vec3("pointLight.specular", m_point_light_specular);
    shader->set_float("pointLight.constant", m_point_light_constant);
    shader->set_float("pointLight.linear", m_point_light_linear);
    shader->set_float("pointLight.quadratic", m_point_light_quadratic);

    if (m_spotlight_enabled) {
        auto camera = graphics->camera();
        shader->set_vec3("spotLight.position", camera->Position);
        shader->set_vec3("spotLight.direction", camera->Front);
        shader->set_vec3("spotLight.ambient", m_spotlight_ambient);
        shader->set_vec3("spotLight.diffuse", m_spotlight_diffuse);
        shader->set_vec3("spotLight.specular", m_spotlight_specular);
        shader->set_float("spotLight.constant", m_spotlight_constant);
        shader->set_float("spotLight.linear", m_spotlight_linear);
        shader->set_float("spotLight.quadratic", m_spotlight_quadratic);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(m_spotlight_cutoff)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(m_spotlight_outer_cutoff)));
    } else {
        shader->set_vec3("spotLight.position", glm::vec3(0.0f));
        shader->set_vec3("spotLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader->set_vec3("spotLight.ambient", glm::vec3(0.0f));
        shader->set_vec3("spotLight.diffuse", glm::vec3(0.0f));
        shader->set_vec3("spotLight.specular", glm::vec3(0.0f));
        shader->set_float("spotLight.constant", 1.0f);
        shader->set_float("spotLight.linear", 0.0f);
        shader->set_float("spotLight.quadratic", 0.0f);
        shader->set_float("spotLight.cutOff", glm::cos(glm::radians(0.0f)));
        shader->set_float("spotLight.outerCutOff", glm::cos(glm::radians(0.0f)));
    }

    shadow_controller->bind_shadow_maps(shader, m_shadow_strength);
    shader->set_float("material.shininess", 32.0f);

    for (const auto& mesh : block->meshes()) {
        instancing->setup_instanced_mesh("grid_blocks", &mesh, 5);
        instancing->draw_instanced_with_textures("grid_blocks", &mesh, shader);
    }
}

void MainController::draw_instanced_grid_shadows() {
    if (!m_use_instancing) return;

    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    auto instancing = engine::core::Controller::get<engine::graphics::InstancingController>();

    engine::resources::Model* block = resources->model("mcblock");

    try {
        engine::resources::Shader* shadow_shader = resources->shader("shadow_depth_instanced");
        shadow_shader->use();

        for (const auto& mesh : block->meshes()) {
            instancing->setup_instanced_mesh("grid_blocks", &mesh, 5);
            instancing->draw_instanced("grid_blocks", &mesh);
        }
    } catch (const std::exception& e) {
        auto shadow_controller = engine::core::Controller::get<engine::graphics::ShadowController>();
        const auto* depth_shader = shadow_controller->get_depth_shader();

        if (depth_shader && instancing->is_group_registered("grid_blocks")) {
            for (int i = -2; i < m_grid_size; ++i) {
                for (int j = -2; j < m_grid_size; ++j) {
                    glm::mat4 model_matrix = calculate_block_model_matrix(glm::vec3(i, -1.0f, j));
                    depth_shader->set_mat4("model", model_matrix);
                    block->draw(depth_shader);
                }
            }
        }
    }
}

void MainController::set_grid_size(int size) {
    m_grid_size = size;
    if (m_use_instancing) {
        setup_instanced_grid();
    }
}

void MainController::start_skin_animation() {
    if (m_animation_state != AnimationState::IDLE) {
        return;
    }

    m_skin_original_position = m_skin_position;
    m_skin_original_rotation = m_skin_rotation;

    m_animation_state = AnimationState::ROTATING_TO_ZERO;
    m_animation_timer = 0.0f;

    m_skin_target_rotation = glm::vec3(0.0f, 0.0f, 0.0f);
}

void MainController::update_skin_animation(float dt) {
    if (m_animation_state == AnimationState::IDLE) {
        return;
    }

    m_animation_timer += dt;
    float progress = m_animation_timer / m_animation_duration;

    switch (m_animation_state) {
        case AnimationState::ROTATING_TO_ZERO: {
            if (progress >= 1.0f) {
                m_skin_rotation = m_skin_target_rotation;
                m_animation_state = AnimationState::MOVING_FORWARD;
                m_animation_timer = 0.0f;

                glm::vec3 forward = glm::vec3(0.0f, 0.0f, 3.0f); // 3 units forward in Z
                m_skin_target_position = m_skin_position + forward;
            } else {
                m_skin_rotation = glm::mix(m_skin_original_rotation, m_skin_target_rotation, progress);
            }
            break;
        }

        case AnimationState::MOVING_FORWARD: {
            if (progress >= 1.0f) {
                m_skin_position = m_skin_target_position;
                m_animation_state = AnimationState::WAITING;
                m_animation_timer = 0.0f;
                // wait
                m_animation_duration = 10.0f;
            } else {
                m_skin_position = glm::mix(m_skin_original_position, m_skin_target_position, progress);
            }
            break;
        }

        case AnimationState::WAITING: {
            if (progress >= 1.0f) {
                m_animation_state = AnimationState::ROTATING_TO_LOOK_BACK;
                m_animation_timer = 0.0f;
                m_animation_duration = 2.0f;

                m_skin_target_rotation = glm::vec3(0.0f, 180.0f, 0.0f);
            }
            break;
        }

        case AnimationState::ROTATING_TO_LOOK_BACK: {
            glm::vec3 current_rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            if (progress >= 1.0f) {
                m_skin_rotation = m_skin_target_rotation;
                m_animation_state = AnimationState::MOVING_BACK;
                m_animation_timer = 0.0f;
            } else {
                m_skin_rotation = glm::mix(current_rotation, m_skin_target_rotation, progress);
            }
            break;
        }

        case AnimationState::MOVING_BACK: {
            if (progress >= 1.0f) {
                m_skin_position = m_skin_original_position;
                m_animation_state = AnimationState::ROTATING_TO_ORIGINAL;
                m_animation_timer = 0.0f;
                m_animation_duration = 2.0f;
            } else {
                m_skin_position = glm::mix(m_skin_target_position, m_skin_original_position, progress);
            }
            break;
        }

        case AnimationState::ROTATING_TO_ORIGINAL: {
            if (progress >= 1.0f) {
                m_skin_rotation = m_skin_original_rotation;
                m_animation_state = AnimationState::IDLE;
                m_animation_timer = 0.0f;
            } else {
                m_skin_rotation = glm::mix(m_skin_target_rotation, m_skin_original_rotation, progress);
            }
            break;
        }

        case AnimationState::IDLE:
            break;
    }
}

void MainController::terminate() {
    spdlog::info("{}::terminate", name());
}

}