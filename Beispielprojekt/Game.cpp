#include "Game.hpp"
#include <algorithm>
#include <cmath>

static inline double len(double x, double y) {
    return std::sqrt(x * x + y * y);
}

// Hilfs-Array f¸r Partnerwechsel bei 4 Spielern (Team0: Spieler 0 & 2, Team1: Spieler 1 & 3)
static const int partnerIndex[4] = { 2, 3, 0, 1 };

Game::Game(double tableWidth, double tableHeight, int players)
    : W(tableWidth), H(tableHeight),
    cueBall_(0, BallType::CUE, tableWidth * 0.22, tableHeight * 0.5, Gosu::Color::WHITE, 10.0),
    numPlayers_(players) {
    reset(false);
}

void Game::reset(bool keepScores) {
    gameOver_ = false;
    winnerTeam_ = -1;
    groupsAssigned_ = false;
    playerGroup_[0].reset();
    playerGroup_[1].reset();
    turn_ = {};
    firstHitBallId_.reset();
    shotActive_ = false;
    // Spieler-Tracking zur¸cksetzen
    lastShooterTeam_[0] = -1;
    lastShooterTeam_[1] = -1;
    if (!keepScores) {
        currentTeam_ = 0;
        // Falls neues Spiel: Team 0 beginnt, erster Spieler Team 0
        currentPlayerIndex_ = 0;
    }
    // Weiﬂe Kugel zur¸cksetzen
    cueBall_.x = W * 0.22;
    cueBall_.y = H * 0.5;
    cueBall_.vx = cueBall_.vy = 0;
    cueBall_.inPlay = true;
    // Objektkugeln aufstellen
    balls_.clear();
    placeTriangle();
}

void Game::placeTriangle() {
    // 15 Objektkugeln im Dreieck (8-Ball): Spitze nach rechts ausgerichtet
    const double d = cueBall_.r * 2.05;   // Abstand der Kugelzentren
    double sx = W * 0.68;
    double sy = H * 0.5;
    auto colorFor = [](int id)->Gosu::Color {
        if (id == 8)  return Gosu::Color::GRAY;
        if (id <= 7)  return Gosu::Color::RED;
        return Gosu::Color::YELLOW;
        };
    auto typeFor = [](int id)->BallType {
        if (id == 0) return BallType::CUE;
        if (id == 8) return BallType::EIGHT;
        if (id <= 7) return BallType::SOLID;
        return BallType::STRIPE;
        };
    int ids[15] = { 1, 10, 2, 9, 8, 3, 12, 4, 13, 5, 14, 6, 11, 7, 15 };
    int k = 0;
    for (int row = 0; row < 5; ++row) {
        for (int i = 0; i <= row; ++i) {
            int id = ids[k++];
            double x = sx + row * d * std::sqrt(3.0) / 2.0;
            double y = sy + (i - row * 0.5) * d;
            balls_.emplace_back(id, typeFor(id), x, y, colorFor(id), 10.0);
        }
    }
}

bool Game::allStopped() const {
    auto slow = [](const Ball& b) { return len(b.vx, b.vy) < 0.08; };
    if (cueBall_.inPlay && !slow(cueBall_)) return false;
    for (const auto& b : balls_) {
        if (b.inPlay && !slow(b)) return false;
    }
    return true;
}

int Game::remainingSolids() const {
    int count = 0;
    for (const auto& b : balls_) {
        if (b.inPlay && b.type == BallType::SOLID) ++count;
    }
    return count;
}

int Game::remainingStripes() const {
    int count = 0;
    for (const auto& b : balls_) {
        if (b.inPlay && b.type == BallType::STRIPE) ++count;
    }
    return count;
}

void Game::beginShot() {
    shotActive_ = true;
    turn_ = {};
    firstHitBallId_.reset();
    // aktuellen Sch¸tzen f¸r diesen Stoﬂ vermerken
    lastShooterTeam_[currentTeam_] = currentPlayerIndex_;
}

