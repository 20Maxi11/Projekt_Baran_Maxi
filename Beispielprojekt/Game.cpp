#include "Game.hpp"
#include <algorithm>
#include <cmath>

static inline double len(double x, double y) { return std::sqrt(x * x + y * y); }

Game::Game(double tableWidth, double tableHeight, int /*players*/)
    : W(tableWidth), H(tableHeight),
    cueBall_(0, BallType::CUE, W * 0.22, H * 0.5, 10.0) {
    set_playfield(0, 0, W, H);
    reset(false);
}

void Game::set_playfield(double left, double top, double right, double bottom) {
    L_ = left; T_ = top; R_ = right; B_ = bottom;
    pocketR = std::max(cueBall_.r * 1.6, 20.0);
}

static BallType typeById(int id) {
    if (id == 8) return BallType::EIGHT;
    if (id >= 1 && id <= 7) return BallType::SOLID;
    return BallType::STRIPE; // 9..15
}

void Game::reset(bool keepScores) {
    gameOver_ = false; winnerTeam_ = -1;
    groupsAssigned_ = false;
    playerGroup_[0].reset(); playerGroup_[1].reset();
    turn_ = {};
    firstHitBallId_.reset();
    shotActive_ = false;

    if (!keepScores) currentTeam_ = 0;

    double Wp = R_ - L_, Hp = B_ - T_;
    cueBall_.x = L_ + Wp * 0.22; cueBall_.y = T_ + Hp * 0.5;
    cueBall_.vx = cueBall_.vy = 0; cueBall_.inPlay = true;

    balls_.clear();
    placeTriangle();
}

void Game::placeTriangle() {
    double Wp = R_ - L_, Hp = B_ - T_;
    const double d = cueBall_.r * 2.05;
    double sx = L_ + Wp * 0.68, sy = T_ + Hp * 0.5;

    // 8-Ball Aufbau (1 vorn, 8 in Mitte, Ecken: 2 und 14)
    int ids[15] = {
        1,
        10, 4,
        3, 8, 12,
        15, 6, 11, 5,
        2, 13, 9, 7, 14
    };

    int k = 0;
    for (int row = 0; row < 5; ++row) {
        for (int i = 0; i <= row; ++i) {
            int id = ids[k++];
            double x = sx + row * d * std::sqrt(3.0) / 2.0;
            double y = sy + (i - row * 0.5) * d;
            balls_.emplace_back(id, typeById(id), x, y, 10.0);
        }
    }
}

bool Game::allStopped() const {
    auto slow = [](const Ball& b) { return len(b.vx, b.vy) < 0.08; };
    if (cueBall_.inPlay && !slow(cueBall_)) return false;
    for (const auto& b : balls_) if (b.inPlay && !slow(b)) return false;
    return true;
}

int Game::remainingSolids() const {
    int n = 0; for (const auto& b : balls_) if (b.inPlay && b.type == BallType::SOLID) ++n; return n;
}
int Game::remainingStripes() const {
    int n = 0; for (const auto& b : balls_) if (b.inPlay && b.type == BallType::STRIPE) ++n; return n;
}

void Game::beginShot() { shotActive_ = true; turn_ = {}; firstHitBallId_.reset(); }
void Game::notifyCueHitBall(int id) { if (!firstHitBallId_.has_value()) firstHitBallId_ = id; }

bool Game::isOwnType(const Ball& b, int player) const {
    if (!groupsAssigned_) return false;
    if (b.type != BallType::SOLID && b.type != BallType::STRIPE) return false;
    return playerGroup_[player].has_value() && playerGroup_[player].value() == b.type;
}

void Game::step(Ball& b) {
    if (!b.inPlay) return;
    b.x += b.vx; b.y += b.vy;
    b.vx *= friction_; b.vy *= friction_;
    if (len(b.vx, b.vy) < 0.08) { b.vx = b.vy = 0; }
    if (!pocket(b)) wall(b);
}

void Game::wall(Ball& b) const {
    // Abprall an Innenkante
    if (b.x - b.r < L_) { b.x = L_ + b.r; b.vx = -b.vx; }
    if (b.x + b.r > R_) { b.x = R_ - b.r; b.vx = -b.vx; }
    if (b.y - b.r < T_) { b.y = T_ + b.r; b.vy = -b.vy; }
    if (b.y + b.r > B_) { b.y = B_ - b.r; b.vy = -b.vy; }
}

bool Game::pocket(const Ball& b) const {
    struct P { double x, y; };
    P p[6] = {
        {L_,T_},{R_,T_},{L_,B_},{R_,B_},{(L_ + R_) / 2.0,T_},{(L_ + R_) / 2.0,B_}
    };
    for (auto& q : p) {
        if (len(b.x - q.x, b.y - q.y) <= pocketR + b.r * 0.35) return true;
    }
    return false;
}

