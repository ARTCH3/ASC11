#pragma once

#ifndef TCOD_NO_CONSOLE
#define TCOD_NO_CONSOLE 1
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26439) // подавляем анализатор для внешнего libtcod
#endif
#include <libtcod.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
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
    // Размеры карты 16:9, чтобы мир и интерфейс занимали весь экран без черных полос.
    // 80x36 -> вместе с 9 строками HUD получаем консоль 80x45, тоже 16:9.
    static const int WIDTH = 80;
    static const int HEIGHT = 36;

private:
    char cells[HEIGHT][WIDTH];
    bool explored[HEIGHT][WIDTH]; // Какие клетки уже были видны
    bool visible[HEIGHT][WIDTH];  // Какие клетки видны сейчас (для FOV)
    TCODMap fovMap; // Карта для расчета поля зрения

public:
    Map();
    ~Map();

    void generate(int currentLevel = 1); // Уровень для контроля спавна предметов на первом уровне
    char getCell(int x, int y) const;
    void setCell(int x, int y, char symbol);
    bool isWall(int x, int y) const;
    bool isWalkable(int x, int y) const;
    bool inBounds(int x, int y) const; // Проверка границ карты

    // FOV функции с использованием TCODMap
    void computeFOV(int playerX, int playerY, int radius, bool lightWalls = true);
    // Добавляет FOV от дополнительного источника света (не перезаписывает существующий FOV)
    void addFOV(int sourceX, int sourceY, int radius, bool lightWalls = true);
    void revealAll(); // Сделать всю карту видимой и исследованной
    bool isVisible(int x, int y) const;
    bool isExplored(int x, int y) const;
    // Отмечаем клетки внутри круга как "исследованные" (но не обязательно видимые).
    void revealCircle(int cx, int cy, int radius);

    // Предметы на карте
    std::vector<Item> items;
    void addItem(int x, int y, int healAmount, int maxHealthBoost, char symbol);
    void addHealItem(int x, int y, int healAmount);
    void addMaxHealthItem(int x, int y, int maxHealthBoost);
    void addTrapItem(int x, int y);  // Ловушка (мина) '.'
    void addShieldItem(int x, int y); // Щит "O"
    void addQuestItem(int x, int y);  // Квестовый предмет '?'
    Item* getItemAt(int x, int y);
    void removeItem(int index);
    
    // Выход на следующий уровень
    Position exitPos;
    void addExit(int x, int y);
    bool isExit(int x, int y) const;
};