void Game::notifyCueHitBall(int id) {
    if (!firstHitBallId_.has_value()) {
        firstHitBallId_ = id;
    }
}

bool Game::isOwnType(const Ball& b, int player) const {
    if (!groupsAssigned_) return false;
    if (b.type != BallType::SOLID && b.type != BallType::STRIPE) return false;
    return playerGroup_[player].has_value() && playerGroup_[player].value() == b.type;
}

void Game::step(Ball& b) {
    if (!b.inPlay) return;
    b.x += b.vx;
    b.y += b.vy;
    b.vx *= friction_;
    b.vy *= friction_;
    if (len(b.vx, b.vy) < 0.08) {
        b.vx = 0;
        b.vy = 0;
    }
    if (!pocket(b)) {
        wall(b);
    }
}

void Game::wall(Ball& b) {
    if (b.x - b.r < 0) {
        b.x = b.r;
        b.vx = -b.vx;
    }
    if (b.x + b.r > W) {
        b.x = W - b.r;
        b.vx = -b.vx;
    }
    if (b.y - b.r < 0) {
        b.y = b.r;
        b.vy = -b.vy;
    }
    if (b.y + b.r > H) {
        b.y = H - b.r;
        b.vy = -b.vy;
    }
}

bool Game::pocket(const Ball& b) const {
    struct Pocket { double x, y; };
    Pocket pockets[6] = { {0,0}, {W,0}, {0,H}, {W,H}, {W / 2.0, 0}, {W / 2.0, H} };
    for (const auto& p : pockets) {
        if (len(b.x - p.x, b.y - p.y) <= pocketR) {
            return true;
        }
    }
    return false;
}

void Game::collide(Ball& a, Ball& b) {
    if (!a.inPlay || !b.inPlay) return;
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double rr = a.r + b.r;
    double dist2 = dx * dx + dy * dy;
    if (dist2 <= 0 || dist2 >= rr * rr) return;
    double d = std::sqrt(dist2);
    double nx = dx / d;
    double ny = dy / d;
    // ‹berlappung auflˆsen
    double overlap = rr - d;
    a.x -= nx * overlap / 2;
    a.y -= ny * overlap / 2;
    b.x += nx * overlap / 2;
    b.y += ny * overlap / 2;
    // Normal- und Tangentialkomponenten der Geschwindigkeiten
    double vaN = a.vx * nx + a.vy * ny;
    double vbN = b.vx * nx + b.vy * ny;
    double tx = -ny;
    double ty = nx;
    double vaT = a.vx * tx + a.vy * ty;
    double vbT = b.vx * tx + b.vy * ty;
    // Elastischer Stoﬂ (gleiche Masse): Normalanteile tauschen
    double vaN_new = vbN;
    double vbN_new = vaN;
    a.vx = vaN_new * nx + vaT * tx;
    a.vy = vaN_new * ny + vaT * ty;
    b.vx = vbN_new * nx + vbT * tx;
    b.vy = vbN_new * ny + vbT * ty;
    // Erste getroffene Kugel erfassen, falls Weiﬂe beteiligt
    if (shotActive_) {
        if (a.type == BallType::CUE && b.type != BallType::CUE) {
            notifyCueHitBall(b.id);
        }
        else if (b.type == BallType::CUE && a.type != BallType::CUE) {
            notifyCueHitBall(a.id);
        }
    }
}

void Game::handleCollisions() {
    // Kollision Weiﬂe mit allen Objektkugeln
    for (auto& b : balls_) {
        collide(cueBall_, b);
    }
    // Kollisionen zwischen Objektkugeln
    for (size_t i = 0; i < balls_.size(); ++i) {
        for (size_t j = i + 1; j < balls_.size(); ++j) {
            collide(balls_[i], balls_[j]);
        }
    }
}

