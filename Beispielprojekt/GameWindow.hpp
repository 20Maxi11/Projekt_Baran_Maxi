#ifndef GAMEWINDOW_HPP
#define GAMEWINDOW_HPP

#include <Gosu/Gosu.hpp>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include "Game.hpp"

// kleines Rechteck für Mausklick-Zonen (Buttons, Namefelder, Pfeile)
struct RectF {
    double x{}, y{}, w{}, h{};
    bool contains(double px, double py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class GameWindow : public Gosu::Window
{
public:
    // FPS: aktuell 60.0
    // Wenn du testen willst: einfach 120.0 eintragen, aber 60 ist meist flüssiger.
    GameWindow(unsigned width, unsigned height, int players = 2, bool fullscreen = true);

    void update() override;      // pro Frame Spiellogik
    void draw() override;        // pro Frame alles zeichnen
    void button_down(Gosu::Button) override; // Maustaste/Taste gedrückt
    void button_up(Gosu::Button) override;   // Maustaste/Taste losgelassen

private:
    // UI-Zustand des Spiels
    enum class UiState { Start, Playing, Paused, GameOver };
    UiState state_ = UiState::Start;

    // Kernspiel (Physik, Regeln, Soundevents)
    Game game_;

    // Queue / Zielen
    bool   dragging_ = false;    // ziehe ich gerade mit der Maus?
    double aimX_ = 0.0, aimY_ = 0.0;   // aktuelle Mausposition
    double power_ = 0.0;               // Stoßstärke 0..1
    int    cueAnimFrames_ = 0;         // Anzahl Animations-Frames nach Stoß
    double cueBackDist_ = 0.0;         // sichtbarer Rückzug des Queue
    double cuePullStart_ = 0.0;        // Start-Projektion beim Ziehen

    // Spielernamen & Auswahl
    std::string p1Name_ = "Spieler 1";
    std::string p2Name_ = "Spieler 2";
    int activeName_ = 0;               // 0 = Spieler 1, 1 = Spieler 2
    RectF p1Box_, p2Box_;              // Eingabefelder für die Namen

    // Avatar-Auswahl (pro Spieler ein Index)
    std::vector<std::string> avatarFiles_;  // Dateinamen der Avatare
    std::vector<std::unique_ptr<Gosu::Image>> avatarImgs_; // geladene Bilder
    int avatarIndex_[2] = { 0, 0 };         // Auswahl P1/P2
    RectF avatarLeft_[2];                   // Klick-Zone "<" je Spieler
    RectF avatarRight_[2];                  // Klick-Zone ">" je Spieler

    // Name-Eingabe-Helfer
    bool   typing_ = false;       // Nameneingabe aktiv (nur im Startscreen)
    double cursorTimer_ = 0.0;    // für blinkenden Cursor
    bool   cursorVisible_ = true; // ob Cursor gerade sichtbar ist
    double backspaceTimer_ = 0.0; // Repeat-Timer für Backspace
    bool   backspaceHeld_ = false;// Backspace aktuell gehalten?

    // Fonts für HUD & Titel
    std::unique_ptr<Gosu::Font> font_;
    std::unique_ptr<Gosu::Font> fontTitle_;
    double lastW_ = 0.0, lastH_ = 0.0; // zum Erkennen von Größenänderungen

    // Tisch-Geometrie (zentriert, 2:1)
    double tableX_ = 0.0, tableY_ = 0.0;
    double tableW_ = 0.0, tableH_ = 0.0;

    // Grafik-Assets
    std::unique_ptr<Gosu::Image> felt_;    // Tisch-Bild
    std::unique_ptr<Gosu::Image> cueImg_;  // Queue-Bild
    std::array<std::unique_ptr<Gosu::Image>, 16> ballImg_; // Kugeln 0..15

    // Sound-Assets
    std::unique_ptr<Gosu::Sample> sfxBall_;   // Ball-Ball / Rail
    std::unique_ptr<Gosu::Sample> sfxPocket_; // Ball in Tasche
    std::unique_ptr<Gosu::Sample> sfxRail_;   // Bande
    std::unique_ptr<Gosu::Sample> sfxWin_;    // Jubeln bei Sieg
    double sfxVolume_ = 1.0;                  // Lautstärke 0..1
    bool   muted_ = false;                    // ob Sound stumm geschaltet ist

    // Normiertes Tuch-Rechteck aus PNG (zwischen 0..1)
    struct NormRect { double l = 0.08, t = 0.08, r = 0.92, b = 0.92; };
    NormRect feltNorm_;
    bool feltOk_ = false;

    // Buttons (Pause & Mute) oben rechts
    RectF pauseBtn_;   // klickbarer Bereich Pause
    RectF muteBtn_;    // klickbarer Bereich Ton an/aus

    // --- Initialisieren / Layout / PNG-Analyse ---
    void loadAssets();                // Bilder laden (Tisch, Queue, Kugeln)
    void loadSounds();                // Sounds laden
    static std::string ballFile(int id); // Dateiname pro Kugel-ID

    double rail() const;              // ungef. Bandendicke in Pixel
    void ensureFonts();               // Fonts passend zur Fenstergröße
    void computeTableRect();          // Tisch-Rechteck berechnen
    void layoutStartBoxes();          // Felder + Avatar-Pfeile im Startscreen
    void layoutHudButtons();          // Pause-/Mute-Button-Positionen setzen

    void analyzeTableImage();         // grünes Tuch aus PNG erkennen
    void rebuildGameGeomFromNorm();   // PNG-Koords -> Spiel-Koords setzen

    // --- Name-Eingabe ---
    void handleNameKey(Gosu::Button b); // Tastatureingaben im Startscreen

    // --- Zeichnen ---
    void drawCircle(double cx, double cy, double r,
        Gosu::Color col, double z = 1.0, int seg = 28);
    static void drawTextShadow(Gosu::Font& f, const std::string& s,
        double x, double y, double z, Gosu::Color col);

    void drawAim();       // Queue zeichnen
    void drawHud();       // HUD (Namen, Avatare, Buttons)
    void drawStart();     // Startbildschirm (Namen + Avatarwahl)
    void drawPause();     // Pause-Overlay
    void drawGameOver();  // Game-Over-Overlay

    // kleinen Avatar-Kreis zeichnen
    void drawAvatarCircle(double cx, double cy, double radius,
        Gosu::Image* img, double z);

    // --- Aktionen ---
    void shootFromAim();  // Cue-Stoß mit aktueller Power
    void togglePause();   // Pause an/aus
    void startMatch();    // neues Match starten (mit aktuellen Namen)

    // --- Sound-Ereignisse aus Game abspielen ---
    void playSoundEvents();
};

#endif
