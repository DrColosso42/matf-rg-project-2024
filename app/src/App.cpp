#include <App.hpp>
#include <MainController.hpp>
#include <engine/graphics/ShadowController.hpp>
#include <engine/graphics/InstancingController.hpp>
#include <engine/resources/ResourcesController.hpp>
#include <spdlog/spdlog.h>

namespace app {

void App::app_setup() {
    spdlog::info("Application is being set up...");
    auto shadow_controller = register_controller<engine::graphics::ShadowController>();
    auto instancing_controller = register_controller<engine::graphics::InstancingController>();
    auto main_controller = register_controller<MainController>();

    auto resources = engine::core::Controller::get<engine::resources::ResourcesController>();
    resources->before(shadow_controller);
    resources->before(instancing_controller);
    shadow_controller->before(main_controller);
    instancing_controller->before(main_controller);
}

int App::on_exit() {
    spdlog::info("Application exiting...");
    return 0;
}

}
