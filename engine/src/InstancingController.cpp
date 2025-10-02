#include <engine/graphics/InstancingController.hpp>
#include <engine/graphics/OpenGL.hpp>
#include <engine/resources/Mesh.hpp>
#include <engine/resources/Texture.hpp>
#include <engine/resources/Shader.hpp>
#include <engine/util/Errors.hpp>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <random>
#include <unordered_map>

namespace engine::graphics {

std::string_view InstancingController::name() const {
    return "InstancingController";
}

void InstancingController::initialize() {
    spdlog::info("{}::initialize", name());
}

void InstancingController::terminate() {
    spdlog::info("{}::terminate", name());

    
    for (auto& [group_name, group] : m_instance_groups) {
        destroy_instance_buffer(group);
    }
    m_instance_groups.clear();
}

void InstancingController::register_instance_group(const std::string& group_name,
                                                  std::shared_ptr<InstancedData> instance_data) {
    
    if (auto it = m_instance_groups.find(group_name); it != m_instance_groups.end()) {
        destroy_instance_buffer(it->second);
        it->second = InstanceGroup(instance_data);
    } else {
        m_instance_groups[group_name] = InstanceGroup(instance_data);
    }

    auto& group = m_instance_groups[group_name];
    create_instance_buffer(group);
}

bool InstancingController::update_instance_group(const std::string& group_name,
                                                std::shared_ptr<InstancedData> instance_data) {
    auto it = m_instance_groups.find(group_name);
    if (it == m_instance_groups.end()) {
        return false;
    }

    it->second.data = instance_data;
    it->second.needs_update = true;
    update_instance_buffer(it->second);
    return true;
}

bool InstancingController::unregister_instance_group(const std::string& group_name) {
    auto it = m_instance_groups.find(group_name);
    if (it == m_instance_groups.end()) {
        return false;
    }

    destroy_instance_buffer(it->second);
    m_instance_groups.erase(it);
    return true;
}

bool InstancingController::is_group_registered(const std::string& group_name) const {
    return m_instance_groups.find(group_name) != m_instance_groups.end();
}

size_t InstancingController::get_instance_count(const std::string& group_name) const {
    auto it = m_instance_groups.find(group_name);
    if (it == m_instance_groups.end()) {
        return 0;
    }

    if (auto transforms = std::dynamic_pointer_cast<InstancedTransforms>(it->second.data)) {
        return transforms->count();
    } else if (auto offsets = std::dynamic_pointer_cast<InstancedOffsets>(it->second.data)) {
        return offsets->count();
    }

    return 0;
}

void InstancingController::setup_instanced_mesh(const std::string& group_name,
                                               const resources::Mesh* mesh,
                                               uint32_t attribute_start) {
    RG_GUARANTEE(mesh != nullptr, "Cannot setup instancing for null mesh");

    auto it = m_instance_groups.find(group_name);
    RG_GUARANTEE(it != m_instance_groups.end(),
                "Instance group '{}' is not registered", group_name);

    auto& group = it->second;

    if (group.buffer_id == 0) {
        create_instance_buffer(group);
    } else if (group.needs_update) {
        update_instance_buffer(group);
    }

    
    glBindVertexArray(mesh->vao());

    
    if (std::dynamic_pointer_cast<InstancedTransforms>(group.data)) {
        setup_matrix_attributes(attribute_start, group.buffer_id);
    } else if (std::dynamic_pointer_cast<InstancedOffsets>(group.data)) {
        setup_offset_attributes(attribute_start, group.buffer_id);
    } else {
        RG_GUARANTEE(false, "Unsupported instance data type for group '{}'", group_name);
    }

    glBindVertexArray(0);
}

void InstancingController::draw_instanced(const std::string& group_name, const resources::Mesh* mesh) {
    RG_GUARANTEE(mesh != nullptr, "Cannot draw null mesh");

    auto it = m_instance_groups.find(group_name);
    RG_GUARANTEE(it != m_instance_groups.end(),
                "Instance group '{}' is not registered", group_name);

    const auto instance_count = get_instance_count(group_name);
    if (instance_count == 0) {
        spdlog::warn("InstancingController: No instances to draw for group '{}'", group_name);
        return;
    }

    glBindVertexArray(mesh->vao());

    if (mesh->num_indices() > 0) {
        glDrawElementsInstanced(GL_TRIANGLES, mesh->num_indices(), GL_UNSIGNED_INT, 0, instance_count);
    } else {
        spdlog::warn("InstancingController: Mesh without indices not fully supported for instancing");
    }

    glBindVertexArray(0);
}

void InstancingController::draw_instanced_with_textures(const std::string& group_name,
                                                       const resources::Mesh* mesh,
                                                       const resources::Shader* shader) {
    RG_GUARANTEE(mesh != nullptr, "Cannot draw null mesh");
    RG_GUARANTEE(shader != nullptr, "Cannot draw with null shader");

    auto it = m_instance_groups.find(group_name);
    RG_GUARANTEE(it != m_instance_groups.end(),
                "Instance group '{}' is not registered", group_name);

    const auto instance_count = get_instance_count(group_name);
    if (instance_count == 0) {
        spdlog::warn("InstancingController: No instances to draw for group '{}'", group_name);
        return;
    }

    std::unordered_map<std::string_view, uint32_t> counts;
    std::string uniform_name;
    uniform_name.reserve(32);

    const auto& textures = mesh->textures();
    for (int i = 0; i < textures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        const auto &texture_type = resources::Texture::uniform_name_convention(textures[i]->type());
        uniform_name.append(texture_type);
        const auto count = (counts[texture_type] += 1);
        uniform_name.append(std::to_string(count));
        shader->set_int(uniform_name, i);
        glBindTexture(GL_TEXTURE_2D, textures[i]->id());
        uniform_name.clear();
    }

    glBindVertexArray(mesh->vao());

    if (mesh->num_indices() > 0) {
        glDrawElementsInstanced(GL_TRIANGLES, mesh->num_indices(), GL_UNSIGNED_INT, 0, instance_count);
    } else {
        spdlog::warn("InstancingController: Mesh without indices not fully supported for instancing");
    }

    glBindVertexArray(0);
}

std::shared_ptr<InstancedOffsets> InstancingController::create_grid_offsets(
    uint32_t width, uint32_t height,
    float spacing,
    const glm::vec2& center_offset) {

    auto offsets = std::make_shared<InstancedOffsets>();

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            glm::vec2 offset;
            offset.x = (static_cast<float>(x) - static_cast<float>(width) * 0.5f) * spacing + center_offset.x;
            offset.y = (static_cast<float>(y) - static_cast<float>(height) * 0.5f) * spacing + center_offset.y;
            offsets->add_offset(offset);
        }
    }

    return offsets;
}

