#include <engine/graphics/ShadowController.hpp>
#include <engine/graphics/OpenGL.hpp>
#include <engine/resources/ResourcesController.hpp>
#include <engine/resources/Shader.hpp>
#include <engine/util/Errors.hpp>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

namespace engine::graphics {

namespace {
    
    std::vector<glm::mat4> create_shadow_transforms(const glm::vec3& light_pos, float near_plane, float far_plane) {
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, near_plane, far_plane);
        std::vector<glm::mat4> shadowTransforms;
        shadowTransforms.push_back(shadowProj * glm::lookAt(light_pos, light_pos + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(light_pos, light_pos + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(light_pos, light_pos + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(light_pos, light_pos + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(light_pos, light_pos + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(light_pos, light_pos + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
        return shadowTransforms;
    }
}

std::string_view ShadowController::name() const {
    return "ShadowController";
}

void ShadowController::initialize() {
    spdlog::info("{}::initialize", name());
    
}

void ShadowController::terminate() {
    spdlog::info("{}::terminate", name());

    
    for (auto& [light_name, shadow] : m_point_lights) {
        destroy_shadow_resources(shadow);
    }
    m_point_lights.clear();

    
    m_depth_shader = nullptr;
}

void ShadowController::register_point_light(const std::string& light_name,
                                           const PointLightShadowParams& params) {
    
    if (auto it = m_point_lights.find(light_name); it != m_point_lights.end()) {
        destroy_shadow_resources(it->second);
        it->second = PointLightShadow(params);
    } else {
        m_point_lights[light_name] = PointLightShadow(params);
    }

    auto& shadow = m_point_lights[light_name];
    
    update_shadow_transforms(shadow);
}

bool ShadowController::unregister_point_light(const std::string& light_name) {
    auto it = m_point_lights.find(light_name);
    if (it == m_point_lights.end()) {
        return false;
    }

    destroy_shadow_resources(it->second);
    m_point_lights.erase(it);
    return true;
}

bool ShadowController::update_light_position(const std::string& light_name,
                                            const glm::vec3& position) {
    auto it = m_point_lights.find(light_name);
    if (it == m_point_lights.end()) {
        return false;
    }

    it->second.params.position = position;
    update_shadow_transforms(it->second);
    return true;
}

bool ShadowController::update_light_far_plane(const std::string& light_name,
                                             float far_plane) {
    auto it = m_point_lights.find(light_name);
    if (it == m_point_lights.end()) {
        return false;
    }

    it->second.params.far_plane = far_plane;
    update_shadow_transforms(it->second);
    return true;
}

const PointLightShadowParams* ShadowController::get_light_params(const std::string& light_name) const {
    auto it = m_point_lights.find(light_name);
    return it != m_point_lights.end() ? &it->second.params : nullptr;
}

bool ShadowController::is_light_registered(const std::string& light_name) const {
    return m_point_lights.find(light_name) != m_point_lights.end();
}

void ShadowController::begin_shadow_pass() {
    
    glGetIntegerv(GL_VIEWPORT, m_state_backup.viewport);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&m_state_backup.framebuffer));

    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void ShadowController::render_shadows_for_light(const std::string& light_name,
                                               std::function<void()> render_geometry) {
    auto it = m_point_lights.find(light_name);
    RG_GUARANTEE(it != m_point_lights.end(),
                "Light '{}' is not registered for shadow mapping", light_name);

    auto& shadow = it->second;
    m_current_rendering_light = light_name;

    
    if (shadow.framebuffer == 0) {
        create_shadow_resources(shadow);
    }

    
    glViewport(0, 0, shadow.params.resolution, shadow.params.resolution);

    
    glBindFramebuffer(GL_FRAMEBUFFER, shadow.framebuffer);
    glClear(GL_DEPTH_BUFFER_BIT);

    
    ensure_depth_shader();
    if (!m_depth_shader) {
        spdlog::error("ShadowController: Depth shader is null!");
        return;
    }

    m_depth_shader->use();

    
    for (unsigned int i = 0; i < 6; ++i) {
        std::string uniform_name = "shadowMatrices[" + std::to_string(i) + "]";
        m_depth_shader->set_mat4(uniform_name, shadow.shadow_transforms[i]);
    }
    m_depth_shader->set_float("far_plane", shadow.params.far_plane);
    m_depth_shader->set_vec3("lightPos", shadow.params.position);

    
    render_geometry();

    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowController::end_shadow_pass() {
    
    glViewport(m_state_backup.viewport[0], m_state_backup.viewport[1],
               m_state_backup.viewport[2], m_state_backup.viewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, m_state_backup.framebuffer);

    m_current_rendering_light.clear();
}

void ShadowController::bind_shadow_maps(const resources::Shader* shader, float shadow_strength) {
    RG_GUARANTEE(shader != nullptr, "Cannot bind shadow maps to null shader");

    for (const auto& [light_name, shadow] : m_point_lights) {
        if (shadow.depth_cubemap == 0) {
            spdlog::warn("ShadowController: Shadow resources not created for light '{}'", light_name);
            continue;
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, shadow.depth_cubemap);

        try {
            shader->set_int("depthMap", 1);
        } catch (const std::exception& e) {
            spdlog::error("ShadowController: Failed to set depthMap uniform: {}", e.what());
        }

        try {
            shader->set_vec3("lightPos", shadow.params.position);
        } catch (const std::exception& e) {
            spdlog::error("ShadowController: Failed to set lightPos uniform: {}", e.what());
        }

        try {
            shader->set_float("far_plane", shadow.params.far_plane);
        } catch (const std::exception& e) {
            spdlog::error("ShadowController: Failed to set far_plane uniform: {}", e.what());
        }

        try {
            shader->set_float("shadowStrength", shadow_strength);
        } catch (const std::exception& e) {
            spdlog::error("ShadowController: Failed to set shadowStrength uniform: {}", e.what());
        }

        break;
    }
}

size_t ShadowController::get_light_count() const {
    return m_point_lights.size();
}

const resources::Shader* ShadowController::get_depth_shader() const {
    return m_depth_shader;
}

void ShadowController::create_shadow_resources(PointLightShadow& shadow) {
    const auto resolution = shadow.params.resolution;

    glGenTextures(1, &shadow.depth_cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadow.depth_cubemap);

    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                     resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }

    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    
    glGenFramebuffers(1, &shadow.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow.framebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow.depth_cubemap, 0);

    
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    
    RG_GUARANTEE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "Shadow framebuffer is not complete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void ShadowController::destroy_shadow_resources(PointLightShadow& shadow) {
    
    if (shadow.depth_cubemap != 0) {
        glDeleteTextures(1, &shadow.depth_cubemap);
        shadow.depth_cubemap = 0;
    }
    if (shadow.framebuffer != 0) {
        glDeleteFramebuffers(1, &shadow.framebuffer);
        shadow.framebuffer = 0;
    }
}

void ShadowController::update_shadow_transforms(PointLightShadow& shadow) {
    const auto& pos = shadow.params.position;
    const float near_plane = shadow.params.near_plane;
    const float far_plane = shadow.params.far_plane;

    auto transforms = create_shadow_transforms(pos, near_plane, far_plane);
    for (size_t i = 0; i < 6; ++i) {
        shadow.shadow_transforms[i] = transforms[i];
    }
}

void ShadowController::ensure_depth_shader() {
    if (m_depth_shader) {
        return; 
    }

    auto resources = core::Controller::get<resources::ResourcesController>();

    try {
        m_depth_shader = resources->shader("shadow_depth");
    } catch (const std::exception& e) {
        RG_GUARANTEE(false, "Failed to load shadow depth shader: {}", e.what());
    }
}

} 
