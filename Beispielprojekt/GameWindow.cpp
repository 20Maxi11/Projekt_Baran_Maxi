#include "GameWindow.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <Gosu/Bitmap.hpp>

// kleine Hilfsfunktion für 0..1 Begrenzung
static inline double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }

// PNG-Heuristiken: grün / dunkel
static inline bool isGreen(int r, int g, int b) {
    return g > 70 && g > int(r * 1.15) && g > int(b * 1.15);
}
static inline bool isDark(int r, int g, int b) {
    return r < 40 && g < 40 && b < 40;
}

// ---------- Optionen ----------
static constexpr double kBallImageScale = 1.00;
static constexpr double kBallShadowMul = 1.06;
static constexpr double kBallShadow_dx = 2.0;
static constexpr double kBallShadow_dy = 2.0;
static constexpr int    kBallSegments = 28;

// Queue-Parameter (Rückzug usw.)
static constexpr double kCueMaxBackPx = 320.0;
static constexpr double kCueMinBackPx = 40.0;
static constexpr int    kCueFireFrames = 10;
static constexpr double kCueThicknessPx = 8.0;

// Für Ball-Bilder
static constexpr unsigned kBallImageFlags =
#ifdef IF_RETRO
Gosu::IF_RETRO;
#else
Gosu::IF_SMOOTH;
#endif

// ---------- Hilfsfunktionen für robuste Bildnamen ----------
static std::string ascii_fallback(std::string s) {
    auto repl = [&](const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
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
        catch (...) {}
    }
    return nullptr;
}

// ======================================================
//  Konstruktor
// ======================================================
GameWindow::GameWindow(unsigned width, unsigned height, int /*players*/, bool fullscreen)
    : Gosu::Window(width, height, fullscreen, 31.0),
    game_(width, height, 2)
{
    set_caption("Gosu Billard");
    loadAssets();
    loadSounds();
    ensureFonts();
    computeTableRect();

    // --- Avatar-Dateien vorbereiten ---
    avatarFiles_ = {
        "avatar1.png",
        "avatar2.png",
        "avatar3.png",
        "avatar4.png",
        "avatar5.png",
        "avatar6.png",
        "avatar7.png",
        "avatar8.png",
        "avatar9.png",
        "avatar10.png",
        "avatar11.png",
        "avatar12.png"
    };
    avatarImgs_.resize(avatarFiles_.size());
    for (std::size_t i = 0; i < avatarFiles_.size(); ++i) {
        try {
            avatarImgs_[i] = std::make_unique<Gosu::Image>(avatarFiles_[i], Gosu::IF_SMOOTH);
        }
        catch (...) {
            avatarImgs_[i].reset(); // falls Bild fehlt
        }
    }
    avatarIndex_[0] = 0;
    avatarIndex_[1] = 0;
}

// ======================================================
//  Assets laden (Grafik)
// ======================================================
void GameWindow::loadAssets() {
    // Tischbild
    try { felt_ = try_load_variants("Tisch.png", Gosu::IF_SMOOTH); }
    catch (...) {}
    if (!felt_) {
        try { felt_ = std::make_unique<Gosu::Image>("Tisch.png", Gosu::IF_SMOOTH); }
        catch (...) {}
    }

    // Queuebild
    try { cueImg_ = try_load_variants("Queue.png", Gosu::IF_SMOOTH); }
    catch (...) {}
    if (!cueImg_) {
        try { cueImg_ = std::make_unique<Gosu::Image>("Queue.png", Gosu::IF_SMOOTH); }
        catch (...) {}
    }

    // Kugelbilder 0..15
    for (int i = 0; i < 16; ++i) {
        auto name = ballFile(i);
        ballImg_[i] = try_load_variants(name, kBallImageFlags);
        if (!ballImg_[i]) {
            try { ballImg_[i] = std::make_unique<Gosu::Image>(std::to_string(i) + ".png", kBallImageFlags); }
            catch (...) {}
        }
    }
}

// ======================================================
//  Sound laden
// ======================================================
void GameWindow::loadSounds() {
    // Ball-Ball Kontakt
    try { sfxBall_ = std::make_unique<Gosu::Sample>("billiard_ball_clack.wav"); }
    catch (...) { sfxBall_.reset(); }

    // Ball in Tasche
    try { sfxPocket_ = std::make_unique<Gosu::Sample>("ball_in_pocket.wav"); }
    catch (...) { sfxPocket_.reset(); }

    // Bande (vorerst gleiche Datei wie Ball-Ball)
    try { sfxRail_ = std::make_unique<Gosu::Sample>("billiard_ball_clack.wav"); }
    catch (...) { sfxRail_.reset(); }

    // Jubeln bei Spielende
    try { sfxWin_ = std::make_unique<Gosu::Sample>("crowd_cheering.mp3"); }
    catch (...) { sfxWin_.reset(); }

    sfxVolume_ = 1.0; // Grundlautstärke
    muted_ = false;   // Sound an
}

