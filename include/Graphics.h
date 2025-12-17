#pragma once

#ifndef TCOD_NO_CONSOLE
#define TCOD_NO_CONSOLE 1 // Отключаем старый C API консоли, чтобы избежать кучи "не найдено определение"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26439) // функции в libtcod headers помечены как noexcept по анализатору
#endif
#include <libtcod.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <memory>

// Вперед объявляем классы, чтобы не тянуть сюда все заголовки.
class Map;
class Entity;
struct Item;

// Класс для работы с выводом через libtcod.
// Использует TCOD_Context для окна и TCOD_Console для отрисовки.
class Graphics {
private:
    std::shared_ptr<TCOD_Context> context;
    tcod::Console console;
    int screenWidth;
    int screenHeight;
    // Размеры UI‑слоёв, которые приходят из main.cpp.
    int leftPanelWidth;
    int rightPanelWidth;
    int topPanelHeight;
    int bottomPanelHeight;
    
    // Цвета для разных объектов
    tcod::ColorRGB colorPlayer;
    tcod::ColorRGB colorWall;      // Стены: #000064
    tcod::ColorRGB colorFloor;     // Пол: #323296
    tcod::ColorRGB colorEnemy;
    tcod::ColorRGB colorItem;
    tcod::ColorRGB colorDark;      // Для невидимых клеток
    tcod::ColorRGB colorExplored;  // Для исследованных, но невидимых клеток
    
    // Для эффекта факела
    TCODNoise torchNoise;
    float torchX;

public:
    // width/height — полный размер экрана.
    // Остальные параметры задают толщину UI‑панелей (те же значения,
    // которые используются в main.cpp при расчёте screenWidth/screenHeight).
    Graphics(int width,
             int height,
             int leftPanelWidth,
             int rightPanelWidth,
             int topPanelHeight,
             int bottomPanelHeight);
    ~Graphics();

    void drawMap(const Map& map, int playerX, int playerY, int torchRadius, bool showExitHint, const std::vector<std::pair<int, int>>& fireflyPositions = {});
    void drawEntity(const Entity& entity);
    // Специальный метод для игрока с динамическим цветом.
    // isPoisoned — отравление, hasShield — активный щит (игрок подсвечивается белым).
    void drawPlayer(const Entity& player, bool isPoisoned, bool hasShield);
    void drawItem(const Item& item);
    void drawUI(const Entity& player,
                const std::vector<Entity>& enemies,
                int level,
                const Map& map,
                bool isPlayerPoisoned,
                bool isPlayerGhostCursed,
                int shieldTurns,
                int shieldWhiteSegments,
                bool questActive,
                int questKills,
                int questTarget,
                // Расширенная информация о квесте для цветной подсветки
                const std::vector<std::pair<int, int>>& questTargets,
                const std::vector<int>& questProgress,
                int questType,
                bool perkQuestHighlightEnabled,
                // Флаги, видел ли игрок этих мобов/предметы (для Legend: ? - ???)
                bool seenRat,
                bool seenBear,
                bool seenSnake,
                bool seenGhost,
                bool seenCrab,
                bool seenMedkit,
                bool seenMaxHP,
                bool seenShield,
                bool seenTrap,
                bool seenQuest);
    void refreshScreen();
    void clearScreen();
    // Читает одну клавишу. Возвращает true если что-то нажали.
    // В key кладем либо символ ('w','a','s','d','q'), либо код стрелки (TCODK_UP и т.п.)
    bool getInput(int& key);
    // Получает позицию мыши на карте. Возвращает true если мышь над игровой областью.
    // mapX, mapY - координаты на карте (0..WIDTH-1, 0..HEIGHT-1)
    bool getMousePosition(int& mapX, int& mapY);
    // Рисует название справа от символа при наведении мыши
    void drawHoverName(int mapX, int mapY, const std::string& name, const tcod::ColorRGB& color);
    // Рисует экран выбора перка при переходе на следующий уровень
    void drawLevelChoiceMenu(int variant1, int variant2, int variant3);
    // Рисует экран смерти с статистикой
    void drawDeathScreen(int level,
                         int killsRat, int killsBear, int killsSnake, int killsGhost, int killsCrab,
                         int itemsMedkit, int itemsMaxHP, int itemsShield, int itemsTrap, int itemsQuest,
                         const std::vector<std::string>& collectedPerks);
    // Переключение полноэкранного режима
    void toggleFullscreen();
};
