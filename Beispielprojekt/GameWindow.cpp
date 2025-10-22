#include "GameWindow.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <Gosu/Bitmap.hpp>

static inline double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }

// Heuristiken für PNG-Analyse
static inline bool isGreen(int r, int g, int b) { return g > 70 && g > int(r * 1.15) && g > int(b * 1.15); }
static inline bool isDark(int r, int g, int b) { return r < 40 && g < 40 && b < 40; }

// ---------- Rendering- und Lade-Optionen ----------
static constexpr double kBallImageScale = 1.00;     // *** WICHTIG: 1.00 -> Physik bestimmt Größe ***/ da sonst bilder ienfach nur größer dargestl werdne aber physik nicht passt 
static constexpr double kBallShadowMul = 1.06;
static constexpr double kBallShadow_dx = 2.0;
static constexpr double kBallShadow_dy = 2.0;
static constexpr int    kBallSegments = 28;

static constexpr unsigned kBallImageFlags =
#ifdef IF_RETRO
Gosu::IF_RETRO;
#else
Gosu::IF_SMOOTH;
#endif

// ---------- robustes Image-Loading ----------
static std::string ascii_fallback(std::string s) {
    auto repl = [&](const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
        };
    repl("ä", "ae"); repl("ö", "oe"); repl("ü", "ue");
    repl("Ä", "Ae"); repl("Ö", "Oe"); repl("Ü", "Ue");
    repl("ß", "ss");
    return s;
}
static std::string no_spaces(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}
static std::unique_ptr<Gosu::Image> try_load_variants(const std::string& baseName, unsigned flags) {
    std::vector<std::string> cand = {
        baseName,
        ascii_fallback(baseName),
        no_spaces(baseName),
        no_spaces(ascii_fallback(baseName))
    };
    for (const auto& f : cand) {
        try { return std::make_unique<Gosu::Image>(f, flags); }
        catch (...) { /* nächste Variante */ }
    }
    return nullptr;
}

GameWindow::GameWindow(unsigned width, unsigned height, int /*players*/, bool fullscreen)
    : Gosu::Window(width, height, fullscreen, 60.0),
    game_(width, height, 2)
{
    set_caption("Gosu Billard (Bilder)");
    loadAssets();
    ensureFonts();
    computeTableRect();
}

void GameWindow::loadAssets() {
    // Tisch & Queue
    try { felt_ = try_load_variants("Tisch.png", Gosu::IF_SMOOTH); }
    catch (...) {}
    if (!felt_) { try { felt_ = std::make_unique<Gosu::Image>("Tisch.png", Gosu::IF_SMOOTH); } catch (...) {} }

    try { cueImg_ = try_load_variants("Queue.png", Gosu::IF_SMOOTH); }
    catch (...) {}
    if (!cueImg_) { try { cueImg_ = std::make_unique<Gosu::Image>("Queue.png", Gosu::IF_SMOOTH); } catch (...) {} }

    // Kugeln 0..15
    for (int i = 0; i < 16; ++i) {
        auto name = ballFile(i);
        ballImg_[i] = try_load_variants(name, kBallImageFlags);
        if (!ballImg_[i]) {
            try { ballImg_[i] = std::make_unique<Gosu::Image>(std::to_string(i) + ".png", kBallImageFlags); }
            catch (...) {}
        }
    }
}

std::string GameWindow::ballFile(int id) {
    switch (id) {
    case  0: return "Kugel_0_weiss.png";
    case  1: return "Kugel_1_gelb_voll.png";
    case  2: return "Kugel_2_blau_voll.png";
    case  3: return "Kugel_3_rot_voll.png";
    case  4: return "Kugel_4_lila_voll.png";
    case  5: return "Kugel_5_orange_voll.png";
    case  6: return "Kugel_6_tuerkis_voll.png";
    case  7: return "Kugel_7_weinrot_voll.png";
    case  8: return "Kugel_8_schwarz_voll.png";
    case  9: return "Kugel_9_gelb_halb.png";
    case 10: return "Kugel_10_blau_halb.png";
    case 11: return "Kugel_11_rot_halb.png";
    case 12: return "Kugel_12_lila_halb.png";
    case 13: return "Kugel_13_orange_halb.png";
    case 14: return "Kugel_14_tuerkis_halb.png";
    case 15: return "Kugel_15_weinrot_halb.png";
    default: return "";
    }
}

