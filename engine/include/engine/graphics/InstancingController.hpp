#ifndef MATF_RG_PROJECT_INSTANCING_CONTROLLER_HPP
#define MATF_RG_PROJECT_INSTANCING_CONTROLLER_HPP

#include <engine/core/Controller.hpp>
#include <engine/util/Errors.hpp>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

namespace engine::resources {
class Shader;
class Mesh;
}

namespace engine::graphics {

struct InstancedData {
    virtual ~InstancedData() = default;
    virtual size_t size() const = 0;
    virtual const void* data() const = 0;
    virtual size_t stride() const = 0;
};

struct InstancedTransforms : public InstancedData {
    std::vector<glm::mat4> transforms;

    size_t size() const override { return transforms.size() * sizeof(glm::mat4); }
    const void* data() const override { return transforms.data(); }
    size_t stride() const override { return sizeof(glm::mat4); }

    void add_transform(const glm::mat4& transform) {
        transforms.push_back(transform);
    }

    void clear() {
        transforms.clear();
    }

    size_t count() const {
        return transforms.size();
    }
};

struct InstancedOffsets : public InstancedData {
    std::vector<glm::vec2> offsets;

    size_t size() const override { return offsets.size() * sizeof(glm::vec2); }
    const void* data() const override { return offsets.data(); }
    size_t stride() const override { return sizeof(glm::vec2); }

    void add_offset(const glm::vec2& offset) {
        offsets.push_back(offset);
    }

    void clear() {
        offsets.clear();
    }

    size_t count() const {
        return offsets.size();
    }
};

class InstancingController final : public core::Controller {
public:
    std::string_view name() const override;

    void register_instance_group(const std::string& group_name,
                                std::shared_ptr<InstancedData> instance_data);

    bool update_instance_group(const std::string& group_name,
                              std::shared_ptr<InstancedData> instance_data);

    bool unregister_instance_group(const std::string& group_name);

    bool is_group_registered(const std::string& group_name) const;

    size_t get_instance_count(const std::string& group_name) const;

    void setup_instanced_mesh(const std::string& group_name,
                             const resources::Mesh* mesh,
                             uint32_t attribute_start);

    void draw_instanced(const std::string& group_name, const resources::Mesh* mesh);

    void draw_instanced_with_textures(const std::string& group_name,
                                     const resources::Mesh* mesh,
                                     const resources::Shader* shader);

    static std::shared_ptr<InstancedOffsets> create_grid_offsets(
        uint32_t width, uint32_t height,
        float spacing,
        const glm::vec2& center_offset = glm::vec2(0.0f));

    static std::shared_ptr<InstancedTransforms> create_circular_transforms(
        uint32_t count,
        float radius,
        float offset,
        float scale_min = 0.05f,
        float scale_max = 0.25f);

private:
    struct InstanceGroup {
        std::shared_ptr<InstancedData> data;
        uint32_t buffer_id{0};
        bool needs_update{true};

        InstanceGroup() = default;
        explicit InstanceGroup(std::shared_ptr<InstancedData> d) : data(std::move(d)) {}

        InstanceGroup(const InstanceGroup&) = delete;
        InstanceGroup& operator=(const InstanceGroup&) = delete;
        InstanceGroup(InstanceGroup&&) = default;
        InstanceGroup& operator=(InstanceGroup&&) = default;
    };

    void initialize() override;
    void terminate() override;
    void create_instance_buffer(InstanceGroup& group);
    void update_instance_buffer(InstanceGroup& group);
    void destroy_instance_buffer(InstanceGroup& group);
    void setup_matrix_attributes(uint32_t attribute_start, uint32_t buffer_id);
    void setup_offset_attributes(uint32_t attribute_start, uint32_t buffer_id);

    std::unordered_map<std::string, InstanceGroup> m_instance_groups;
};

} // namespace engine::graphics

#endif // MATF_RG_PROJECT_INSTANCING_CONTROLLER_HPP
