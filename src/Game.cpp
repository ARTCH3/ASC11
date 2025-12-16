#include "Game.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace {
// Простой helper для целых случайных чисел в диапазоне [min, max].
int rng(int min, int max)
{
    return min + (std::rand() % (max - min + 1));
}

// Проверка, стоят ли клетки по соседству по стороне.
bool isAdjacent(const Position& a, const Position& b)
{
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    return (dx == 1 && dy == 0) || (dx == 0 && dy == 1);
}

// Отбрасывание игрока от медведя.
void applyBearKnockback(GameState& state, const Entity& bear)
{
    const int knockbackDx = (bear.pos.x < state.player.pos.x) ? 1
                           : (bear.pos.x > state.player.pos.x) ? -1
                           : 0;
    const int knockbackDy = (bear.pos.y < state.player.pos.y) ? 1
                           : (bear.pos.y > state.player.pos.y) ? -1
                           : 0;

    if (knockbackDx == 0 && knockbackDy == 0) {
        return;
    }

    const int knockbackDistance = rng(4, 7);
    for (int step = 0; step < knockbackDistance; ++step) {
        const int newX = state.player.pos.x + knockbackDx;
        const int newY = state.player.pos.y + knockbackDy;

        if (!state.map.inBounds(newX, newY) || !state.map.isWalkable(newX, newY)) {
            break;
        }

        bool blocked = false;
        for (const auto& e : state.enemies) {
            if (e.isAlive() && e.pos.x == newX && e.pos.y == newY) {
                blocked = true;
                break;
            }
        }
        if (blocked) {
            break;
        }

        state.player.move(knockbackDx, knockbackDy);
    }
}
} // namespace

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
        if (map.inBounds(newX, newY) &&
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
    for (auto& enemy : enemies) {
        if (!enemy.isAlive()) {
            continue;
        }

        if (enemy.pos.x == player.pos.x && enemy.pos.y == player.pos.y) {
            enemy.takeDamage(player.damage);
        }

        if (isAdjacent(enemy.pos, player.pos)) {
            player.takeDamage(enemy.damage);
            if (enemy.symbol == SYM_BEAR) {
                applyBearKnockback(*this, enemy);
            }
        }
    }

    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
                       [](const Entity& e) { return !e.isAlive(); }),
        enemies.end());

    if (!player.isAlive()) {
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
    else if (key == TCODK_ESCAPE || key == 27) {
        state.isRunning = false;
        return;
    }

    // Перемещаем игрока
    if (dx != 0 || dy != 0) {
        int newX = state.player.pos.x + dx;
        int newY = state.player.pos.y + dy;

        // Проверяем границы
        if (state.map.inBounds(newX, newY)) {

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

    // Создаем медведей на случайных позициях
    const int bearsToSpawn = 2 + level / 2; // Медведей меньше, чем крыс
    for (int i = 0; i < bearsToSpawn; ++i) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int bx = std::rand() % Map::WIDTH;
            int by = std::rand() % Map::HEIGHT;

            // Ищем свободную клетку
            if (map.getCell(bx, by) == SYM_FLOOR &&
                !(bx == player.pos.x && by == player.pos.y) &&
                !map.isExit(bx, by)) {
                Entity bear(bx, by, SYM_BEAR, TCOD_ColorRGB{139, 69, 19}); // Коричневый цвет
                // Случайное здоровье от 8 до 12
                bear.maxHealth = 8 + (std::rand() % 5); // 8, 9, 10, 11 или 12
                bear.health = bear.maxHealth;
                // Случайный урон от 3 до 5
                bear.damage = 3 + (std::rand() % 3); // 3, 4 или 5
                enemies.push_back(bear);
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

