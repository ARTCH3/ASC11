#include "Game.h"

#include <cstdlib>
#include <ctime>
#include <algorithm>

GameState::GameState()
    : map(),
      player(5, 5, SYM_PLAYER, TCOD_ColorRGB{100, 200, 255}),
      isRunning(true),
      torchRadius(8), // Радиус факела
      level(1)        // Начинаем с уровня 1
{
    // Инициализируем генератор случайных чисел один раз.
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    generateNewLevel();
}

// Обновление позиций врагов (простой AI: двигаются к игроку)
void GameState::updateEnemies()
{
    for (size_t i = 0; i < enemies.size(); ++i) {
        if (!enemies[i].isAlive()) {
            continue; // Пропускаем мертвых
        }

        int dx = 0;
        int dy = 0;

        // Простой AI: двигаемся к игроку
        if (enemies[i].pos.x < player.pos.x) {
            dx = 1;
        } else if (enemies[i].pos.x > player.pos.x) {
            dx = -1;
        }

        if (enemies[i].pos.y < player.pos.y) {
            dy = 1;
        } else if (enemies[i].pos.y > player.pos.y) {
            dy = -1;
        }

        // Случайно выбираем направление (горизонтальное или вертикальное)
        if (std::rand() % 2 == 0) {
            dy = 0;
        } else {
            dx = 0;
        }

        int newX = enemies[i].pos.x + dx;
        int newY = enemies[i].pos.y + dy;

        // Проверяем, можно ли туда пойти
        if (newX >= 0 && newX < Map::WIDTH &&
            newY >= 0 && newY < Map::HEIGHT &&
            map.isWalkable(newX, newY) &&
            !(newX == player.pos.x && newY == player.pos.y)) {
            // Перемещаем
            enemies[i].move(dx, dy);
        }
    }
}

// Обработка боя
void GameState::processCombat()
{
    // Проверяем столкновение игрока с врагами
    for (size_t i = 0; i < enemies.size(); ++i) {
        if (!enemies[i].isAlive()) {
            continue;
        }

        // Если враг на той же клетке, что и игрок
        if (enemies[i].pos.x == player.pos.x &&
            enemies[i].pos.y == player.pos.y) {
            // Игрок атакует врага
            enemies[i].takeDamage(player.damage);
        }

        // Если враг рядом с игроком (в соседней клетке), он атакует
        int dx = abs(enemies[i].pos.x - player.pos.x);
        int dy = abs(enemies[i].pos.y - player.pos.y);
        if ((dx == 1 && dy == 0) || (dx == 0 && dy == 1)) {
            player.takeDamage(enemies[i].damage);
        }
    }

    // Удаляем мертвых врагов
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
                       [](const Entity& e) { return !e.isAlive(); }),
        enemies.end()
    );

    // Проверяем, не умер ли игрок
    if (!player.isAlive()) {
        // Вместо выхода из игры, перезапускаем её
        restartGame();
    }
}

// Обработка предметов
void GameState::processItems()
{
    // Проверяем, есть ли предмет на позиции игрока
    for (int i = static_cast<int>(map.items.size()) - 1; i >= 0; --i) {
        const Item& item = map.items[i];
        if (item.pos.x == player.pos.x && item.pos.y == player.pos.y) {
            // Сначала увеличиваем максимум здоровья, если предмет дает бонус.
            if (item.maxHealthBoost > 0) {
                player.maxHealth += item.maxHealthBoost;
            }

            // Затем лечим, если предмет лечит.
            if (item.healAmount > 0) {
                player.health += item.healAmount;
                if (player.health > player.maxHealth) {
                    player.health = player.maxHealth;
                }
            }

            // Убираем предмет
            map.removeItem(i);
        }
    }
}

