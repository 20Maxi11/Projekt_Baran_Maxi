#ifndef GAME_HPP
#define GAME_HPP

#include <vector>
#include <optional>
#include "Ball.hpp"

class Game {
public:
    Game(double tableWidth, double tableHeight, int players = 2);

    void set_playfield(double left, double top, double right, double bottom);

    bool allStopped() const;
    bool isOver() const { return gameOver_; }
    int  currentPlayer() const;
    int  winner() const { return winnerTeam_; }
    int  numPlayers() const { return 2; }
    int  remainingSolids() const;
    int  remainingStripes() const;

    void reset(bool keepScores);
    void update();
    void beginShot();
    void notifyCueHitBall(int id);

    Ball& cue();
    const Ball& cue() const;
    const std::vector<Ball>& balls() const;

    double W, H;            // Fenstergröße (Info)
    double pocketR = 24.0;  // Taschengröße

private:
    // Spielbereich (Innenmaß des grünen Tisches)
    double L_ = 0, T_ = 0, R_ = 0, B_ = 0;

    double friction_ = 0.99;

    Ball cueBall_;
    std::vector<Ball> balls_;

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

    void placeTriangle();
    void step(Ball& b);
    void wall(Ball& b) const;          // <- jetzt const
    bool pocket(const Ball& b) const;
    void collide(Ball& a, Ball& b);
    void handleCollisions();
    void assignGroupsIfNeeded();
    void resolveTurnIfStopped();
    bool isOwnType(const Ball& b, int player) const;
};

#endif
