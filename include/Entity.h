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

// Символы, используемые в игре (CP437-compatible: отображаются с classic tileset).
enum GameSymbols {
    SYM_PLAYER = '@',    // Игрок
    SYM_WALL   = '#',    // Стена (только для логики карты и выхода)
    SYM_FLOOR  = '.',    // Пол (только для логики карты, не отрисовывается)
    SYM_ENEMY  = 'r',    // Крыса
    SYM_BEAR   = 'B',    // Медведь
    SYM_SNAKE  = 'S',    // Змея
    SYM_GHOST  = 'g',    // Призрак
    SYM_CRAB   = 'C',    // Краб
    SYM_ITEM   = '$',    // Medkit отображается символом $ (CP437 код 36)
    SYM_EXIT   = '#',    // Выход на следующий уровень (совпадает с SYM_WALL)
    SYM_MAX_HP = '+',     // Новый предмет для увеличения максимального здоровья
    SYM_GHOST_ITEM = '.', // Прозрачный (призрачный) предмет
    SYM_SHIELD = 'O',     // Щит — защита от откидывания
    SYM_QUEST = '?'       // Квестовый предмет
};

// Простая структура позиции на карте.
struct Position {
    int x, y;
    Position() : x(0), y(0) {}
    Position(int x, int y) : x(x), y(y) {}
};

// Базовая сущность (игрок, враг и др.).
class Entity {
public:
    Position pos;
    int symbol; // Код символа (ASCII, CP437 или Unicode при совместимом тайлсете)
    TCOD_ColorRGB color;
    int health;
    int maxHealth;
    int damage;

    // Дополнительные поля, которые используются только некоторыми типами мобов.
    // Для краба:
    //  - crabAttachedToPlayer == true, если краб "прицепился" к игроку и инвертирует управление.
    //  - crabAttachmentCooldown > 0, если краб недавно отцепился и пока не может снова цепляться.
    bool crabAttachedToPlayer;
    int  crabAttachmentCooldown;

    Entity(int startX, int startY, int sym, const TCOD_ColorRGB& col);
    void move(int dx, int dy);
    void takeDamage(int amount);
    bool isAlive() const;
};
