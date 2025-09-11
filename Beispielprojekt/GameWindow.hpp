#ifndef GAMEWINDOW_HPP
#define GAMEWINDOW_HPP

#include <Gosu/Gosu.hpp>
#include "Game.hpp"

class GameWindow : public Gosu::Window {
public:
    GameWindow(unsigned width, unsigned height, int players = 2);

    void update() override;
    void draw() override;
    void button_down(Gosu::Button button) override;
    void button_up(Gosu::Button button) override;

private:
    Game          game_;
    Gosu::Font    font_{ 18 };
    bool          dragging_ = false;
    double        aimX_ = 0.0, aimY_ = 0.0;
    double        power_ = 0.0;  // Stoﬂst‰rke 0..1

    // Hilfszeichenroutinen
    void drawCircle(double cx, double cy, double r, Gosu::Color color, double z = 1.0, int segments = 28);
    void drawAim();
    void shootFromAim();
};

#endif
