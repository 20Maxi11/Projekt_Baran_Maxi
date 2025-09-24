#ifndef GAMEWINDOW_HPP
#define GAMEWINDOW_HPP

#include <Gosu/Gosu.hpp>
#include <memory>
#include <string>
#include "Game.hpp"

struct RectF {
    double x{}, y{}, w{}, h{};
    bool contains(double px, double py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class GameWindow : public Gosu::Window {
public:
    GameWindow(unsigned width, unsigned height, int players = 2);

    void update() override;
    void draw() override;
    void button_down(Gosu::Button) override;
    void button_up(Gosu::Button) override;

private:
    enum class UiState { Start, Playing, Paused, GameOver };
    UiState state_ = UiState::Start;

    Game game_;

    // Zielhilfe / Stoß
    bool   dragging_ = false;
    double aimX_ = 0.0, aimY_ = 0.0;
    double power_ = 0.0;

    // Spielernamen
    std::string p1Name_ = "Spieler 1";
    std::string p2Name_ = "Spieler 2";
    int activeName_ = 0;

    // Fonts
    std::unique_ptr<Gosu::Font> font_, fontTitle_;
    double lastW_ = 0, lastH_ = 0;

    // Startscreen Felder
    RectF p1Box_, p2Box_;

    // Tisch-Geometrie (zentriert, 2:1)
    double tableX_ = 0, tableY_ = 0, tableW_ = 0, tableH_ = 0;

    // Layout / Hilfen
    double rail() const;         // Bandendicke relativ zur Tischgröße
    void ensureFonts();
    void layoutStartBoxes();
    void computeTableRect();     // berechnet tableX_/Y_/W_/H_

    // Zeichnen
    void drawCircle(double cx, double cy, double r, Gosu::Color c, double z = 1.0, int seg = 28);
    void drawAim();
    void drawHud();
    void drawStart();
    void drawPause();
    void drawGameOver();
    static void drawTextShadow(Gosu::Font& f, const std::string& s, double x, double y, double z, Gosu::Color col);

    // Aktionen
    void shootFromAim();
    void togglePause();
    void startMatch();

    // Namenseingabe ohne TextInput
    void handleNameKey(Gosu::Button b);
};

#endif
