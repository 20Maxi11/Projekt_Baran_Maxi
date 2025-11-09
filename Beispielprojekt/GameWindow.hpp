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

    bool   dragging_ = false;
    double aimX_ = 0.0, aimY_ = 0.0;
    double power_ = 0.0;
    int    cueAnimFrames_ = 0;
    double cueBackDist_ = 0.0;
    double cuePullStart_ = 0.0;

    std::string p1Name_ = "Spieler 1";
    std::string p2Name_ = "Spieler 2";
    int activeName_ = 0;
    RectF p1Box_, p2Box_;

    std::unique_ptr<Gosu::Font> font_, fontTitle_;
    double lastW_ = 0, lastH_ = 0;

    double tableX_ = 0, tableY_ = 0, tableW_ = 0, tableH_ = 0;

    std::unique_ptr<Gosu::Image> felt_;
    std::unique_ptr<Gosu::Image> cueImg_;
    std::array<std::unique_ptr<Gosu::Image>, 16> ballImg_;

    // --- Sound-Samples ---
    std::unique_ptr<Gosu::Sample> sfxBall_;
    std::unique_ptr<Gosu::Sample> sfxPocket_;
    std::unique_ptr<Gosu::Sample> sfxRail_;
    std::unique_ptr<Gosu::Sample> sfxWin_;
    double sfxVolume_ = 1.0;

    void loadAssets();
    void loadSounds();
    static std::string ballFile(int id);

    double rail() const;
    void ensureFonts();
    void layoutStartBoxes();
    void computeTableRect();

    void drawCircle(double cx, double cy, double r, Gosu::Color c, double z = 1.0, int seg = 28);
    static void drawTextShadow(Gosu::Font& f, const std::string& s, double x, double y, double z, Gosu::Color col);

    void drawAim();
    void drawHud();
    void drawStart();
    void drawPause();
    void drawGameOver();

    void shootFromAim();
    void togglePause();
    void startMatch();
    void handleNameKey(Gosu::Button b);

    // PNG : Spielfeld
    struct NormRect { double l = 0.08, t = 0.08, r = 0.92, b = 0.92; };
    NormRect feltNorm_;
    bool feltOk_ = false;
    void analyzeTableImage();
    void rebuildGameGeomFromNorm();

    // Sound-Events aus Game abspielen
    void playSoundEvents();
};

#endif
