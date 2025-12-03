#pragma once

#include <libtcod.hpp>

// Символы, которые мы используем в игре.
enum GameSymbols {
    SYM_PLAYER = '@',
    SYM_WALL   = '#',      // Используется только для внутренней логики карты (не для отрисовки)
    SYM_FLOOR  = '.',      // Используется только для внутренней логики карты (не для отрисовки)
    SYM_ENEMY  = 'r',      // Крыса
    SYM_ITEM   = 0x2665,   // Предмет для восстановления здоровья (♥)
    SYM_EXIT   = '^'       // Выход на следующий уровень
};

// Самая простая структура позиции.
struct Position {
    int x;
    int y;
    
    Position() : x(0), y(0) {}
    Position(int x, int y) : x(x), y(y) {}
};

// Базовая сущность (игрок, враг и т.п.).
// Теперь с здоровьем и уроном.
class Entity {
public:
    Position pos;
    char symbol;
    TCOD_ColorRGB color;
    int health;
    int maxHealth;
    int damage; // Урон, который наносит эта сущность
    
    Entity(int startX, int startY, char sym, const TCOD_ColorRGB& col);
    void move(int dx, int dy);
    void takeDamage(int amount);
    bool isAlive() const;
};
