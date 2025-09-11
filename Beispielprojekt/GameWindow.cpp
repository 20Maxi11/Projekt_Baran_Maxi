#include "GameWindow.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

GameWindow::GameWindow(unsigned width, unsigned height, int players)
    : Gosu::Window(width, height, false, 60.0),  // <-- 4 Parameter, nicht 5
    game_(width, height, players)
{
    set_caption("Gosu Billard");
}


void GameWindow::update() {
    // Mausposition für Zielhilfe erfassen
    aimX_ = input().mouse_x();
    aimY_ = input().mouse_y();
    // Beim Ziehen: Stoßstärke aus Abstand berechnen
    if (dragging_ && game_.allStopped()) {
        double dx = aimX_ - game_.cue().x;
        double dy = aimY_ - game_.cue().y;
        double dist = std::min(300.0, std::sqrt(dx * dx + dy * dy));
        power_ = std::clamp(dist / 300.0, 0.0, 1.0);
    }
    game_.update();
}

void GameWindow::drawCircle(double cx, double cy, double r, Gosu::Color color, double z, int segments) {
    const double angleStep = 2 * 3.1415926535 / segments;
    double prevX = cx + r;
    double prevY = cy;
    for (int i = 1; i <= segments; ++i) {
        double angle = angleStep * i;
        double newX = cx + std::cos(angle) * r;
        double newY = cy + std::sin(angle) * r;
        Gosu::Graphics::draw_triangle(cx, cy, color, prevX, prevY, color, newX, newY, color, z);
        prevX = newX;
        prevY = newY;
    }
}

void GameWindow::drawAim() {
    if (!game_.allStopped() || game_.isOver()) return;
    const Ball& cueBall = game_.cue();
    double dx = aimX_ - cueBall.x;
    double dy = aimY_ - cueBall.y;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0) return;
    double nx = dx / dist;
    double ny = dy / dist;
    // Gestrichelte Ziellinie zeichnen
    double lineLen = std::min(dist, 320.0);
    Gosu::Color lineColor(180, 255, 255, 255);
    const int dashCount = 24;
    for (int i = 0; i < dashCount; ++i) {
        double t0 = (lineLen / dashCount) * i;
        double t1 = (lineLen / dashCount) * (i + 0.5);
        double x0 = cueBall.x + nx * t0;
        double y0 = cueBall.y + ny * t0;
        double x1 = cueBall.x + nx * t1;
        double y1 = cueBall.y + ny * t1;
        Gosu::Graphics::draw_line(x0, y0, lineColor, x1, y1, lineColor, 3);
    }
    // Queue-Indikator (hinter der weißen Kugel, repräsentiert Ausholen)
    double backOffset = 40.0 + 160.0 * power_;
    double bx1 = cueBall.x - nx * (cueBall.r + backOffset);
    double by1 = cueBall.y - ny * (cueBall.r + backOffset);
    double bx2 = cueBall.x - nx * cueBall.r;
    double by2 = cueBall.y - ny * cueBall.r;
    double qx = -ny * 3;
    double qy = nx * 3;
    Gosu::Color cueColor(220, 200, 180, 160);
    Gosu::Graphics::draw_triangle(bx1 - qx, by1 - qy, cueColor,
        bx1 + qx, by1 + qy, cueColor,
        bx2 + qx, by2 + qy, cueColor, 2);
    Gosu::Graphics::draw_triangle(bx1 - qx, by1 - qy, cueColor,
        bx2 - qx, by2 - qy, cueColor,
        bx2 + qx, by2 + qy, cueColor, 2);
    // Power-Anzeige 
    double barW = 180, barH = 10;
    double barX = 10, barY = height() - 20;
    Gosu::Graphics::draw_rect(barX, barY, barW, barH, Gosu::Color::GRAY, 2);
    Gosu::Graphics::draw_rect(barX, barY, barW * power_, barH, Gosu::Color::YELLOW, 3);
}
//Platzhalter für Bilder dann 
void GameWindow::draw() {
    // Tisch (grüner Hintergrund)
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color::GREEN, 0);
    // Taschen (schwarze Kreise in Ecken und Mitten der kurzen Banden)
    Gosu::Color pocketColor = Gosu::Color::BLACK;
    double pr = game_.cue().r * 1.5;  // Taschenradius ~1.5 * Kugelradius
    drawCircle(0, 0, pr, pocketColor, 1, 20);
    drawCircle(width(), 0, pr, pocketColor, 1, 20);
    drawCircle(0, height(), pr, pocketColor, 1, 20);
    drawCircle(width(), height(), pr, pocketColor, 1, 20);
    drawCircle(width() / 2.0, 0, pr, pocketColor, 1, 20);
    drawCircle(width() / 2.0, height(), pr, pocketColor, 1, 20);
    // Kugeln zeichnen
    auto drawBall = [&](const Ball& b) {
        if (!b.inPlay) return;
        drawCircle(b.x, b.y, b.r, b.color, 2, 28);
        drawCircle(b.x - b.r * 0.35, b.y - b.r * 0.35, b.r * 0.3, Gosu::Color(180, 255, 255, 255), 3, 14);
        };
    for (const Ball& b : game_.balls()) {
        drawBall(b);
    }
    drawBall(game_.cue());
    // Zielhilfe zeichnen
    drawAim();
    // HUD-Text (Spielinfo)
    std::ostringstream hud;
    if (!game_.isOver()) {
        if (game_.numPlayers() > 2) {
            hud << "Team1(" << game_.remainingSolids() << " solids left)  "
                << "Team2(" << game_.remainingStripes() << " stripes left)   ";
        }
        else {
            hud << "P1(" << game_.remainingSolids() << " solids left)  "
                << "P2(" << game_.remainingStripes() << " stripes left)   ";
        }
        hud << "Turn: P" << (game_.currentPlayer() + 1);
    }
    else {
        if (game_.numPlayers() > 2) {
            hud << "Winners: Team " << (game_.winner() + 1);
            if (game_.winner() == 0) {
                hud << " (P1 & P3)";
            }
            else {
                hud << " (P2 & P4)";
            }
        }
        else {
            hud << "Winner: Player " << (game_.winner() + 1);
        }
        hud << "  -  N: new match, R: rebuild";
    }
    font_.draw_text(hud.str(), 10, 10, 4, 1.0, 1.0, Gosu::Color::WHITE);
}

void GameWindow::shootFromAim() {
    if (!game_.allStopped() || game_.isOver()) return;
    Ball& cueBall = game_.cue();
    double dx = aimX_ - cueBall.x;
    double dy = aimY_ - cueBall.y;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0) return;
    double nx = dx / dist;
    double ny = dy / dist;
    double force = 6.0 + 20.0 * power_;  // Stoßkraft (Basis 6, skaliert bis 26)
    cueBall.vx = -nx * force;
    cueBall.vy = -ny * force;
    game_.beginShot();
}

void GameWindow::button_down(Gosu::Button button) {
    Gosu::Window::button_down(button);
    if (button == Gosu::MS_LEFT && game_.allStopped() && !game_.isOver()) {
        dragging_ = true;
    }
    if (button == Gosu::KB_R) {
        game_.reset(true);
    }
    if (button == Gosu::KB_N) {
        game_.reset(false);
    }
    if (button == Gosu::KB_ESCAPE) {
        close();
    }
}

void GameWindow::button_up(Gosu::Button button) {
    if (button == Gosu::MS_LEFT && dragging_) {
        shootFromAim();
    }
    dragging_ = false;
    power_ = 0.0;
}
