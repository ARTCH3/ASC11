#include "Entity.h"

// Конструктор просто запоминает стартовые значения.
Entity::Entity(int startX, int startY, int sym, const TCOD_ColorRGB& col)
    : pos(startX, startY),
      symbol(sym),
      color(col),
      health(20),
      maxHealth(20),
      damage(1),
      crabAttachedToPlayer(false),
      crabAttachmentCooldown(0)
{
}

// Простейшее перемещение без какой-либо проверки.
void Entity::move(int dx, int dy)
{
    pos.x += dx;
    pos.y += dy;
}

// Получение урона.
void Entity::takeDamage(int amount)
{
    health -= amount;
    if (health < 0) {
        health = 0;
    }
}

// Проверка, жива ли сущность.
bool Entity::isAlive() const
{
    return health > 0;
}
