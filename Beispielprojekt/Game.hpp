#ifndef GAME_HPP
#define GAME_HPP

#include <vector>
#include <optional>
#include "Ball.hpp"

// *** globale Skalierung für die Ballgröße (Physik & damit auch Rendering) ***
inline constexpr double BALL_SIZE_MUL = 1.40;

struct PocketGeom { double x, y, r; };

class Game {
public:
    Game(double tableWidth, double tableHeight, int players = 2);

    void set_playfield(double left, double top, double right, double bottom);
    void set_pockets(std::vector<PocketGeom> pockets);

    bool allStopped() const;
    bool isOver() const { return gameOver_; }
    int  currentPlayer() const;
    int  winner() const { return winnerTeam_; }
    int  remainingSolids() const;
    int  remainingStripes() const;

    void reset(bool keepScores);
    void update();
    void beginShot();
    void notifyCueHitBall(int id);

    // Sofort bis zum Stillstand vorsimulieren (für SPACE / Timeout)
    void fastForwardToRest();

    Ball& cue();
    const Ball& cue() const;
    const std::vector<Ball>& balls() const;

    // Für HUD: ggf. Gruppe eines Spielers (VOLLE/HALBE), sonst leer
    std::optional<BallType> groupOfPlayer(int p) const { return playerGroup_[p]; }

    double W, H;
    double pocketR = 24.0;

private:
    double L_ = 0, T_ = 0, R_ = 0, B_ = 0;
    double friction_ = 0.984; // etwas stärker, damit Kugeln kürzer rollen

    Ball cueBall_;
    std::vector<Ball> balls_;
    std::vector<PocketGeom> pockets_;

    int  currentTeam_ = 0;
    bool gameOver_ = false;
    int  winnerTeam_ = -1;
    bool groupsAssigned_ = false;
    std::optional<BallType> playerGroup_[2];

    bool shotActive_ = false;
    bool anyMoving_ = false;
    bool lastMoving_ = false;
    std::optional<int> firstHitBallId_;

    // Auto-Vorspulen, falls Stoß zu lange dauert
    int  shotFrames_ = 0;
    static constexpr int kShotTimeoutFrames_ = 8 * 60; // ~8s @60fps

    struct TurnResult {
        bool foul = false;
        bool anyPocket = false;
        bool pocketedEight = false;
        bool scoredOwn = false;
        std::vector<int> pocketedIds;
    } turn_;

    static double baseBallRadius() { return 10.0; }
    static double defaultBallRadius() { return baseBallRadius() * BALL_SIZE_MUL; }

    void placeTriangle();
    void step(Ball& b);
    void wall(Ball& b) const;
    bool pocket(const Ball& b) const;
    void collide(Ball& a, Ball& b);
    void handleCollisions();
    void assignGroupsIfNeeded();
    void resolveTurnIfStopped();
    bool isOwnType(const Ball& b, int player) const;
};

#endif
