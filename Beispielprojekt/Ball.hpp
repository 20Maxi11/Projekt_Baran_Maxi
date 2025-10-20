#ifndef BALL_HPP
#define BALL_HPP

#include <cstddef>

enum class BallType { CUE, SOLID, STRIPE, EIGHT };

struct Ball {
    int id;            // 0=weiﬂ, 1..7 volle, 8=schwarz, 9..15 halbe
    BallType type;
    double x, y;       // Position (Pixel)
    double vx = 0.0, vy = 0.0;   // Geschwindigkeit
    double r = 10.0;            // Radius (Pixel)
    bool   inPlay = true;        // im Spiel?

    Ball(int id_, BallType t, double X, double Y, double radius = 10.0)
        : id(id_), type(t), x(X), y(Y), r(radius) {
    }
};

#endif
