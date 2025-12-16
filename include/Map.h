#pragma once

#include <libtcod.hpp>
#include <vector>
#include "Entity.h"

// Структура для предмета на карте.
struct Item {
    Position pos;
    int healAmount;      // Сколько здоровья восстанавливает
    int maxHealthBoost;  // Насколько увеличивает максимум здоровья
    char symbol;         // Символ, который рисуем на карте

    Item(int x, int y, int heal, int boost, char sym)
        : pos(x, y), healAmount(heal), maxHealthBoost(boost), symbol(sym) {}
};

class Map {
public:
    // Сделаем размеры карты доступны снаружи,
    // чтобы их могли использовать другие части кода.
    static const int WIDTH = 50;
    static const int HEIGHT = 20;

private:
    char cells[HEIGHT][WIDTH];
    bool explored[HEIGHT][WIDTH]; // Какие клетки уже были видны
    bool visible[HEIGHT][WIDTH];  // Какие клетки видны сейчас (для FOV)
    TCODMap fovMap; // Карта для расчета поля зрения

public:
    Map();
    ~Map();

    void generate();
    char getCell(int x, int y) const;
    void setCell(int x, int y, char symbol);
    bool isWall(int x, int y) const;
    bool isWalkable(int x, int y) const;

    // FOV функции с использованием TCODMap
    void computeFOV(int playerX, int playerY, int radius, bool lightWalls = true);
    bool isVisible(int x, int y) const;
    bool isExplored(int x, int y) const;

    // Предметы на карте
    std::vector<Item> items;
    void addItem(int x, int y, int healAmount, int maxHealthBoost, char symbol);
    void addHealItem(int x, int y, int healAmount);
    void addMaxHealthItem(int x, int y, int maxHealthBoost);
    void addGhostItem(int x, int y); // Прозрачный предмет '.'
    void addShieldItem(int x, int y); // Щит "O"
    Item* getItemAt(int x, int y);
    void removeItem(int index);
    
    // Выход на следующий уровень
    Position exitPos;
    void addExit(int x, int y);
    bool isExit(int x, int y) const;
};
