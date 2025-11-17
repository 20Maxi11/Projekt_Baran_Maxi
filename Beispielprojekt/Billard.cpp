#include <Gosu/Gosu.hpp>
#include <Gosu/AutoLink.hpp>
#include "GameWindow.hpp"
// Hauptprogramm: Erzeugt das Spielfenster und startet die Billard-Anwendung.

int main()
{
    // Vollbild-fix (Breite/Höhe werden durch Monitor bestimmt)
    GameWindow win(1280, 720, 2, /*fullscreen=*/true);
    win.show();
    return 0;
}
 