void Game::assignGroupsIfNeeded() {
    if (groupsAssigned_) return;
    // Gruppen anhand im Stoﬂ versenkter Kugeln zuweisen (sofern kein Foul)
    bool pocketedSolid = false;
    bool pocketedStripe = false;
    for (int id : turn_.pocketedIds) {
        if (id >= 1 && id <= 7) pocketedSolid = true;
        else if (id >= 9 && id <= 15) pocketedStripe = true;
    }
    if (turn_.foul) return;
    if (pocketedSolid ^ pocketedStripe) {
        // genau eine Sorte versenkt
        groupsAssigned_ = true;
        if (pocketedSolid) {
            playerGroup_[currentTeam_] = BallType::SOLID;
            playerGroup_[1 - currentTeam_] = BallType::STRIPE;
        }
        else {
            playerGroup_[currentTeam_] = BallType::STRIPE;
            playerGroup_[1 - currentTeam_] = BallType::SOLID;
        }
    }
}

void Game::resolveTurnIfStopped() {
    if (anyMoving_ || !shotActive_) return;
    // Alle Kugeln stehen nach einem Stoﬂ -> Zug auswerten
    // Foul-Pr¸fung: erste getroffene Kugel
    if (!firstHitBallId_.has_value()) {
        // Weiﬂe hat nichts getroffen
        turn_.foul = true;
    }
    else {
        int firstId = firstHitBallId_.value();
        if (!groupsAssigned_) {
            if (firstId == 8) {
                // Acht zuerst getroffen bei offenem Tisch -> Foul
                turn_.foul = true;
            }
        }
        else {
            // Gruppen sind bereits zugewiesen
            BallType firstType;
            if (firstId == 8) {
                firstType = BallType::EIGHT;
            }
            else if (firstId >= 1 && firstId <= 7) {
                firstType = BallType::SOLID;
            }
            else if (firstId >= 9 && firstId <= 15) {
                firstType = BallType::STRIPE;
            }
            else {
                firstType = BallType::CUE;
            }
            if (firstType == BallType::EIGHT) {
                // Acht zuerst getroffen, obwohl noch eigene Kugeln ¸brig -> Foul
                bool ownRemaining = false;
                if (playerGroup_[currentTeam_].has_value()) {
                    BallType ownType = playerGroup_[currentTeam_].value();
                    if (ownType == BallType::SOLID) {
                        ownRemaining = (remainingSolids() > 0);
                    }
                    else if (ownType == BallType::STRIPE) {
                        ownRemaining = (remainingStripes() > 0);
                    }
                }
                if (ownRemaining) {
                    turn_.foul = true;
                }
            }
            else if ((firstType == BallType::SOLID || firstType == BallType::STRIPE)
                && playerGroup_[currentTeam_].has_value()
                && playerGroup_[currentTeam_].value() != firstType) {
                // falsche Farbe zuerst getroffen (gegnerische Gruppe) -> Foul
                turn_.foul = true;
            }
        }
    }

    // Gruppen ggf. jetzt festlegen
    assignGroupsIfNeeded();

    // Acht versenkt?
    if (turn_.pocketedEight) {
        bool ownCleared = false;
        if (groupsAssigned_) {
            BallType ownType = playerGroup_[currentTeam_].value_or(BallType::SOLID);
            if (ownType == BallType::SOLID) {
                ownCleared = (remainingSolids() == 0);
            }
            else {
                ownCleared = (remainingStripes() == 0);
            }
        }
        if (!turn_.foul && ownCleared) {
            // Acht korrekt versenkt -> aktuelles Team gewinnt
            gameOver_ = true;
            winnerTeam_ = currentTeam_;
        }
        else {
            // Acht versenkt mit Foul oder vorzeitig -> Gegner gewinnt
            gameOver_ = true;
            winnerTeam_ = 1 - currentTeam_;
        }
    }
    else {
        // Keine 8 versenkt: bestimmen, ob Spieler weitermachen darf
        bool keepTurn = false;
        if (!turn_.foul) {
            if (!groupsAssigned_) {
                // vor Gruppenfestlegung: wenn irgendeine Objektkugel versenkt wurde, weiter
                keepTurn = turn_.anyPocket;
            }
            else {
                // nach Gruppenfestlegung: nur weiter, wenn eigene Kugel versenkt
                keepTurn = turn_.scoredOwn;
            }
        }
        if (!keepTurn) {
            // Zug wechselt zum anderen Team
            int newTeam = 1 - currentTeam_;
            // n‰chsten Spieler des neuen Teams bestimmen
            if (numPlayers_ > 2) {
                if (lastShooterTeam_[newTeam] < 0) {
                    // Team hatte noch keinen Stoﬂ: fester Startspieler
                    currentPlayerIndex_ = (newTeam == 0 ? 0 : 1);
                }
                else {
                    // sonst Partner des zuletzt f¸r dieses Team spielenden Spielers
                    currentPlayerIndex_ = partnerIndex[lastShooterTeam_[newTeam]];
                }
            }
            else {
                currentPlayerIndex_ = newTeam;
            }
            currentTeam_ = newTeam;
        }
        else {
            // gleicher Spieler (bzw. Team) macht weiter ñ bei Team ggf. Partner wechseln
            if (numPlayers_ > 2) {
                currentPlayerIndex_ = partnerIndex[currentPlayerIndex_];
            }
            // (bei 2 Spielern bleibt currentPlayerIndex unver‰ndert, da ein Team = ein Spieler)
        }
    }

    // Bereit f¸r den n‰chsten Stoﬂ
    shotActive_ = false;
    turn_ = {};
    firstHitBallId_.reset();
}

