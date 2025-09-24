#include <Gosu/Gosu.hpp>
#include <Gosu/AutoLink.hpp>
#include "GameWindow.hpp"

// Aufruf (optional): spiel.exe [breite] [höhe]
int main(int argc, char* argv[]) {
    unsigned w = 1000, h = 600;
    if (argc >= 3) {
        w = std::max(600, std::atoi(argv[1]));
        h = std::max(400, std::atoi(argv[2]));
    }
    GameWindow window(w, h, 2);
    window.show();
    return 0;
}
