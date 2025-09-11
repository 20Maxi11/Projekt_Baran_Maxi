#ifndef BALL_HPP
#define BALL_HPP

#include <Gosu/Gosu.hpp>

enum class BallType { CUE, SOLID, STRIPE, EIGHT };

struct Ball {
    int id;                  // 0 = Weiﬂ, 1ñ7 = Volle, 8 = Acht, 9ñ15 = Halbe
    BallType type;
    double x, y;
    double vx = 0.0, vy = 0.0;
    double r = 10.0;
    Gosu::Color color;
    bool inPlay = true;

    Ball(int id_, BallType t, double X, double Y, Gosu::Color c, double radius = 10.0)
        : id(id_), type(t), x(X), y(Y), r(radius), color(c) {
    }
};

#endif
