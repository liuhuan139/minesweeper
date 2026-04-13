#include "MineSweeper.h"
#include <gtkmm/application.h>

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "com.example.minesweeper");
    MineSweeper window;
    return app->run(window);
}