// Логика обработки ввода
// key - либо символ ('w','a','s','d','q'), либо код стрелки (TCODK_UP и т.п.)
void handleInput(GameState& state, int key)
{
    int dx = 0;
    int dy = 0;

    // Обрабатываем разные типы ввода
    // Основные направления (WASD)
    if (key == TCODK_UP || key == 'w' || key == 'W') {
        dy = -1;
    } else if (key == TCODK_DOWN || key == 's' || key == 'S') {
        dy = 1;
    } else if (key == TCODK_LEFT || key == 'a' || key == 'A') {
        dx = -1;
    } else if (key == TCODK_RIGHT || key == 'd' || key == 'D') {
        dx = 1;
    }
    // Диагональные направления
    else if (key == 'q' || key == 'Q') {
        // Q - влево-вверх (северо-запад)
        dx = -1;
        dy = -1;
    } else if (key == 'e' || key == 'E') {
        // E - вправо-вверх (северо-восток)
        dx = 1;
        dy = -1;
    } else if (key == 'z' || key == 'Z') {
        // Z - влево-вниз (юго-запад)
        dx = -1;
        dy = 1;
    } else if (key == 'c' || key == 'C') {
        // C - вправо-вниз (юго-восток)
        dx = 1;
        dy = 1;
    }
    // Выход только по ESC
    else if (key == TCODK_ESCAPE) {
        state.isRunning = false;
        return;
    }

    // Перемещаем игрока
    if (dx != 0 || dy != 0) {
        int newX = state.player.pos.x + dx;
        int newY = state.player.pos.y + dy;

        // Проверяем границы
        if (newX >= 0 && newX < Map::WIDTH &&
            newY >= 0 && newY < Map::HEIGHT) {

            // Если на новой клетке враг — атакуем его
            for (auto& e : state.enemies) {
                if (e.pos.x == newX && e.pos.y == newY && e.isAlive()) {
                    e.takeDamage(state.player.damage);
                }
            }

            // Перемещаемся, если клетка не стена или это выход
            if (state.map.isWalkable(newX, newY) || state.map.isExit(newX, newY)) {
                state.player.move(dx, dy);
                
                // Проверяем переход на следующий уровень
                if (state.checkExit()) {
                    return; // Уровень уже сгенерирован, выходим
                }
            }
        }
    }

    // Обрабатываем бой и предметы
    state.processCombat();
    state.processItems();

    // Обновляем врагов
    state.updateEnemies();

    // Обрабатываем бой еще раз (на случай, если враг переместился на игрока)
    state.processCombat();

    // Обновляем FOV с учетом стен
    state.map.computeFOV(state.player.pos.x, state.player.pos.y, state.torchRadius, true);
}

// Генерация нового уровня
void GameState::generateNewLevel()
{
    // Очищаем карту и врагов
    map.items.clear();
    enemies.clear();
    
    // Генерируем новую карту
    map.generate();
    
    // Размещаем игрока в безопасном месте (левый верхний угол)
    player.pos.x = 5;
    player.pos.y = 5;
    map.setCell(player.pos.x, player.pos.y, SYM_FLOOR);
    
    // Создаем несколько крыс на случайных позициях
    const int ratsToSpawn = 5 + level; // Больше крыс на более высоких уровнях
    for (int i = 0; i < ratsToSpawn; ++i) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int rx = std::rand() % Map::WIDTH;
            int ry = std::rand() % Map::HEIGHT;

            // Ищем свободную клетку
            if (map.getCell(rx, ry) == SYM_FLOOR &&
                !(rx == player.pos.x && ry == player.pos.y) &&
                !map.isExit(rx, ry)) {
                Entity rat(rx, ry, SYM_ENEMY, TCOD_ColorRGB{255, 50, 50});
                rat.health = 3;
                rat.maxHealth = 3;
                rat.damage = 1;
                enemies.push_back(rat);
                break;
            }
        }
    }

    // Инициализируем FOV
    map.computeFOV(player.pos.x, player.pos.y, torchRadius, true);
}

// Проверка перехода на следующий уровень
bool GameState::checkExit()
{
    if (map.isExit(player.pos.x, player.pos.y)) {
        level++;
        generateNewLevel();
        return true;
    }
    return false;
}

// Перезапуск игры после смерти игрока
void GameState::restartGame()
{
    // Сбрасываем уровень на 1
    level = 1;
    
    // Восстанавливаем здоровье игрока
    player.health = player.maxHealth;
    
    // Генерируем новый уровень (это также очистит карту и врагов)
    generateNewLevel();
}

