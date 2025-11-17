#ifndef BALL_HPP
#define BALL_HPP

#include <cstddef>
// BallType: Art der Kugel 
enum class BallType { CUE, SOLID, STRIPE, EIGHT };
// Ball: beschreibt eine einzelne Billardkugel im Spielfeld
struct Ball {
    int id;            // 0=weiﬂ, 1..7 volle, 8=schwarz, 9..15 halbe
    BallType type;
    double x, y;       // Position (Pixel)
    double vx = 0.00, vy = 0.00;   // Geschwindigkeit
    double r = 10.0;            // Radius (Pixel)
    bool   inPlay = true;        // im Spiel?
    // Position, Geschwindigkeit, Radius und ob die Kugel noch im Spiel ist.
    Ball(int id_, BallType t, double X, double Y, double radius = 10.0)
        : id(id_), type(t), x(X), y(Y), r(radius) {
    }
};

#endif