// Dateiname für Kugel-ID
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

// ======================================================
//  Layout
// ======================================================
double GameWindow::rail() const {
    // Bandendicke aus Tischgröße, aber begrenzt
    double r = std::min(tableW_, tableH_) * 0.05;
    return std::max(18.0, std::min(56.0, r));
}

void GameWindow::ensureFonts() {
    // Fonts nur neu erstellen, wenn Fenstergröße neu ist
    if (font_ && fontTitle_ && lastW_ == width() && lastH_ == height()) return;
    lastW_ = width();
    lastH_ = height();
    int hud = std::max(16, (int)std::round(height() * 0.035));
    int title = std::max(26, (int)std::round(height() * 0.065));
    font_ = std::make_unique<Gosu::Font>(hud, Gosu::default_font_name());
    fontTitle_ = std::make_unique<Gosu::Font>(title, Gosu::default_font_name());
}

void GameWindow::computeTableRect() {
    // Tisch zentriert im Fenster, Seitenverhältnis 2:1
    double pad = std::max(20.0, std::min(width(), height()) * 0.07);
    double availW = std::max(1.0, width() - 2 * pad);
    double availH = std::max(1.0, height() - 2 * pad);

    const double aspect = 2.0;
    double w = availW, h = w / aspect;
    if (h > availH) { h = availH; w = h * aspect; }

    tableW_ = w;
    tableH_ = h;
    tableX_ = (width() - tableW_) / 2.0;
    tableY_ = (height() - tableH_) / 2.0;
}

void GameWindow::layoutStartBoxes() {
    // Name-Boxen und Avatar-Pfeile im Startscreen
    computeTableRect();
    double r = rail();
    double bh = std::max(36.0, height() * 0.06);
    double bx = tableX_ + tableW_ * 0.18;
    double bw = tableW_ * 0.64;
    double y1 = tableY_ + r + tableH_ * 0.20;
    double y2 = y1 + bh + std::max(24.0, height() * 0.05);

    p1Box_ = { bx, y1, bw, bh };
    p2Box_ = { bx, y2, bw, bh };

    // Avatar-Kreis über der Box, mit <Avatar> Pfeilen
    for (int i = 0; i < 2; ++i) {
        const RectF& box = (i == 0) ? p1Box_ : p2Box_;
        double avatarR = box.h * 0.5;
        double avatarCx = box.x + box.w / 2.0;
        double avatarCy = box.y - avatarR - 10.0;

        double arrowW = avatarR * 0.8;
        double arrowH = avatarR * 0.8;

        avatarLeft_[i] = { avatarCx - avatarR * 2.0, avatarCy - arrowH / 2.0, arrowW, arrowH };
        avatarRight_[i] = { avatarCx + avatarR * 1.2, avatarCy - arrowH / 2.0, arrowW, arrowH };
    }
}

void GameWindow::layoutHudButtons() {
    // Pause- und Mute-Buttons oben rechts
    double size = std::max(24.0, height() * 0.04);
    double pad = 8.0;

    muteBtn_ = { (double)width() - pad - size, pad, size, size };
    pauseBtn_ = { (double)width() - pad * 2 - size * 2, pad, size, size };
}

