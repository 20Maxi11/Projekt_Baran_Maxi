#include "GameWindow.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

static inline double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }

GameWindow::GameWindow(unsigned width, unsigned height, int /*players*/)
    : Gosu::Window(width, height, false, 60.0)
    , game_(width, height, 2)
{
    set_caption("Gosu Billard");
    set_resizable(true);
    ensureFonts();
    computeTableRect();
}

double GameWindow::rail() const {
    // 5% der kleineren Tischkante, min 18, max 56
    double r = std::min(tableW_, tableH_) * 0.05;
    return std::max(18.0, std::min(56.0, r));
}

void GameWindow::ensureFonts() {
    if (font_ && fontTitle_ && lastW_ == width() && lastH_ == height()) return;
    lastW_ = width(); lastH_ = height();
    int hud = std::max(16, (int)std::round(height() * 0.035));
    int title = std::max(26, (int)std::round(height() * 0.065));
    font_ = std::make_unique<Gosu::Font>(hud, Gosu::default_font_name());
    fontTitle_ = std::make_unique<Gosu::Font>(title, Gosu::default_font_name());
}

void GameWindow::computeTableRect() {
    // „Raum“-Rand (schwarzer Bereich um den Tisch)
    double roomPad = std::max(20.0, std::min(width(), height()) * 0.07); // ~7% Außenrand
    double availW = std::max(1.0, width() - 2 * roomPad);
    double availH = std::max(1.0, height() - 2 * roomPad);

    // Tisch im Verhältnis 2:1 (Breite:Höhe) maximal ins Fenster einpassen
    const double aspect = 2.0; // klassisches Poolmaß
    double w = availW;
    double h = w / aspect;
    if (h > availH) { h = availH; w = h * aspect; }

    tableW_ = w;
    tableH_ = h;
    tableX_ = (width() - tableW_) / 2.0;
    tableY_ = (height() - tableH_) / 2.0;
}

void GameWindow::layoutStartBoxes() {
    computeTableRect(); // bei Resizes konsistent
    double r = rail(), bh = std::max(36.0, height() * 0.06);
    double bx = tableX_ + tableW_ * 0.18, bw = tableW_ * 0.64;
    double y1 = tableY_ + r + tableH_ * 0.20, y2 = y1 + bh + std::max(24.0, height() * 0.05);
    p1Box_ = { bx, y1, bw, bh };
    p2Box_ = { bx, y2, bw, bh };
}

void GameWindow::update() {
    ensureFonts();
    computeTableRect();

    // Innenmaß (grüner Filz) ans Game melden
    double r = rail();
    double px = tableX_ + r, py = tableY_ + r;
    double pw = tableW_ - 2 * r, ph = tableH_ - 2 * r;

    game_.W = width(); game_.H = height();
    game_.set_playfield(px, py, px + pw, py + ph);

    if (state_ != UiState::Playing) return;

    aimX_ = input().mouse_x();
    aimY_ = input().mouse_y();

    if (dragging_ && game_.allStopped()) {
        double dx = aimX_ - game_.cue().x, dy = aimY_ - game_.cue().y;
        double d = std::sqrt(dx * dx + dy * dy);
        power_ = clamp01(std::min(300.0, d) / 300.0);
    }

    game_.update();
    if (game_.isOver()) state_ = UiState::GameOver;
}

void GameWindow::drawCircle(double cx, double cy, double r, Gosu::Color color, double z, int seg) {
    const double step = 2 * 3.14159265358979323846 / seg;
    double px = cx + r, py = cy;
    for (int i = 1; i <= seg; ++i) {
        double a = step * i;
        double nx = cx + std::cos(a) * r, ny = cy + std::sin(a) * r;
        Gosu::Graphics::draw_triangle(cx, cy, color, px, py, color, nx, ny, color, z);
        px = nx; py = ny;
    }
}

void GameWindow::drawTextShadow(Gosu::Font& f, const std::string& s, double x, double y, double z, Gosu::Color col) {
    f.draw_text(s, x + 1, y + 1, z, 1.0, 1.0, Gosu::Color(200, 0, 0, 0));
    f.draw_text(s, x, y, z, 1.0, 1.0, col);
}

