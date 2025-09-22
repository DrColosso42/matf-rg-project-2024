#ifndef PROJECT_APP_HPP
#define PROJECT_APP_HPP

#include <engine/core/App.hpp>

namespace app {

class App : public engine::core::App {
public:
    App() = default;
    ~App() override = default;

protected:
    void app_setup() override;
    int on_exit() override;
};

}

#endif