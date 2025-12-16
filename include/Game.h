#pragma once

#include "Map.h"
#include "Entity.h"
#include <vector>

// Главное состояние игры.
// Внутри храним карту, игрока, врагов и флаг, идет ли игра.
struct GameState {
    Map map;
    Entity player;
    std::vector<Entity> enemies; // Враги (крысы, медведи, змеи)
    bool isRunning;
    int torchRadius; // Радиус факела для FOV
    int level;       // Текущий уровень (начинается с 1)

    // Состояние отравления игрока.
    // Мы специально храним его в GameState, чтобы не усложнять класс Entity.
    bool isPlayerPoisoned = false;   // Отравлен ли сейчас игрок
    int poisonTurnsRemaining = 0;    // Сколько ходов еще длится яд

    // Состояние "проклятия призрака" — когда игрок не видит своё здоровье.
    bool isPlayerGhostCursed = false;   // Действует ли сейчас эффект призрака
    int ghostCurseTurnsRemaining = 0;   // Сколько ходов ещё скрыт HP

    // Состояние эффекта краба: инвертированное управление.
    bool isPlayerControlsInverted = false; // Инвертировано ли сейчас управление (эффект краба)
    int crabInversionTurnsRemaining = 0;   // Сколько ходов ещё действует инверсия

    GameState(); // Конструктор задает стартовые значения.
    void updateEnemies(); // Обновление позиций врагов
    void processCombat(); // Обработка боя
    void processItems(); // Обработка предметов
    void generateNewLevel(); // Генерация нового уровня
    bool checkExit(); // Проверка перехода на следующий уровень
    void restartGame(); // Перезапуск игры после смерти игрока

    // Обновление эффекта отравления: наносим периодический урон и уменьшаем таймер.
    void updatePoison();
    // Применяем яд к игроку: задаем новое время действия, не суммируя эффект.
    void applyPoisonToPlayer(int minTurns, int maxTurns);

    // Обновление эффекта призрака: просто тикает таймер, урона не наносит.
    void updateGhostCurse();
    // Вешаем на игрока эффект призрака (скрытие HP) на случайное число ходов.
    void applyGhostCurseToPlayer(int minTurns, int maxTurns);

    // Обновление эффекта краба (инвертированное управление).
    void updateCrabInversion();
    // Вешаем на игрока эффект краба: инвертируем управление на случайное число ходов.
    void applyCrabInversionToPlayer(int minTurns, int maxTurns);
};

// Обработка ввода и простейшая логика перемещения игрока.
// key - обычный int (символ или код клавиши)
void handleInput(GameState& state, int key);