double GameWindow::rail() const {
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
    double pad = std::max(20.0, std::min(width(), height()) * 0.07);
    double availW = std::max(1.0, width() - 2 * pad);
    double availH = std::max(1.0, height() - 2 * pad);

    const double aspect = 2.0;
    double w = availW, h = w / aspect;
    if (h > availH) { h = availH; w = h * aspect; }

    tableW_ = w; tableH_ = h;
    tableX_ = (width() - tableW_) / 2.0;
    tableY_ = (height() - tableH_) / 2.0;
}

void GameWindow::layoutStartBoxes() {
    computeTableRect();
    double r = rail(), bh = std::max(36.0, height() * 0.06);
    double bx = tableX_ + tableW_ * 0.18, bw = tableW_ * 0.64;
    double y1 = tableY_ + r + tableH_ * 0.20, y2 = y1 + bh + std::max(24.0, height() * 0.05);
    p1Box_ = { bx, y1, bw, bh };
    p2Box_ = { bx, y2, bw, bh };
}

// ---------- PNG ANALYSE ----------
void GameWindow::analyzeTableImage() {
    if (!felt_) return;

    Gosu::Bitmap bmp = felt_->data().to_bitmap();
    const int imgW = (int)bmp.width();
    const int imgH = (int)bmp.height();

    int minX = imgW, minY = imgH, maxX = 0, maxY = 0;
    for (int y = 0; y < imgH; ++y) for (int x = 0; x < imgW; ++x) {
        Gosu::Color c = bmp.get_pixel(x, y);
        int r = c.red(), g = c.green(), b = c.blue();
        if (isGreen(r, g, b)) {
            if (x < minX) minX = x; if (x > maxX) maxX = x;
            if (y < minY) minY = y; if (y > maxY) maxY = y;
        }
    }
    if (minX < maxX && minY < maxY) {
        feltNorm_.l = double(minX) / imgW; feltNorm_.r = double(maxX) / imgW;
        feltNorm_.t = double(minY) / imgH; feltNorm_.b = double(maxY) / imgH;
        feltOk_ = true;
    }
    else {
        feltNorm_ = { 0.08,0.08,0.92,0.92 };
        feltOk_ = false;
    }

    const double cx = (feltNorm_.l + feltNorm_.r) * 0.5;
    const double cy = (feltNorm_.t + feltNorm_.b) * 0.5;
    const std::array<std::pair<double, double>, 6> guesses = { {
        {feltNorm_.l, feltNorm_.t}, {feltNorm_.r, feltNorm_.t},
        {feltNorm_.l, feltNorm_.b}, {feltNorm_.r, feltNorm_.b},
        {cx,           feltNorm_.t}, {cx,           feltNorm_.b}
    } };

    const int win = std::max(10, std::min(imgW, imgH) / 20);
    auto clampi = [](int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); };

    for (int i = 0; i < 6; ++i) {
        int gx = int(guesses[i].first * imgW), gy = int(guesses[i].second * imgH);
        int x0 = clampi(gx - win, 0, imgW - 1), x1 = clampi(gx + win, 0, imgW - 1);
        int y0 = clampi(gy - win, 0, imgH - 1), y1 = clampi(gy + win, 0, imgH - 1);

        long long sx = 0, sy = 0, n = 0; double maxR2 = 0.0;

        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
            Gosu::Color c = bmp.get_pixel(x, y);
            if (!isDark(c.red(), c.green(), c.blue())) continue;
            sx += x; sy += y; ++n;
        }

        double cxp = gx, cyp = gy;
        if (n > 50) {
            cxp = double(sx) / n; cyp = double(sy) / n;
            for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
                Gosu::Color c = bmp.get_pixel(x, y);
                if (!isDark(c.red(), c.green(), c.blue())) continue;
                double dx = x - cxp, dy = y - cyp; double d2 = dx * dx + dy * dy; if (d2 > maxR2) maxR2 = d2;
            }
        }
        else {
            maxR2 = (win * 0.6) * (win * 0.6);
        }

        pocketsNorm_[i].nx = cxp / imgW;
        pocketsNorm_[i].ny = cyp / imgH;
        pocketsNorm_[i].nr = std::max(0.01, std::min(0.08, std::sqrt(maxR2) / double(std::min(imgW, imgH))));
    }
}