// ======================================================
//  PNG analysieren + Bande verschmälern + Geometrie setzen
// ======================================================
void GameWindow::analyzeTableImage() {
    if (!felt_) return;

    Gosu::Bitmap bmp = felt_->data().to_bitmap();
    const int imgW = (int)bmp.width();
    const int imgH = (int)bmp.height();

    int minX = imgW, minY = imgH, maxX = 0, maxY = 0;
    for (int y = 0; y < imgH; ++y) {
        for (int x = 0; x < imgW; ++x) {
            Gosu::Color c = bmp.get_pixel(x, y);
            int r = c.red(), g = c.green(), b = c.blue();
            if (isGreen(r, g, b)) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (minX < maxX && minY < maxY) {
        // normiertes Rechteck des Tuchs
        feltNorm_.l = double(minX) / imgW;
        feltNorm_.r = double(maxX) / imgW;
        feltNorm_.t = double(minY) / imgH;
        feltNorm_.b = double(maxY) / imgH;

        // Bande schmäler machen (deine gemessenen Werte)
        double cmToRelW = 0.03790854628568;   // oben/unten
        double cmToRelH = 0.02071661436276;   // links/rechts
        double shrinkLR = cmToRelH * (feltNorm_.r - feltNorm_.l);
        double shrinkTB = cmToRelW * (feltNorm_.b - feltNorm_.t);

        feltNorm_.l += shrinkLR;
        feltNorm_.r -= shrinkLR;
        feltNorm_.t += shrinkTB;
        feltNorm_.b -= shrinkTB;

        feltOk_ = true;
    }
    else {
        // Fallback-Rechteck
        feltNorm_ = { 0.08, 0.08, 0.92, 0.92 };
        feltOk_ = false;
    }
}

void GameWindow::rebuildGameGeomFromNorm() {
    // Tuch-Koordinaten in Fensterkoordinaten umsetzen
    double ix = tableX_ + feltNorm_.l * tableW_;
    double iy = tableY_ + feltNorm_.t * tableH_;
    double iw = (feltNorm_.r - feltNorm_.l) * tableW_;
    double ih = (feltNorm_.b - feltNorm_.t) * tableH_;
    game_.set_playfield(ix, iy, ix + iw, iy + ih);
}

// ======================================================
//  Sounds aus Game abspielen
// ======================================================
void GameWindow::playSoundEvents() {
    const auto& evs = game_.soundEvents();
    for (const auto& ev : evs) {
        double base = std::max(0.0, std::min(1.0, ev.volume));
        double vol = base * (muted_ ? 0.0 : sfxVolume_);
        switch (ev.type) {
        case SoundEventType::BallBall:
            if (sfxBall_) sfxBall_->play(vol);
            break;
        case SoundEventType::RailHit:
            if (sfxRail_)      sfxRail_->play(vol);
            else if (sfxBall_) sfxBall_->play(vol * 0.8);
            break;
        case SoundEventType::Pocket:
            if (sfxPocket_)    sfxPocket_->play(vol);
            else if (sfxBall_) sfxBall_->play(vol * 0.8);
            break;
        case SoundEventType::GameOver:
            if (sfxWin_) sfxWin_->play(1.0);
            break;
        }
    }
    game_.clearSoundEvents();
}

// ======================================================
//  Update
// ======================================================
void GameWindow::update() {
    ensureFonts();
    computeTableRect();
    layoutHudButtons(); // Buttons je nach Fenstergröße setzen

    // einmalig das PNG analysieren
    static bool analyzed = false;
    if (!analyzed) { analyzeTableImage(); analyzed = true; }

    // Spielfeld an aktuelle Fenstergröße koppeln
    game_.W = width();
    game_.H = height();
    rebuildGameGeomFromNorm();

    // Cursor-Blinken für Namenseingabe
    cursorTimer_ += 1.0 / 60.0;
    if (cursorTimer_ >= 0.5) {
        cursorVisible_ = !cursorVisible_;
        cursorTimer_ = 0.0;
    }

    // Backspace-Auto-Repeat
    if (backspaceHeld_ && state_ == UiState::Start) {
        backspaceTimer_ += 1.0 / 60.0;
        if (backspaceTimer_ > 0.10) {
            std::string& s = (activeName_ == 0) ? p1Name_ : p2Name_;
            if (!s.empty()) s.pop_back();
            backspaceTimer_ = 0.06; 
        }
    }

    // Wenn wir nicht im aktiven Spiel sind, trotzdem Physik & Sounds updaten
    if (state_ != UiState::Playing) {
        game_.update();
        playSoundEvents();
        if (game_.isOver()) state_ = UiState::GameOver;
        return;
    }

    // --- aktive Spielphase ---
    aimX_ = input().mouse_x();
    aimY_ = input().mouse_y();

    // Queue-Ziehen, solange alle Kugeln stehen
    if (dragging_ && game_.allStopped()) {
        const Ball& c = game_.cue();
        double dx = aimX_ - c.x;
        double dy = aimY_ - c.y;
        double d = std::max(1.0, std::sqrt(dx * dx + dy * dy));
        double nx = dx / d;
        double ny = dy / d;
        double proj = dx * nx + dy * ny;
        double pull = std::max(0.0, proj - cuePullStart_);
        cueBackDist_ = std::min(kCueMaxBackPx, kCueMinBackPx + pull);
        power_ = clamp01(cueBackDist_ / kCueMaxBackPx);
    }

    // Queue-Vorwärts-Anim nach Stoß
    if (cueAnimFrames_ > 0) {
        double t = (double)cueAnimFrames_ / (double)kCueFireFrames;
        cueBackDist_ *= t * t;
        --cueAnimFrames_;
    }
    if (!dragging_ && cueAnimFrames_ == 0) {
        cueBackDist_ = 0.0;
    }

    game_.update();
    playSoundEvents();

    if (game_.isOver()) {
        state_ = UiState::GameOver;
    }
}

// ======================================================
//  Zeichnen
// ======================================================
void GameWindow::drawCircle(double cx, double cy, double r,
    Gosu::Color col, double z, int seg) {
    const double step = 2 * 3.14159265358979323846 / seg;
    double px = cx + r, py = cy;
    for (int i = 1; i <= seg; ++i) {
        double a = step * i;
        double nx = cx + std::cos(a) * r;
        double ny = cy + std::sin(a) * r;
        Gosu::Graphics::draw_triangle(cx, cy, col, px, py, col, nx, ny, col, z);
        px = nx; py = ny;
    }
}

void GameWindow::drawTextShadow(Gosu::Font& f, const std::string& s,
    double x, double y, double z, Gosu::Color col) {
    f.draw_text(s, x + 1, y + 1, z, 1.0, 1.0, Gosu::Color(200, 0, 0, 0));
    f.draw_text(s, x, y, z, 1.0, 1.0, col);
}

// Avatar als Kreis (mit Bild) zeichnen
void GameWindow::drawAvatarCircle(double cx, double cy, double radius,
    Gosu::Image* img, double z) {
    // Schatten hinter dem Avatar-Kreis
    drawCircle(cx + 2.0, cy + 2.0, radius * 1.02,
        Gosu::Color(100, 0, 0, 0), z, 28);

    if (img) {
        // immer nach der größten Kante skalieren -> alle gleich großer Kreis
        double iw = (double)img->width();
        double ih = (double)img->height();
        double base = std::max(iw, ih);          // größte Bildkante
        double s = (2.0 * radius) / base;        // ein einheitlicher Scale-Faktor
        img->draw_rot(cx, cy, z + 0.1, 0.0,
            0.5, 0.5, s, s);
    }
    else {
        // Platzhalter-Kreis, falls kein Bild geladen werden konnte
        drawCircle(cx, cy, radius,
            Gosu::Color(255, 200, 200, 200), z, 28);
    }

    /*dünner Rand - Kreis oben drauf(Avatar - Rahmen)
    drawCircle(cx, cy, radius,
        Gosu::Color(200, 255, 255, 255), z + 0.2, 32);*/
}

void GameWindow::drawAim() {
    if (!game_.allStopped() || game_.isOver() || !cueImg_) return;

    const Ball& c = game_.cue();
    double dx = aimX_ - c.x;
    double dy = aimY_ - c.y;
    double d = std::sqrt(dx * dx + dy * dy);
    if (d < 1.0) return;

    double nx = dx / d;
    double ny = dy / d;

    double s = kCueThicknessPx / std::max(1.0, (double)cueImg_->height());
    double cueLen = cueImg_->width() * s;

    double safeGap = c.r * 0.25;
    double offset = (c.r + safeGap + cueBackDist_ + cueLen * 0.5);

    double cxp = c.x + nx * offset;
    double cyp = c.y + ny * offset;
    double angleDeg = std::atan2(dy, dx) * 180.0 / 3.14159265;

    cueImg_->draw_rot(cxp, cyp, 4, angleDeg, 0.5, 0.5, s, s);
}

void GameWindow::drawHud() {
    double barH = std::max(32.0, height() * 0.06);
    double y = height() - (barH + 6);
    Gosu::Graphics::draw_rect(0, y, width(), barH + 6,
        Gosu::Color(180, 0, 0, 0), 5);

    int turn = game_.currentPlayer();

    // Avatar im HUD (links/rechts neben Namen)
    double radiusHud = barH * 0.4;
    double centerY = y + barH / 2.0;

    Gosu::Image* ava1 = nullptr;
    Gosu::Image* ava2 = nullptr;
    if (!avatarImgs_.empty()) {
        int n = (int)avatarImgs_.size();
        int i1 = ((avatarIndex_[0] % n) + n) % n;
        int i2 = ((avatarIndex_[1] % n) + n) % n;
        if (avatarImgs_[i1]) ava1 = avatarImgs_[i1].get();
        if (avatarImgs_[i2]) ava2 = avatarImgs_[i2].get();
    }

    double ava1x = 12.0 + radiusHud;
    double ava2x = width() - 12.0 - radiusHud;

    if (ava1) drawAvatarCircle(ava1x, centerY, radiusHud, ava1, 6.0);
    if (ava2) drawAvatarCircle(ava2x, centerY, radiusHud, ava2, 6.0);

    // Texte mit Abstand zu den Avataren
    double leftTextX = ava1 ? (ava1x + radiusHud + 8.0) : 12.0;
    double rightTextX = ava2 ? (ava2x - radiusHud - 8.0) : (double)width() - 12.0;

    std::string left = " " + p1Name_;
    std::string right = " " + p2Name_;

    // Gruppenanzeige nur, wenn Gruppen wirklich vergeben sind
    if (game_.groupsAssigned()) {
        auto g0 = game_.groupOfPlayer(0);
        auto g1 = game_.groupOfPlayer(1);
        int solidLeft = game_.remainingSolids();
        int stripeLeft = game_.remainingStripes();

        if (g0) {
            if (g0.value() == BallType::SOLID)
                left += "  (volle: " + std::to_string(solidLeft) + ")";
            else
                left += "  (halbe: " + std::to_string(stripeLeft) + ")";
        }
        if (g1) {
            if (g1.value() == BallType::SOLID)
                right += "  (volle: " + std::to_string(solidLeft) + ")";
            else
                right += "  (halbe: " + std::to_string(stripeLeft) + ")";
        }
    }

    Gosu::Color colL = (turn == 0)
        ? Gosu::Color::WHITE
        : Gosu::Color(220, 220, 220, 255);
    Gosu::Color colR = (turn == 1)
        ? Gosu::Color::WHITE
        : Gosu::Color(220, 220, 220, 255);

    drawTextShadow(*font_, left, leftTextX, y + 6, 6, colL);
    double rw = font_->text_width(right);
    drawTextShadow(*font_, right, rightTextX - rw, y + 6, 6, colR);

    // Mitte: wer ist am Zug
    std::string mid = (turn == 0 ? p1Name_ : p2Name_) + " ist am Zug";
    double mw = font_->text_width(mid);
    drawTextShadow(*font_, mid,
        (width() - mw) / 2.0, y + 6,
        6, Gosu::Color(255, 220, 0, 255));

    // --- Pause- und Mute-Buttons zeichnen (oben rechts) ---
    Gosu::Graphics::draw_rect(pauseBtn_.x, pauseBtn_.y,
        pauseBtn_.w, pauseBtn_.h,
        Gosu::Color(180, 0, 0, 0), 7);
    Gosu::Graphics::draw_rect(muteBtn_.x, muteBtn_.y,
        muteBtn_.w, muteBtn_.h,
        Gosu::Color(muted_ ? 220 : 80, 0, 0, 0), 7);

    // kleines Text-Symbol in den Buttons
    std::string pauseText = "||";             // Pause-Symbol
    double pw = font_->text_width(pauseText);
    drawTextShadow(*font_, pauseText,
        pauseBtn_.x + (pauseBtn_.w - pw) / 2.0,
        pauseBtn_.y + 2,
        8, Gosu::Color::WHITE);

    std::string muteText = muted_ ? "X" : "S"; // X = stumm, S = Sound
    double mwb = font_->text_width(muteText);
    drawTextShadow(*font_, muteText,
        muteBtn_.x + (muteBtn_.w - mwb) / 2.0,
        muteBtn_.y + 2,
        8, Gosu::Color::WHITE);
}

void GameWindow::drawStart() {
    layoutStartBoxes();
    Gosu::Graphics::draw_rect(0, 0, width(), height(),
        Gosu::Color::BLACK, 0);

    // Tischbild im Hintergrund
    if (felt_) {
        double sx = tableW_ / felt_->width();
        double sy = tableH_ / felt_->height();
        felt_->draw(tableX_, tableY_, 2, sx, sy);
    }
    else {
        Gosu::Graphics::draw_rect(tableX_, tableY_, tableW_, tableH_,
            Gosu::Color(30, 120, 40, 255), 2);
    }

    // Titel
    std::string title = "Billard – Namen & Avatare";
    double tw = fontTitle_->text_width(title);
    drawTextShadow(*fontTitle_, title,
        (width() - tw) / 2.0, tableY_ + rail() * 0.5,
        3, Gosu::Color::WHITE);

    // Rahmen um Eingabeboxen
    Gosu::Color frame(200, 50, 50, 50);
    Gosu::Graphics::draw_rect(p1Box_.x, p1Box_.y, p1Box_.w, p1Box_.h, frame, 3);
    Gosu::Graphics::draw_rect(p2Box_.x, p2Box_.y, p2Box_.w, p2Box_.h, frame, 3);

    drawTextShadow(*font_, "Spieler 1:",
        p1Box_.x,
        p1Box_.y - font_->height() - 6,
        4, Gosu::Color::WHITE);
    drawTextShadow(*font_, "Spieler 2:",
        p2Box_.x,
        p2Box_.y - font_->height() - 6,
        4, Gosu::Color::WHITE);

    Gosu::Color c1 = (activeName_ == 0)
        ? Gosu::Color(255, 255, 220, 0)
        : Gosu::Color::WHITE;
    Gosu::Color c2 = (activeName_ == 1)
        ? Gosu::Color(255, 255, 220, 0)
        : Gosu::Color::WHITE;

    // Cursor in Namenseingabe-Feld
    std::string p1Show = p1Name_;
    if (activeName_ == 0 && cursorVisible_) p1Show += "|";
    std::string p2Show = p2Name_;
    if (activeName_ == 1 && cursorVisible_) p2Show += "|";

    drawTextShadow(*font_, p1Show,
        p1Box_.x + 10,
        p1Box_.y + (p1Box_.h - font_->height()) / 2,
        4, c1);
    drawTextShadow(*font_, p2Show,
        p2Box_.x + 10,
        p2Box_.y + (p2Box_.h - font_->height()) / 2,
        4, c2);

    // Avatare zeichnen: < Avatar >
    for (int i = 0; i < 2; ++i) {
        const RectF& box = (i == 0) ? p1Box_ : p2Box_;
        const RectF& leftArrow = avatarLeft_[i];
        const RectF& rightArrow = avatarRight_[i];

        double avatarR = box.h * 0.5;
        double avatarCx = box.x + box.w / 2.0;
        double avatarCy = box.y - avatarR - 10.0;

        Gosu::Image* img = nullptr;
        if (!avatarImgs_.empty()) {
            int n = (int)avatarImgs_.size();
            int idx = ((avatarIndex_[i] % n) + n) % n;
            if (avatarImgs_[idx]) img = avatarImgs_[idx].get();
        }

        drawAvatarCircle(avatarCx, avatarCy, avatarR, img, 4.0);

        // linke/right Pfeilflächen zeichnen
        Gosu::Graphics::draw_rect(leftArrow.x, leftArrow.y,
            leftArrow.w, leftArrow.h,
            Gosu::Color(160, 0, 0, 0), 3);
        Gosu::Graphics::draw_rect(rightArrow.x, rightArrow.y,
            rightArrow.w, rightArrow.h,
            Gosu::Color(160, 0, 0, 0), 3);

        std::string lt = "<";
        std::string rt = ">";
        double ltw = font_->text_width(lt);
        double rtw = font_->text_width(rt);

        drawTextShadow(*font_, lt,
            leftArrow.x + (leftArrow.w - ltw) / 2.0,
            leftArrow.y,
            5, Gosu::Color::WHITE);
        drawTextShadow(*font_, rt,
            rightArrow.x + (rightArrow.w - rtw) / 2.0,
            rightArrow.y,
            5, Gosu::Color::WHITE);
    }

    // Hinweistext
    std::string hint =
        "ENTER: Start   |   KLICK/TAB: Feld/Avatar wechseln   |   ESC: Beenden";
    double hw = font_->text_width(hint);
    drawTextShadow(*font_, hint,
        (width() - hw) / 2.0,
        tableY_ + tableH_ - rail() - font_->height() - 6,
        4, Gosu::Color::WHITE);
}

void GameWindow::drawPause() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(),
        Gosu::Color(170, 0, 0, 0), 10);
    std::string title = "PAUSE";
    double tw = fontTitle_->text_width(title);
    drawTextShadow(*fontTitle_, title,
        (width() - tw) / 2.0,
        tableY_ + rail() + 20,
        11, Gosu::Color::WHITE);

    std::string info = "P: Zurueck   |   N: Neues Spiel   |   ESC: Beenden";
    double iw = font_->text_width(info);
    drawTextShadow(*font_, info,
        (width() - iw) / 2.0,
        tableY_ + rail() + 20 + fontTitle_->height() + 8,
        11, Gosu::Color(255, 220, 0, 255));
}