void Game::update() {
    if (gameOver_) return;
    anyMoving_ = false;
    // Bewegung aller Kugeln berechnen
    step(cueBall_);
    for (auto& b : balls_) {
        step(b);
    }
    // Ereignisse w‰hrend des Stoﬂes pr¸fen (Taschen)
    // Scratch (Weiﬂe versenkt)
    if (!cueBall_.inPlay || pocket(cueBall_)) {
        cueBall_.x = W * 0.22;
        cueBall_.y = H * 0.5;
        cueBall_.vx = cueBall_.vy = 0;
        cueBall_.inPlay = true;
        turn_.foul = true;
        turn_.anyPocket = true;
    }
    // Objektkugeln versenkt
    for (size_t i = 0; i < balls_.size(); ++i) {
        Ball& b = balls_[i];
        if (!b.inPlay) continue;
        if (pocket(b)) {
            if (b.type == BallType::EIGHT) {
                turn_.pocketedEight = true;
            }
            else {
                turn_.anyPocket = true;
                turn_.pocketedIds.push_back(b.id);
                if (groupsAssigned_ && isOwnType(b, currentTeam_)) {
                    turn_.scoredOwn = true;
                }
            }
            // Kugel aus dem Spiel nehmen
            b.inPlay = false;
            b.vx = b.vy = 0;
            balls_.erase(balls_.begin() + i);
            --i;
        }
    }
    // Kollisionen behandeln
    handleCollisions();
    // Pr¸fen, ob noch Kugeln in Bewegung sind
    auto moving = [&](const Ball& x) {
        return x.inPlay && len(x.vx, x.vy) >= 0.08;
        };
    if (cueBall_.inPlay && moving(cueBall_)) {
        anyMoving_ = true;
    }
    for (auto& b : balls_) {
        if (moving(b)) {
            anyMoving_ = true;
            break;
        }
    }
    // Wenn jetzt alle Kugeln liegen und vorher Bewegung war: Stoﬂ auswerten
    if (!anyMoving_ && lastMoving_) {
        resolveTurnIfStopped();
    }
    lastMoving_ = anyMoving_;
}

Ball& Game::cue() {
    return cueBall_;
}

const Ball& Game::cue() const {
    return cueBall_;
}

const std::vector<Ball>& Game::balls() const {
    return balls_;
}

int Game::currentPlayer() const {
    return (numPlayers_ > 2 ? currentPlayerIndex_ : currentTeam_);
}