std::shared_ptr<InstancedTransforms> InstancingController::create_circular_transforms(
    uint32_t count,
    float radius,
    float offset,
    float scale_min,
    float scale_max) {

    auto transforms = std::make_shared<InstancedTransforms>();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> offset_dist(-offset, offset);
    std::uniform_real_distribution<float> scale_dist(scale_min, scale_max);
    std::uniform_real_distribution<float> rotation_dist(0.0f, 360.0f);

    for (uint32_t i = 0; i < count; ++i) {
        glm::mat4 model = glm::mat4(1.0f);

        float angle = (static_cast<float>(i) / static_cast<float>(count)) * 360.0f;
        float displacement_x = offset_dist(gen);
        float displacement_y = offset_dist(gen);
        float displacement_z = offset_dist(gen);

        float x = sin(glm::radians(angle)) * radius + displacement_x;
        float y = displacement_y * 0.4f;
        float z = cos(glm::radians(angle)) * radius + displacement_z;

        model = glm::translate(model, glm::vec3(x, y, z));

        float scale = scale_dist(gen);
        model = glm::scale(model, glm::vec3(scale));

        float rot_angle = glm::radians(rotation_dist(gen));
        model = glm::rotate(model, rot_angle, glm::vec3(0.4f, 0.6f, 0.8f));

        transforms->add_transform(model);
    }

    return transforms;
}

void InstancingController::create_instance_buffer(InstanceGroup& group) {
    RG_GUARANTEE(group.data != nullptr, "Cannot create buffer for null instance data");

    glGenBuffers(1, &group.buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, group.buffer_id);

    glBufferData(GL_ARRAY_BUFFER, group.data->size(), group.data->data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    group.needs_update = false;
}

void InstancingController::update_instance_buffer(InstanceGroup& group) {
    if (!group.needs_update || group.buffer_id == 0) {
        return;
    }

    RG_GUARANTEE(group.data != nullptr, "Cannot update buffer for null instance data");

    glBindBuffer(GL_ARRAY_BUFFER, group.buffer_id);
    glBufferData(GL_ARRAY_BUFFER, group.data->size(), group.data->data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    group.needs_update = false;
}

void InstancingController::destroy_instance_buffer(InstanceGroup& group) {
    if (group.buffer_id != 0) {
        glDeleteBuffers(1, &group.buffer_id);
        group.buffer_id = 0;
    }
}

void InstancingController::setup_matrix_attributes(uint32_t attribute_start, uint32_t buffer_id) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id);

    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t location = attribute_start + i;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                             (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(location, 1);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancingController::setup_offset_attributes(uint32_t attribute_start, uint32_t buffer_id) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id);

    glEnableVertexAttribArray(attribute_start);
    glVertexAttribPointer(attribute_start, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glVertexAttribDivisor(attribute_start, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} 