void GameWindow::drawAim() {
    if (!game_.allStopped() || game_.isOver()) return;
    const Ball& c = game_.cue();
    double dx = aimX_ - c.x, dy = aimY_ - c.y;
    double d = std::sqrt(dx * dx + dy * dy);
    if (d < 1.0) return;
    double nx = dx / d, ny = dy / d;

    double len = std::min(d, 340.0);
    Gosu::Color lc(200, 255, 255, 255);
    const int dashN = 24;
    for (int i = 0;i < dashN;++i) {
        double t0 = (len / dashN) * i, t1 = (len / dashN) * (i + 0.55);
        double x0 = c.x + nx * t0, y0 = c.y + ny * t0;
        double x1 = c.x + nx * t1, y1 = c.y + ny * t1;
        Gosu::Graphics::draw_line(x0, y0, lc, x1, y1, lc, 3);
    }

    double back = 40 + 160 * power_;
    double bx1 = c.x - nx * (c.r + back), by1 = c.y - ny * (c.r + back);
    double bx2 = c.x - nx * (c.r), by2 = c.y - ny * (c.r);
    double qx = -ny * 3, qy = nx * 3;
    Gosu::Color qc(220, 200, 180, 180);
    Gosu::Graphics::draw_triangle(bx1 - qx, by1 - qy, qc, bx1 + qx, by1 + qy, qc, bx2 + qx, by2 + qy, qc, 2);
    Gosu::Graphics::draw_triangle(bx1 - qx, by1 - qy, qc, bx2 - qx, by2 - qy, qc, bx2 + qx, by2 + qy, qc, 2);

    // Powerbar
    double bw = std::max(180.0, width() * 0.18), bh = std::max(10.0, height() * 0.015);
    double bx = 10, by = height() - (bh + 10);
    Gosu::Graphics::draw_rect(bx, by, bw, bh, Gosu::Color(180, 30, 30, 30), 2);
    Gosu::Graphics::draw_rect(bx, by, bw * power_, bh, Gosu::Color(255, 220, 0, 255), 3);
}

void GameWindow::drawHud() {
    double barH = std::max(32.0, height() * 0.06);
    double y = height() - (barH + 6);
    Gosu::Graphics::draw_rect(0, y, width(), barH + 6, Gosu::Color(180, 0, 0, 0), 5);

    int turn = game_.currentPlayer();
    Gosu::Color a = (turn == 0) ? Gosu::Color::WHITE : Gosu::Color(220, 220, 220, 255);
    Gosu::Color b = (turn == 1) ? Gosu::Color::WHITE : Gosu::Color(220, 220, 220, 255);

    std::string l = " " + p1Name_ + "  (volle: " + std::to_string(game_.remainingSolids()) + ")";
    std::string r = " " + p2Name_ + "  (halbe: " + std::to_string(game_.remainingStripes()) + ")";

    drawTextShadow(*font_, l, 12, y + 6, 6, a);
    double rw = font_->text_width(r);
    drawTextShadow(*font_, r, width() - 12 - rw, y + 6, 6, b);

    std::string mid = (turn == 0 ? p1Name_ : p2Name_) + std::string(" ist am Zug");
    double mw = font_->text_width(mid);
    drawTextShadow(*font_, mid, (width() - mw) / 2.0, y + 6, 6, Gosu::Color(255, 220, 0, 255));
}

