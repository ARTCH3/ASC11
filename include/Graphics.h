#pragma once

#include <libtcod.hpp>
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
    Graphics(int width, int height);
    ~Graphics();

    void drawMap(const Map& map, int playerX, int playerY, int torchRadius);
    void drawEntity(const Entity& entity);
    void drawPlayer(const Entity& player); // Специальный метод для игрока с динамическим цветом
    void drawItem(const Item& item);
    void drawUI(const Entity& player,
                const std::vector<Entity>& enemies,
                int level,
                const Map& map,
                int shieldTurns,
                bool questActive,
                int questKills,
                int questTarget);
    void refreshScreen();
    void clearScreen();
    // Читает одну клавишу. Возвращает true если что-то нажали.
    // В key кладем либо символ ('w','a','s','d','q'), либо код стрелки (TCODK_UP и т.п.)
    bool getInput(int& key);
    // Переключение полноэкранного режима
    void toggleFullscreen();
};
