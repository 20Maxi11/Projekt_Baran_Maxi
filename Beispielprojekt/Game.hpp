#ifndef GAME_HPP
#define GAME_HPP

#include <vector>
#include <optional>
#include "Ball.hpp"

class Game {
public:
    Game(double tableWidth, double tableHeight, int players = 2);

    // Zustandsabfragen
    bool allStopped() const;
    bool isOver() const { return gameOver_; }
    int currentPlayer() const;
    int winner() const { return winnerTeam_; }
    int numPlayers() const { return numPlayers_; }
    int remainingSolids() const;
    int remainingStripes() const;

    // Game-Aktionen
    void reset(bool keepScores);
    void update();
    void beginShot();
    void notifyCueHitBall(int id);

    // Zugriff auf Kugeln (für Rendering)
    Ball& cue();
    const Ball& cue() const;
    const std::vector<Ball>& balls() const;

private:
    // Tischgröße und Physik
    double W, H;
    double friction_ = 0.99;
    double pocketR = 15.0;

    // Kugeln
    Ball cueBall_;
    std::vector<Ball> balls_;

    // Spielzustand
    int numPlayers_;
    int currentTeam_ = 0;
    int currentPlayerIndex_ = 0;
    bool gameOver_ = false;
    int winnerTeam_ = -1;
    bool groupsAssigned_ = false;
    std::optional<BallType> playerGroup_[2];
    bool shotActive_ = false;
    bool anyMoving_ = false;
    bool lastMoving_ = false;
    std::optional<int> firstHitBallId_;

    // Zur Verfolgung der Spieler innerhalb der Teams (bei 4 Spielern)
    int lastShooterTeam_[2] = { -1, -1 };

    struct TurnResult {
        bool foul = false;
        bool anyPocket = false;
        bool pocketedEight = false;
        bool scoredOwn = false;
        std::vector<int> pocketedIds;
    } turn_;

    // Hilfsfunktionen (intern)
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