void GameWindow::drawStart() {
    layoutStartBoxes();

    // kompletter Hintergrund schwarz (Raum)
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color::BLACK, 0);

    // Holz-Rail + grüner Filz (wie im Spiel)
    double r = rail();
    Gosu::Graphics::draw_rect(tableX_, tableY_, tableW_, tableH_, Gosu::Color(255, 90, 60, 30), 1);               // Holz
    Gosu::Graphics::draw_rect(tableX_ + r, tableY_ + r, tableW_ - 2 * r, tableH_ - 2 * r, Gosu::Color(30, 120, 40, 255), 2);  // grün

    // UI
    std::string title = "8-Ball – Namen eingeben";
    double tw = fontTitle_->text_width(title);
    drawTextShadow(*fontTitle_, title, (width() - tw) / 2, tableY_ + r * 0.5, 3, Gosu::Color::WHITE);

    Gosu::Color frame(200, 50, 50, 50);
    Gosu::Graphics::draw_rect(p1Box_.x, p1Box_.y, p1Box_.w, p1Box_.h, frame, 3);
    Gosu::Graphics::draw_rect(p2Box_.x, p2Box_.y, p2Box_.w, p2Box_.h, frame, 3);

    drawTextShadow(*font_, "Spieler 1:", p1Box_.x, p1Box_.y - font_->height() - 6, 4, Gosu::Color::WHITE);
    drawTextShadow(*font_, "Spieler 2:", p2Box_.x, p2Box_.y - font_->height() - 6, 4, Gosu::Color::WHITE);

    Gosu::Color c1 = (activeName_ == 0) ? Gosu::Color(255, 255, 220, 0) : Gosu::Color::WHITE;
    Gosu::Color c2 = (activeName_ == 1) ? Gosu::Color(255, 255, 220, 0) : Gosu::Color::WHITE;
    drawTextShadow(*font_, p1Name_, p1Box_.x + 10, p1Box_.y + (p1Box_.h - font_->height()) / 2, 4, c1);
    drawTextShadow(*font_, p2Name_, p2Box_.x + 10, p2Box_.y + (p2Box_.h - font_->height()) / 2, 4, c2);

    std::string hint = "ENTER: Start   |   KLICK/TAB: Feld wechseln   |   ESC: Beenden";
    double hw = font_->text_width(hint);
    drawTextShadow(*font_, hint, (width() - hw) / 2, tableY_ + tableH_ - r - font_->height() - 6, 4, Gosu::Color::WHITE);
}

void GameWindow::drawPause() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color(170, 0, 0, 0), 10);
    std::string title = "PAUSE";
    double tw = fontTitle_->text_width(title);
    drawTextShadow(*fontTitle_, title, (width() - tw) / 2, tableY_ + rail() + 20, 11, Gosu::Color::WHITE);

    std::string info = "P: Zurueck   |   N: Neues Spiel   |   ESC: Beenden";
    double iw = font_->text_width(info);
    drawTextShadow(*font_, info, (width() - iw) / 2, tableY_ + rail() + 20 + fontTitle_->height() + 8, 11, Gosu::Color(255, 220, 0, 255));
}

void GameWindow::drawGameOver() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color(150, 0, 0, 0), 10);
    int win = game_.winner();
    std::string winner = (win == 0 ? p1Name_ : p2Name_) + std::string(" hat gewonnen!");
    double ww = fontTitle_->text_width(winner);
    drawTextShadow(*fontTitle_, winner, (width() - ww) / 2, tableY_ + rail() + 20, 11, Gosu::Color::WHITE);

    std::string info = "N: Neues Match   |   R: Neu aufbauen   |   ESC: Beenden";
    double iw = font_->text_width(info);
    drawTextShadow(*font_, info, (width() - iw) / 2, tableY_ + rail() + 20 + fontTitle_->height() + 8, 11, Gosu::Color(255, 220, 0, 255));
}

void GameWindow::draw() {
    // Raum: schwarz
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color::BLACK, 0);

    // Holz-Rail + grüner Filz (zentriert, 2:1)
    double r = rail();
    Gosu::Graphics::draw_rect(tableX_, tableY_, tableW_, tableH_, Gosu::Color(255, 90, 60, 30), 1);              // Holz
    Gosu::Graphics::draw_rect(tableX_ + r, tableY_ + r, tableW_ - 2 * r, tableH_ - 2 * r, Gosu::Color(30, 120, 40, 255), 2); // grün

    // Taschen (an Innenmaß!)
    Gosu::Color pc = Gosu::Color::BLACK;
    double pr = game_.pocketR;
    double ix = tableX_ + r, iy = tableY_ + r, iw = tableW_ - 2 * r, ih = tableH_ - 2 * r;
    drawCircle(ix, iy, pr, pc, 3, 22);
    drawCircle(ix + iw, iy, pr, pc, 3, 22);
    drawCircle(ix, iy + ih, pr, pc, 3, 22);
    drawCircle(ix + iw, iy + ih, pr, pc, 3, 22);
    drawCircle(ix + iw / 2.0, iy, pr, pc, 3, 22);
    drawCircle(ix + iw / 2.0, iy + ih, pr, pc, 3, 22);

    // Kugeln
    auto drawBall = [&](const Ball& b) {
        if (!b.inPlay) return;
        drawCircle(b.x, b.y, b.r, b.color, 4, 28);
        drawCircle(b.x - b.r * 0.35, b.y - b.r * 0.35, b.r * 0.30, Gosu::Color(180, 255, 255, 255), 5, 14);
        };
    for (const Ball& b : game_.balls()) drawBall(b);
    drawBall(game_.cue());

    // Overlays
    switch (state_) {
    case UiState::Start:    drawStart(); break;
    case UiState::Paused:   drawAim(); drawHud(); drawPause(); break;
    case UiState::GameOver: drawAim(); drawHud(); drawGameOver(); break;
    case UiState::Playing:  drawAim(); drawHud(); break;
    }
}