void GameWindow::drawGameOver() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(),
        Gosu::Color(150, 0, 0, 0), 10);
    int win = game_.winner();
    std::string winner = (win == 0 ? p1Name_ : p2Name_) + " hat gewonnen!";
    double ww = fontTitle_->text_width(winner);
    drawTextShadow(*fontTitle_, winner,
        (width() - ww) / 2.0,
        tableY_ + rail() + 20,
        11, Gosu::Color::WHITE);

    std::string info = "N: Neues Match   |   R: Neu aufbauen   |   ESC: Beenden";
    double iw = font_->text_width(info);
    drawTextShadow(*font_, info,
        (width() - iw) / 2.0,
        tableY_ + rail() + 20 + fontTitle_->height() + 8,
        11, Gosu::Color(255, 220, 0, 255));
}

void GameWindow::draw() {
    Gosu::Graphics::draw_rect(0, 0, width(), height(),
        Gosu::Color::BLACK, 0);

    // Tisch zeichnen
    if (felt_) {
        double sx = tableW_ / felt_->width();
        double sy = tableH_ / felt_->height();
        felt_->draw(tableX_, tableY_, 1, sx, sy);
    }
    else {
        Gosu::Graphics::draw_rect(tableX_, tableY_, tableW_, tableH_,
            Gosu::Color(30, 120, 40, 255), 1);
    }

    // Kugeln zeichnen
    auto drawBall = [&](const Ball& b) {
        if (!b.inPlay) return;
        drawCircle(b.x + kBallShadow_dx, b.y + kBallShadow_dy,
            b.r * kBallShadowMul,
            Gosu::Color(90, 0, 0, 0), 3, kBallSegments);
        drawCircle(b.x, b.y, b.r + 1.0,
            Gosu::Color(200, 0, 0, 0), 4, kBallSegments);
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

    // Queue
    drawAim();

    // Je nach Zustand Overlays
    switch (state_) {
    case UiState::Start:    drawStart(); break;
    case UiState::Paused:   drawHud(); drawPause(); break;
    case UiState::GameOver: drawHud(); drawGameOver(); break;
    case UiState::Playing:  drawHud(); break;
    }
}

// ======================================================
//  Aktionen / Input
// ======================================================
void GameWindow::shootFromAim() {
    if (state_ != UiState::Playing) return;
    if (!game_.allStopped() || game_.isOver()) return;

    Ball& c = game_.cue();
    double dx = aimX_ - c.x;
    double dy = aimY_ - c.y;
    double d = std::sqrt(dx * dx + dy * dy);
    if (d < 1.0) return;
    double nx = dx / d;
    double ny = dy / d;

    double force = 6.0 + 20.0 * power_;
    c.vx = -nx * force;
    c.vy = -ny * force;
    game_.beginShot();

    // Queue nach vorne animieren
    cueAnimFrames_ = kCueFireFrames;
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

// Name-Eingabe mit Groß/Klein, Zahlen, Backspace-Repeat
void GameWindow::handleNameKey(Gosu::Button b) {
    std::string& s = (activeName_ == 0) ? p1Name_ : p2Name_;

    bool shift =
        input().down(Gosu::KB_LEFT_SHIFT) ||
        input().down(Gosu::KB_RIGHT_SHIFT);

    // BACKSPACE (mit Auto-Repeat)
    if (b == Gosu::KB_BACKSPACE) {
        backspaceHeld_ = true;
        backspaceTimer_ = 0.0;
        if (!s.empty()) s.pop_back();
        return;
    }

    // ENTER = Spiel starten
    if (b == Gosu::KB_RETURN) {
        startMatch();
        return;
    }

    // SPACE
    if (b == Gosu::KB_SPACE) {
        s.push_back(' ');
        return;
    }

    // Buchstaben A–Z
    if (b >= Gosu::KB_A && b <= Gosu::KB_Z) {
        char base = shift ? 'A' : 'a';
        char ch = base + (int(b) - int(Gosu::KB_A));
        s.push_back(ch);
        return;
    }

    // Zahlen 0–9 + Shift-Sonderzeichen
    if (b >= Gosu::KB_0 && b <= Gosu::KB_9) {
        int num = (int)b - (int)Gosu::KB_0;
        if (!shift) {
            s.push_back('0' + num);
            return;
        }
        const char* shifts = ")!\"§$%&/(";
        if (num >= 0 && num < 10) {
            s.push_back(shifts[num]);
            return;
        }
    }

    // Minus / Unterstrich
    if (b == Gosu::KB_MINUS) {
        s.push_back(shift ? '_' : '-');
        return;
    }
}

void GameWindow::button_down(Gosu::Button b) {
    Gosu::Window::button_down(b);

    // --- Startscreen: Klick in Name-Box / Avatar-Pfeile ---
    if (state_ == UiState::Start && b == Gosu::MS_LEFT) {
        layoutStartBoxes();
        double mx = input().mouse_x();
        double my = input().mouse_y();

        // Namensfelder aktivieren
        if (p1Box_.contains(mx, my)) {
            activeName_ = 0;
            return;
        }
        if (p2Box_.contains(mx, my)) {
            activeName_ = 1;
            return;
        }

        // Avatar-Pfeile
        for (int i = 0; i < 2; ++i) {
            if (avatarLeft_[i].contains(mx, my)) {
                if (!avatarImgs_.empty()) {
                    int n = (int)avatarImgs_.size();
                    avatarIndex_[i] = (avatarIndex_[i] - 1 + n) % n;
                }
                return;
            }
            if (avatarRight_[i].contains(mx, my)) {
                if (!avatarImgs_.empty()) {
                    int n = (int)avatarImgs_.size();
                    avatarIndex_[i] = (avatarIndex_[i] + 1) % n;
                }
                return;
            }
        }
        return;
    }

    // Startscreen: Tastatur für Namen
    if (state_ == UiState::Start) {
        if (b == Gosu::KB_TAB) {  // zwischen Spieler 1/2 wechseln
            activeName_ ^= 1;
            return;
        }
        handleNameKey(b);
        return;
    }

    // --- Klickbare Optionen im PAUSE-Overlay ---
    if (state_ == UiState::Paused && b == Gosu::MS_LEFT) {
        double mx = input().mouse_x();
        double my = input().mouse_y();

        std::string tBack = "P: Zurueck";
        std::string tSep = "   |   ";
        std::string tNew = "N: Neues Spiel";
        std::string tEsc = "ESC: Beenden";

        std::string full = tBack + tSep + tNew + tSep + tEsc;

        double baseY = tableY_ + rail() + 20;
        double yText = baseY + fontTitle_->height() + 8;
        double hText = font_->height();
        double totalW = font_->text_width(full);
        double x = (width() - totalW) / 2.0;

        double wBack = font_->text_width(tBack);
        if (mx >= x && mx <= x + wBack && my >= yText && my <= yText + hText) {
            // wie Taste P
            togglePause();
            return;
        }
        x += wBack + font_->text_width(tSep);

        double wNew = font_->text_width(tNew);
        if (mx >= x && mx <= x + wNew && my >= yText && my <= yText + hText) {
            // wie Taste N
            game_.reset(false);
            state_ = UiState::Playing;
            return;
        }
        x += wNew + font_->text_width(tSep);

        double wEsc = font_->text_width(tEsc);
        if (mx >= x && mx <= x + wEsc && my >= yText && my <= yText + hText) {
            // wie ESC
            close();
            return;
        }
       
    }

    // --- Klickbare Optionen im GAMEOVER-Overlay ---
    if (state_ == UiState::GameOver && b == Gosu::MS_LEFT) {
        double mx = input().mouse_x();
        double my = input().mouse_y();

        std::string tNew = "N: Neues Match";
        std::string tSep = "   |   ";
        std::string tRe = "R: Neu aufbauen";
        std::string tEsc = "ESC: Beenden";

        std::string full = tNew + tSep + tRe + tSep + tEsc;

        double baseY = tableY_ + rail() + 20;
        double yText = baseY + fontTitle_->height() + 8;
        double hText = font_->height();
        double totalW = font_->text_width(full);
        double x = (width() - totalW) / 2.0;

        double wNew = font_->text_width(tNew);
        if (mx >= x && mx <= x + wNew && my >= yText && my <= yText + hText) {
            // wie Taste N -> neues Match
            game_.reset(false);
            state_ = UiState::Playing;
            return;
        }
        x += wNew + font_->text_width(tSep);

        double wRe = font_->text_width(tRe);
        if (mx >= x && mx <= x + wRe && my >= yText && my <= yText + hText) {
            // wie Taste R -> neu aufbauen
            game_.reset(true);
            state_ = UiState::Playing;
            return;
        }
        x += wRe + font_->text_width(tSep);

        double wEsc = font_->text_width(tEsc);
        if (mx >= x && mx <= x + wEsc && my >= yText && my <= yText + hText) {
            // wie ESC
            close();
            return;
        }
        
    }

    // --- überall: Pause- und Mute-Buttons per Klick ---
    if (b == Gosu::MS_LEFT) {
        double mx = input().mouse_x();
        double my = input().mouse_y();

        if (pauseBtn_.contains(mx, my)) {
            togglePause();
            return;
        }
        if (muteBtn_.contains(mx, my)) {
            muted_ = !muted_;
            return;
        }
    }

    // --- Leertaste = Vorspulen im Spiel ---
    if (b == Gosu::KB_SPACE && state_ == UiState::Playing) {
        if (!game_.allStopped()) {
            game_.fastForwardToRest();
            playSoundEvents();
        }
        return;
    }

    // --- Queue ziehen per linker Maustaste im Playing ---
    if (b == Gosu::MS_LEFT &&
        state_ == UiState::Playing &&
        game_.allStopped() &&
        !game_.isOver()) {

        dragging_ = true;

        const Ball& c = game_.cue();
        double dx = input().mouse_x() - c.x;
        double dy = input().mouse_y() - c.y;
        double d = std::max(1.0, std::sqrt(dx * dx + dy * dy));
        double nx = dx / d;
        double ny = dy / d;
        cuePullStart_ = dx * nx + dy * ny;

        cueBackDist_ = kCueMinBackPx;
    }

    // Tastatur-Steuerung für Pause/Reset/Neues Spiel
    if (b == Gosu::KB_P && (state_ == UiState::Playing || state_ == UiState::Paused))
        togglePause();
    if (b == Gosu::KB_R && state_ != UiState::Start)
        game_.reset(true);
    if (b == Gosu::KB_N && state_ != UiState::Start) {
        game_.reset(false);
        state_ = UiState::Playing;
    }
    if (b == Gosu::KB_ESCAPE)
        close();
}

void GameWindow::button_up(Gosu::Button b) {
    // Backspace losgelassen -> Auto-Repeat stoppen
    if (b == Gosu::KB_BACKSPACE) {
        backspaceHeld_ = false;
        backspaceTimer_ = 0.0;
    }

    // Maustaste losgelassen -> ggf. Stoß ausführen
    if (b == Gosu::MS_LEFT && dragging_) {
        shootFromAim();
    }
    dragging_ = false;
}
