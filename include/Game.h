#pragma once

#include "Map.h"
#include "Entity.h"
#include <vector>

// Все поля GameState объявлены ниже, включая shieldTurns, visionTurns, questActive и т.д.

// Главное состояние игры.
// Внутри храним карту, игрока, врагов и флаг, идет ли игра.
struct GameState {
    Map map;
    Entity player;
    std::vector<Entity> enemies; // Враги (крысы, медведи, змеи)
    bool isRunning;
    int torchRadius; // Радиус факела для FOV
    int level;       // Текущий уровень (начинается с 1)
    int shieldTurns; // Количество ходов с эффектом щита
    int shieldWhiteSegments; // сколько "белых" делений щита (урон по щиту)
    int visionTurns; // Количество ходов с полной подсветкой карты
    // Расширенная система квестов
    bool questActive; // Активен ли квест
    enum QuestType {
        QUEST_KILL,  // Убийство мобов
        QUEST_COLLECT // Сбор предметов
    };
    QuestType questType; // Тип квеста
    // Цели квеста: пары (символ моба/предмета, количество)
    // Для убийства: SYM_ENEMY, SYM_BEAR, SYM_SNAKE, SYM_GHOST, SYM_CRAB
    // Для сбора: SYM_ITEM, SYM_MAX_HP, SYM_SHIELD, SYM_TRAP
    std::vector<std::pair<int, int>> questTargets; // (символ, цель)
    std::vector<int> questProgress; // Прогресс по каждой цели (индекс соответствует questTargets)
    
    // Старые поля для совместимости (используются только для простых квестов)
    int questTarget;  // Сколько монстров нужно убить (устарело, используется только для обратной совместимости)
    int questKills;   // Сколько монстров уже убито (устарело)
    
    // Модификатор для подсветки квеста цветом
    bool perkQuestHighlightEnabled = false;

    // --- Прогрессия между этажами (перки) ---
    // Активен ли сейчас экран выбора перка при переходе по лестнице.
    bool isPerkChoiceActive = false;
    
    // Текущие варианты модификаторов для отображения (генерируются один раз при активации экрана)
    int perkChoiceVariant1 = 0; // Вариант для первого столбца (0-5)
    int perkChoiceVariant2 = 0; // Вариант для второго столбца (0-5)
    int perkChoiceVariant3 = 0; // Вариант для третьего столбца (0-5)

    // Постоянные бонусы (действуют на все последующие уровни).
    int perkBonusRats = 0;      // Дополнительные крысы на каждый новый уровень.
    int perkBonusHeals = 0;     // Дополнительные аптечки (heal items) на каждый новый уровень.
    int perkBonusShields = 0;   // Дополнительные предметы-щиты на каждый новый уровень.
    bool perkFireflyEnabled = false; // "Светлячок" (летающий источник света, раскрывает туман войны).

    // Позиции светлячков на карте (могут быть несколько, накапливаются при выборе перка).
    struct Firefly {
        int x, y;
        Firefly(int x_, int y_) : x(x_), y(y_) {}
    };
    std::vector<Firefly> fireflies; // Вектор позиций светлячков

    // Постоянный эффект: на каждом уровне первые несколько шагов показываем лестницу.
    bool perkShowExitFirst3Steps = false;
    int stepsOnCurrentLevel = 0; // Сколько ходов сделано на текущем уровне.

    // Временный эффект: медведи с "ядом" только на следующем уровне.
    bool perkBearPoisonNextLevel = false;       // Помечаем, что следующий этаж будет с ядовитыми медведями.
    bool perkBearPoisonActiveThisLevel = false; // Фактическое состояние на текущем этаже.

    // Временные эффекты "только на следующий этаж".
    int perkSnakesNextLevel = 0;           // Сколько змей добавить только на следующем уровне.
    int perkExtraMaxHpItemsNextLevel = 0;  // Сколько доп. предметов Max HP добавить на следующем уровне.
    int perkTorchRadiusDeltaNextLevel = 0; // Насколько изменить радиус факела на следующем уровне (обычно отрицательное число).

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

    // Флаг, подсвечивать ли лестницу потому что все враги убиты (до посещения лестницы)
    bool showExitBecauseCleared = false;

    // --- Статус \"встреченных\" сущностей/предметов для Legend ---
    bool seenRat = false;
    bool seenBear = false;
    bool seenSnake = false;
    bool seenGhost = false;
    bool seenCrab = false;

    bool seenMedkit = false;
    bool seenMaxHP = false;
    bool seenShield = false;
    bool seenTrap = false;
    bool seenQuest = false;

    // --- Разблокированные мобы и предметы (для прогрессии) ---
    // Мобы разблокируются только если были на первом уровне или выбраны через перки
    bool unlockedRat = false;
    bool unlockedBear = false;
    bool unlockedSnake = false;
    bool unlockedGhost = false;
    bool unlockedCrab = false;

    // Предметы разблокируются только если были на первом уровне или выбраны через перки
    bool unlockedMedkit = false;
    bool unlockedMaxHP = false;
    bool unlockedShield = false;
    bool unlockedTrap = false;
    bool unlockedQuest = false;

    // --- Статистика для экрана смерти ---
    int killsRat = 0;
    int killsBear = 0;
    int killsSnake = 0;
    int killsGhost = 0;
    int killsCrab = 0;
    
    int itemsMedkit = 0;
    int itemsMaxHP = 0;
    int itemsShield = 0;
    int itemsTrap = 0;
    int itemsQuest = 0;
    
    // Список собранных перков (для отображения на экране смерти)
    std::vector<std::string> collectedPerks;

    // Флаг для экрана смерти
    bool isDeathScreenActive = false;

    GameState(); // Конструктор задает стартовые значения.
    void updateEnemies(); // Обновление позиций врагов
    void processCombat(); // Обработка боя
    void processItems(); // Обработка предметов
    void generateQuest(); // Генерация нового квеста (убийство или сбор)
    void generateNewLevel(); // Генерация нового уровня
    bool checkExit(); // Проверка перехода на следующий уровень
    // Применить выбранный перк (1, 2 или 3) и перейти на следующий уровень.
    void applyLevelChoice(int choiceIndex);
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

    // Применяем урон к щиту, возвращаем сколько урона прошло по здоровью.
    int applyShieldHit(int damage);
};

// Обработка ввода и простейшая логика перемещения игрока.
// key - обычный int (символ или код клавиши)
void handleInput(GameState& state, int key);