void GameWindow::shootFromAim() {
    if (state_ != UiState::Playing) return;
    if (!game_.allStopped() || game_.isOver()) return;
    Ball& c = game_.cue();
    double dx = aimX_ - c.x, dy = aimY_ - c.y;
    double d = std::sqrt(dx * dx + dy * dy);
    if (d < 1.0) return;
    double nx = dx / d, ny = dy / d;
    double force = 6.0 + 20.0 * power_;
    c.vx = -nx * force; c.vy = -ny * force;
    game_.beginShot();
}

void GameWindow::togglePause() {
    if (state_ == UiState::Playing) state_ = UiState::Paused;
    else if (state_ == UiState::Paused) state_ = UiState::Playing;
}

void GameWindow::startMatch() {
    if (p1Name_.empty()) p1Name_ = "Spieler 1";
    if (p2Name_.empty()) p2Name_ = "Spieler 2";
    game_.reset(false);
    state_ = UiState::Playing;
}

void GameWindow::handleNameKey(Gosu::Button b) {
    std::string& s = (activeName_ == 0) ? p1Name_ : p2Name_;

    if (b == Gosu::KB_BACKSPACE) { if (!s.empty()) s.pop_back(); return; }
    if (b == Gosu::KB_RETURN) { startMatch(); return; }

    if (b >= Gosu::KB_A && b <= Gosu::KB_Z) {
        char ch = 'A' + (int(b) - int(Gosu::KB_A));
        s.push_back(ch); return;
    }
    if (b >= Gosu::KB_0 && b <= Gosu::KB_9) {
        char ch = '0' + (int(b) - int(Gosu::KB_0));
        s.push_back(ch); return;
    }
    if (b == Gosu::KB_SPACE) { s.push_back(' '); return; }
    if (b == Gosu::KB_MINUS) { s.push_back('-'); return; }
}

void GameWindow::button_down(Gosu::Button b) {
    Gosu::Window::button_down(b);

    // Startscreen: Maus klickt Feld
    if (b == Gosu::MS_LEFT && state_ == UiState::Start) {
        layoutStartBoxes();
        double mx = input().mouse_x(), my = input().mouse_y();
        if (p1Box_.contains(mx, my)) activeName_ = 0;
        else if (p2Box_.contains(mx, my)) activeName_ = 1;
        return;
    }

    if (state_ == UiState::Start) {
        if (b == Gosu::KB_TAB) { activeName_ ^= 1; return; }
        handleNameKey(b);
        return;
    }

    // Gameplay
    if (b == Gosu::MS_LEFT && state_ == UiState::Playing && game_.allStopped() && !game_.isOver())
        dragging_ = true;

    // Global
    if (b == Gosu::KB_P && (state_ == UiState::Playing || state_ == UiState::Paused)) togglePause();
    if (b == Gosu::KB_R && state_ != UiState::Start) game_.reset(true);
    if (b == Gosu::KB_N && state_ != UiState::Start) { game_.reset(false); state_ = UiState::Playing; }
    if (b == Gosu::KB_ESCAPE) close();
}

void GameWindow::button_up(Gosu::Button b) {
    if (b == Gosu::MS_LEFT && dragging_) { shootFromAim(); }
    dragging_ = false; power_ = 0.0;
}