// ---------- Normdaten -> Bildschirm & Game ----------
void GameWindow::rebuildGameGeomFromNorm() {
    double ix = tableX_ + feltNorm_.l * tableW_;
    double iy = tableY_ + feltNorm_.t * tableH_;
    double iw = (feltNorm_.r - feltNorm_.l) * tableW_;
    double ih = (feltNorm_.b - feltNorm_.t) * tableH_;
    game_.set_playfield(ix, iy, ix + iw, iy + ih);

    std::vector<PocketGeom> P; P.reserve(6);
    double shortEdge = std::min(tableW_, tableH_);
    for (const auto& pn : pocketsNorm_) {
        double px = tableX_ + pn.nx * tableW_;
        double py = tableY_ + pn.ny * tableH_;
        double pr = pn.nr * shortEdge;
        P.push_back({ px, py, pr });
    }
    game_.set_pockets(P);
}

void GameWindow::update() {
    ensureFonts();
    computeTableRect();

    static bool analyzed = false;
    if (!analyzed) { analyzeTableImage(); analyzed = true; }

    game_.W = width(); game_.H = height();
    rebuildGameGeomFromNorm();

    if (state_ != UiState::Playing) return;

    aimX_ = input().mouse_x();
    aimY_ = input().mouse_y();

    if (dragging_ && game_.allStopped()) {
        double dx = aimX_ - game_.cue().x, dy = aimY_ - game_.cue().y;
        double d = std::sqrt(dx * dx + dy * dy);
        power_ = clamp01(std::min(300.0, d) / 300.0);
    }

    if (cueAnimFrames_ > 0) --cueAnimFrames_;

    game_.update();
    if (game_.isOver()) state_ = UiState::GameOver;
}

// Hilfs-Kreis (Schatten/Outline)
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

    if (cueImg_) {
        const double baseBack = 40.0;
        double back = baseBack + 160.0 * power_;
        if (cueAnimFrames_ > 0) {
            double t = cueAnimFrames_ / 10.0;
            back *= t;
        }

        double angleDeg = std::atan2(dy, dx) * 180.0 / 3.14159265;
        double s = std::max(0.35, tableH_ / 900.0);
        double cueLen = cueImg_->width() * s;

        double cxp = c.x - nx * (c.r + back + cueLen * 0.5);
        double cyp = c.y - ny * (c.r + back + cueLen * 0.5);

        cueImg_->draw_rot(cxp, cyp, 4, angleDeg, 0.5, 0.5, s, s);
    }
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
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color::BLACK, 0);

    if (felt_) {
        double sx = tableW_ / felt_->width();
        double sy = tableH_ / felt_->height();
        felt_->draw(tableX_, tableY_, 2, sx, sy);
    }
    else {
        Gosu::Graphics::draw_rect(tableX_, tableY_, tableW_, tableH_, Gosu::Color(30, 120, 40, 255), 2);
    }

    std::string title = "Billard – Namen eingeben";
    double tw = fontTitle_->text_width(title);
    drawTextShadow(*fontTitle_, title, (width() - tw) / 2, tableY_ + rail() * 0.5, 3, Gosu::Color::WHITE);

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
    drawTextShadow(*font_, hint, (width() - hw) / 2, tableY_ + tableH_ - rail() - font_->height() - 6, 4, Gosu::Color::WHITE);
}

void GameWindow::drawPause() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color(170, 0, 0, 0), 10);
    std::string title = "PAUSE";
    double tw = fontTitle_->text_width(title);
    drawTextShadow(*fontTitle_, title, (width() - tw) / 2, tableY_ + rail() + 20, 11, Gosu::Color::WHITE);

    std::string info = "P: Zurueck   |   N: Neues Spiel   |   ESC: Beenden";
    double iw = font_->text_width(info);
    drawTextShadow(*font_, info, (width() - iw) / 2, tableY_ + rail() + 20 + fontTitle_->height() + 8, 11,
        Gosu::Color(255, 220, 0, 255));
}