void Game::collide(Ball& a, Ball& b) {
    if (!a.inPlay || !b.inPlay) return;
    double dx = b.x - a.x, dy = b.y - a.y;
    double rr = a.r + b.r;
    double dist2 = dx * dx + dy * dy;
    if (dist2 <= 0 || dist2 >= rr * rr) return;

    double d = std::sqrt(dist2);
    double nx = dx / d, ny = dy / d;

    // Überlappung auseinander schieben
    double overlap = rr - d;
    a.x -= nx * overlap / 2; a.y -= ny * overlap / 2;
    b.x += nx * overlap / 2; b.y += ny * overlap / 2;

    // Geschwindigkeiten in Normal-/Tangentialanteil zerlegen
    double vaN = a.vx * nx + a.vy * ny;
    double vbN = b.vx * nx + b.vy * ny;
    double tx = -ny, ty = nx;
    double vaT = a.vx * tx + a.vy * ty;
    double vbT = b.vx * tx + b.vy * ty;

    // Elastischer Stoß gleicher Massen: Normalanteile tauschen
    double vaNn = vbN, vbNn = vaN;
    a.vx = vaNn * nx + vaT * tx; a.vy = vaNn * ny + vaT * ty;
    b.vx = vbNn * nx + vbT * tx; b.vy = vbNn * ny + vbT * ty;

    if (shotActive_) {
        if (a.type == BallType::CUE && b.type != BallType::CUE) notifyCueHitBall(b.id);
        else if (b.type == BallType::CUE && a.type != BallType::CUE) notifyCueHitBall(a.id);
    }
}

void Game::handleCollisions() {
    for (auto& b : balls_) collide(cueBall_, b);
    for (size_t i = 0;i < balls_.size();++i)
        for (size_t j = i + 1;j < balls_.size();++j)
            collide(balls_[i], balls_[j]);
}

void Game::assignGroupsIfNeeded() {
    if (groupsAssigned_) return;
    bool pocketedSolid = false, pocketedStripe = false;
    for (int id : turn_.pocketedIds) {
        if (id >= 1 && id <= 7) pocketedSolid = true;
        else if (id >= 9 && id <= 15) pocketedStripe = true;
    }
    if (turn_.foul) return;
    if (pocketedSolid ^ pocketedStripe) {
        groupsAssigned_ = true;
        playerGroup_[currentTeam_] = pocketedSolid ? BallType::SOLID : BallType::STRIPE;
        playerGroup_[1 - currentTeam_] = pocketedSolid ? BallType::STRIPE : BallType::SOLID;
    }
}

void Game::resolveTurnIfStopped() {
    if (anyMoving_ || !shotActive_) return;

    if (!firstHitBallId_.has_value()) turn_.foul = true;
    else {
        int firstId = firstHitBallId_.value();
        BallType firstType = typeById(firstId);
        if (!groupsAssigned_) {
            if (firstType == BallType::EIGHT) turn_.foul = true;
        }
        else {
            if (firstType == BallType::EIGHT) {
                bool ownRemaining = false;
                if (playerGroup_[currentTeam_].has_value()) {
                    BallType own = playerGroup_[currentTeam_].value();
                    ownRemaining = (own == BallType::SOLID) ? (remainingSolids() > 0)
                        : (remainingStripes() > 0);
                }
                if (ownRemaining) turn_.foul = true;
            }
            else if (playerGroup_[currentTeam_].has_value() &&
                playerGroup_[currentTeam_].value() != firstType) {
                turn_.foul = true;
            }
        }
    }

    assignGroupsIfNeeded();

    if (turn_.pocketedEight) {
        bool ownCleared = false;
        if (groupsAssigned_) {
            BallType own = playerGroup_[currentTeam_].value_or(BallType::SOLID);
            ownCleared = (own == BallType::SOLID) ? (remainingSolids() == 0)
                : (remainingStripes() == 0);
        }
        if (!turn_.foul && ownCleared) { gameOver_ = true; winnerTeam_ = currentTeam_; }
        else { gameOver_ = true; winnerTeam_ = 1 - currentTeam_; }
    }
    else {
        bool keepTurn = false;
        if (!turn_.foul) {
            if (!groupsAssigned_) keepTurn = turn_.anyPocket;
            else keepTurn = turn_.scoredOwn;
        }
        if (!keepTurn) currentTeam_ = 1 - currentTeam_;
    }

    shotActive_ = false;
    turn_ = {};
    firstHitBallId_.reset();
}

void Game::update() {
    if (gameOver_) return;

    anyMoving_ = false;

    step(cueBall_);
    for (auto& b : balls_) step(b);

    // Weiße versenkt -> Foul, zurück auf Break-Linie
    if (!cueBall_.inPlay || pocket(cueBall_)) {
        double Wp = R_ - L_, Hp = B_ - T_;
        cueBall_.x = L_ + Wp * 0.22; cueBall_.y = T_ + Hp * 0.5;
        cueBall_.vx = cueBall_.vy = 0; cueBall_.inPlay = true;
        turn_.foul = true; turn_.anyPocket = true;
    }

    // Objektkugeln versenkt
    for (size_t i = 0;i < balls_.size();++i) {
        Ball& b = balls_[i];
        if (!b.inPlay) continue;
        if (pocket(b)) {
            if (b.type == BallType::EIGHT) turn_.pocketedEight = true;
            else {
                turn_.anyPocket = true;
                turn_.pocketedIds.push_back(b.id);
                if (groupsAssigned_ && isOwnType(b, currentTeam_)) turn_.scoredOwn = true;
            }
            b.inPlay = false; b.vx = b.vy = 0;
            balls_.erase(balls_.begin() + i); --i;
        }
    }

    handleCollisions();

    auto moving = [&](const Ball& x) { return x.inPlay && len(x.vx, x.vy) >= 0.08; };
    if (cueBall_.inPlay && moving(cueBall_)) anyMoving_ = true;
    for (auto& b : balls_) if (moving(b)) { anyMoving_ = true; break; }

    if (!anyMoving_ && lastMoving_) resolveTurnIfStopped();
    lastMoving_ = anyMoving_;
}

Ball& Game::cue() { return cueBall_; }
const Ball& Game::cue() const { return cueBall_; }
const std::vector<Ball>& Game::balls() const { return balls_; }
int Game::currentPlayer() const { return currentTeam_; }
