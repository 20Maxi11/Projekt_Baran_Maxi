#ifndef GAMEWINDOW_HPP
#define GAMEWINDOW_HPP

#include <Gosu/Gosu.hpp>
#include <memory>
#include <string>
#include <array>
#include "Game.hpp"

struct RectF {
    double x{}, y{}, w{}, h{};
    bool contains(double px, double py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class GameWindow : public Gosu::Window {
public:
    GameWindow(unsigned width, unsigned height, int players = 2, bool fullscreen = true);

    void update() override;
    void draw() override;
    void button_down(Gosu::Button) override;
    void button_up(Gosu::Button) override;

private:
    enum class UiState { Start, Playing, Paused, GameOver };
    UiState state_ = UiState::Start;

    Game game_;

    // Zielhilfe / Stoﬂ
    bool   dragging_ = false;
    double aimX_ = 0.0, aimY_ = 0.0;
    double power_ = 0.0;
    int    cueAnimFrames_ = 0;

    // Spielernamen (Startbild)
    std::string p1Name_ = "Spieler 1";
    std::string p2Name_ = "Spieler 2";
    int activeName_ = 0;
    RectF p1Box_, p2Box_;

    // Fonts
    std::unique_ptr<Gosu::Font> font_, fontTitle_;
    double lastW_ = 0, lastH_ = 0;

    // Tisch-Geometrie (zentriert, 2:1)
    double tableX_ = 0, tableY_ = 0, tableW_ = 0, tableH_ = 0;

    // Assets
    std::unique_ptr<Gosu::Image> felt_;      // Tisch (PNG)
    std::unique_ptr<Gosu::Image> cueImg_;    // Queue
    std::array<std::unique_ptr<Gosu::Image>, 16> ballImg_; // 0..15

    void loadAssets();
    static std::string ballFile(int id);

	// Layout / Hiferoutinen
	double rail() const;         // nur ‰uﬂere Umrandung
    void ensureFonts();
    void layoutStartBoxes();
    void computeTableRect();

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
    void handleNameKey(Gosu::Button b);

    // --- Bildanalyse & Mapping ---
    struct NormRect { double l = 0.08, t = 0.08, r = 0.92, b = 0.92; };
    NormRect feltNorm_;
    bool feltOk_ = false;

    struct NormPocket { double nx = 0.0, ny = 0.0, nr = 0.05; }; // normiert 0..1
    std::array<NormPocket, 6> pocketsNorm_{};

    void analyzeTableImage();       // PNG -> Normdaten
    void rebuildGameGeomFromNorm(); // Normdaten -> Bildschirm + Game
};

#endif
