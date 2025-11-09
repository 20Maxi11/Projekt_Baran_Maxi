#ifndef GAME_HPP
#define GAME_HPP

#include <vector>
#include <optional>
#include "Ball.hpp"

// *** globale Skalierung für die Ballgröße (Physik & damit auch Rendering) ***
inline constexpr double BALL_SIZE_MUL = 1.40;

struct PocketGeom { double x, y, r; };

// ---- Sounds, die die Physik an das Window meldet ----
enum class SoundEventType {
    BallBall,
    RailHit,
    Pocket,
    GameOver
};

struct SoundEvent {
    SoundEventType type;
    double volume;      // 0..1
};

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

    // fürs Rendering
    Ball& cue();
    const Ball& cue() const;
    const std::vector<Ball>& balls() const;

    // für Window
    double W, H;
    double pocketR = 24.0;

    // Sound-Events abholen
    const std::vector<SoundEvent>& soundEvents() const { return soundEvents_; }
    void clearSoundEvents() { soundEvents_.clear(); }

    // damit Leertaste sofort alles fertig rechnet
    void fastForwardToRest();

    // für HUD (du nutzt das im Window)
    bool groupsAssigned() const { return groupsAssigned_; }
    std::optional<BallType> groupOfPlayer(int p) const {
        if (p < 0 || p > 1) return std::nullopt;
        return playerGroup_[p];
    }

private:
    double L_ = 0, T_ = 0, R_ = 0, B_ = 0;
    double friction_ = 0.99;

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

    struct TurnResult {
        bool foul = false;
        bool anyPocket = false;
        bool pocketedEight = false;
        bool scoredOwn = false;
        std::vector<int> pocketedIds;
    } turn_;

    // Sound-Puffer
    std::vector<SoundEvent> soundEvents_;

    static double baseBallRadius() { return 10.0; }
    static double defaultBallRadius() { return baseBallRadius() * BALL_SIZE_MUL; }

    void placeTriangle();
    void step(Ball& b);
    void wall(Ball& b);
    bool pocket(const Ball& b) const;
    void collide(Ball& a, Ball& b);
    void handleCollisions();
    void assignGroupsIfNeeded();
    void resolveTurnIfStopped();
    bool isOwnType(const Ball& b, int player) const;
};

#endif
