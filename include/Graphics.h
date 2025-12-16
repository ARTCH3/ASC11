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
    // Специальный метод для игрока с динамическим цветом.
    // Параметр isPoisoned позволяет временно перекрасить игрока в ядовито-зелёный цвет.
    void drawPlayer(const Entity& player, bool isPoisoned);
    void drawItem(const Item& item);
    void drawUI(const Entity& player,
                const std::vector<Entity>& enemies,
                int level,
                const Map& map,
                bool isPlayerPoisoned,
                bool isPlayerGhostCursed);
    void refreshScreen();
    void clearScreen();
    // Читает одну клавишу. Возвращает true если что-то нажали.
    // В key кладем либо символ ('w','a','s','d','q'), либо код стрелки (TCODK_UP и т.п.)
    bool getInput(int& key);
    // Переключение полноэкранного режима
    void toggleFullscreen();
};
