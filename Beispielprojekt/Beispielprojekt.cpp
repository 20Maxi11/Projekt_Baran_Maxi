#include <Gosu/Gosu.hpp>
#include <Gosu/AutoLink.hpp>
#include "GameWindow.hpp"

int main(int argc, char* argv[]) {
    // Fenstergröße (Standard: 2 Spieler)
    unsigned width = 800, height = 600;
    int players = 2;
    if (argc > 1) {
        int p = std::atoi(argv[1]);
        if (p >= 2 && p <= 4) players = p;
    }
    GameWindow window(width, height, players);
    window.show();
    return 0;
}