void GameWindow::drawGameOver() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color(150, 0, 0, 0), 10);
    int win = game_.winner();
    std::string winner = (win == 0 ? p1Name_ : p2Name_) + std::string(" hat gewonnen!");
    double ww = fontTitle_->text_width(winner);
    drawTextShadow(*fontTitle_, winner, (width() - ww) / 2, tableY_ + rail() + 20, 11, Gosu::Color::WHITE);

    std::string info = "N: Neues Match   |   R: Neu aufbauen   |   ESC: Beenden";
    double iw = font_->text_width(info);
    drawTextShadow(*font_, info, (width() - iw) / 2, tableY_ + rail() + 20 + fontTitle_->height() + 8, 11,
        Gosu::Color(255, 220, 0, 255));
}

void GameWindow::draw() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(), Gosu::Color::BLACK, 0);

    // Tisch
    if (felt_) {
        double sx = tableW_ / felt_->width();
        double sy = tableH_ / felt_->height();
        felt_->draw(tableX_, tableY_, 1, sx, sy);
    }
    else {
        Gosu::Graphics::draw_rect(tableX_, tableY_, tableW_, tableH_, Gosu::Color(30, 120, 40, 255), 1);
    }

    // Taschen-Overlay
    Gosu::Color pc = Gosu::Color::BLACK;
    for (const auto& pn : pocketsNorm_) {
        double px = tableX_ + pn.nx * tableW_;
        double py = tableY_ + pn.ny * tableH_;
        double pr = pn.nr * std::min(tableW_, tableH_);
        drawCircle(px, py, pr, pc, 3, kBallSegments);
    }

    // --- Bälle zeichnen ---
    auto drawBall = [&](const Ball& b) {
        if (!b.inPlay) return;

        // Schatten & Outline
        drawCircle(b.x + kBallShadow_dx, b.y + kBallShadow_dy, b.r * kBallShadowMul, Gosu::Color(90, 0, 0, 0), 3, kBallSegments);
        drawCircle(b.x, b.y, b.r + 1.0, Gosu::Color(200, 0, 0, 0), 4, kBallSegments);

        auto& img = ballImg_[b.id];
        if (img) {
            double sx = kBallImageScale * (2 * b.r) / img->width();
            double sy = kBallImageScale * (2 * b.r) / img->height();
            img->draw_rot(b.x, b.y, 5, 0.0, 0.5, 0.5, sx, sy);
        }
        else {
            drawCircle(b.x, b.y, b.r, Gosu::Color::WHITE, 5, kBallSegments);
        }
        };

    for (const Ball& b : game_.balls()) drawBall(b);
    drawBall(game_.cue());

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

    if (b >= Gosu::KB_A && b <= Gosu::KB_Z) { char ch = 'A' + (int(b) - int(Gosu::KB_A)); s.push_back(ch); return; }
    if (b >= Gosu::KB_0 && b <= Gosu::KB_9) { char ch = '0' + (int(b) - int(Gosu::KB_0)); s.push_back(ch); return; }
    if (b == Gosu::KB_SPACE) { s.push_back(' '); return; }
    if (b == Gosu::KB_MINUS) { s.push_back('-'); return; }
}

void GameWindow::button_down(Gosu::Button b) {
    Gosu::Window::button_down(b);

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

    if (b == Gosu::MS_LEFT && state_ == UiState::Playing && game_.allStopped() && !game_.isOver())
        dragging_ = true;

    if (b == Gosu::KB_P && (state_ == UiState::Playing || state_ == UiState::Paused)) togglePause();
    if (b == Gosu::KB_R && state_ != UiState::Start) game_.reset(true);
    if (b == Gosu::KB_N && state_ != UiState::Start) { game_.reset(false); state_ = UiState::Playing; }
    if (b == Gosu::KB_ESCAPE) close();
}

void GameWindow::button_up(Gosu::Button b) {
    if (b == Gosu::MS_LEFT && dragging_) { shootFromAim(); }
    dragging_ = false;
}
