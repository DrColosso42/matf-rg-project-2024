#ifndef MATF_RG_PROJECT_SHADOW_CONTROLLER_HPP
#define MATF_RG_PROJECT_SHADOW_CONTROLLER_HPP

#include <engine/core/Controller.hpp>
#include <engine/util/Errors.hpp>
#include <glm/glm.hpp>
#include <unordered_map>
#include <array>
#include <functional>
#include <memory>

namespace engine::resources {
class Shader;
}

namespace engine::graphics {

struct PointLightShadowParams {
    glm::vec3 position{0.0f};
    float far_plane{25.0f};
    uint32_t resolution{1024};
    float near_plane{1.0f};
};

class ShadowController final : public core::Controller {
public:
    std::string_view name() const override;
    void register_point_light(const std::string& light_name, const PointLightShadowParams& params);
    bool unregister_point_light(const std::string& light_name);
    bool update_light_position(const std::string& light_name, const glm::vec3& position);
    bool update_light_far_plane(const std::string& light_name, float far_plane);
    const PointLightShadowParams* get_light_params(const std::string& light_name) const;
    bool is_light_registered(const std::string& light_name) const;
    void begin_shadow_pass();
    void render_shadows_for_light(const std::string& light_name,
                                  std::function<void()> render_geometry);
    void end_shadow_pass();
    void bind_shadow_maps(const resources::Shader* shader, float shadow_strength = 1.0f);
    size_t get_light_count() const;
    const resources::Shader* get_depth_shader() const;

private:
    struct PointLightShadow {
        PointLightShadowParams params;
        uint32_t depth_cubemap{0};
        uint32_t framebuffer{0};
        std::array<glm::mat4, 6> shadow_transforms;

        PointLightShadow() = default;
        PointLightShadow(const PointLightShadowParams& p) : params(p) {}

        PointLightShadow(const PointLightShadow&) = delete;
        PointLightShadow& operator=(const PointLightShadow&) = delete;
        PointLightShadow(PointLightShadow&&) = default;
        PointLightShadow& operator=(PointLightShadow&&) = default;
    };

    void initialize() override;
    void terminate() override;
    void create_shadow_resources(PointLightShadow& shadow);
    void destroy_shadow_resources(PointLightShadow& shadow);
    void update_shadow_transforms(PointLightShadow& shadow);
    void ensure_depth_shader();

    std::unordered_map<std::string, PointLightShadow> m_point_lights;
    resources::Shader* m_depth_shader{nullptr};

    struct StateBackup {
        int viewport[4];
        uint32_t framebuffer;
    } m_state_backup;

    std::string m_current_rendering_light;
    uint32_t m_next_texture_unit{5};
};

} // namespace engine::graphics

#endif // MATF_RG_PROJECT_SHADOW_CONTROLLER_HPP