#pragma once

#include "Map.h"
#include "Entity.h"
#include <vector>

// Главное состояние игры.
// Внутри храним карту, игрока, врагов и флаг, идет ли игра.
struct GameState {
    Map map;
    Entity player;
    std::vector<Entity> enemies; // Враги (крысы)
    bool isRunning;
    int torchRadius; // Радиус факела для FOV
    int level;       // Текущий уровень (начинается с 1)
    int shieldTurns; // Количество ходов с эффектом щита

    GameState(); // Конструктор задает стартовые значения.
    void updateEnemies(); // Обновление позиций врагов
    void processCombat(); // Обработка боя
    void processItems(); // Обработка предметов
    void generateNewLevel(); // Генерация нового уровня
    bool checkExit(); // Проверка перехода на следующий уровень
    void restartGame(); // Перезапуск игры после смерти игрока
};

// Обработка ввода и простейшая логика перемещения игрока.
// key - обычный int (символ или код клавиши)
void handleInput(GameState& state, int key);
