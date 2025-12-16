#include "Game.h"

#include <cstdlib>
#include <ctime>
#include <algorithm>

GameState::GameState()
    : map(),
      player(5, 5, SYM_PLAYER, TCOD_ColorRGB{100, 200, 255}),
      isRunning(true),
      torchRadius(8), // Радиус факела
      level(1),       // Начинаем с уровня 1
      shieldTurns(0), // Эффект щита неактивен
      visionTurns(0), // Эффект полной подсветки неактивен
      questActive(false),
      questTarget(0),
      questKills(0)
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
            bool wasAlive = enemies[i].isAlive();
            enemies[i].takeDamage(player.damage);
            if (questActive && wasAlive && !enemies[i].isAlive()) {
                questKills++;
            }
        }

        // Если враг рядом с игроком (в соседней клетке), он атакует
        int dx = abs(enemies[i].pos.x - player.pos.x);
        int dy = abs(enemies[i].pos.y - player.pos.y);
        if ((dx == 1 && dy == 0) || (dx == 0 && dy == 1)) {
            player.takeDamage(enemies[i].damage);
            
            // Если это медведь, отбрасываем игрока
            if (enemies[i].symbol == SYM_BEAR) {
                // Определяем направление от медведя к игроку (игрок отлетает в противоположную сторону)
                int knockbackDx = 0;
                int knockbackDy = 0;
                
                if (enemies[i].pos.x < player.pos.x) {
                    // Медведь слева, игрок отлетает вправо
                    knockbackDx = 1;
                } else if (enemies[i].pos.x > player.pos.x) {
                    // Медведь справа, игрок отлетает влево
                    knockbackDx = -1;
                }
                
                if (enemies[i].pos.y < player.pos.y) {
                    // Медведь сверху, игрок отлетает вниз
                    knockbackDy = 1;
                } else if (enemies[i].pos.y > player.pos.y) {
                    // Медведь снизу, игрок отлетает вверх
                    knockbackDy = -1;
                }
                
                // Случайное количество клеток от 4 до 7
                int knockbackDistance = 4 + (std::rand() % 4); // 4, 5, 6 или 7
                
                // Если есть эффект щита, отбрасывание не действует
                if (shieldTurns > 0) {
                    // Эффект щита даст защиту и уменьшится после любого хода
                    continue;
                }
                // Применяем отбрасывание обычным образом
                for (int step = 0; step < knockbackDistance; ++step) {
                    int newX = player.pos.x + knockbackDx;
                    int newY = player.pos.y + knockbackDy;
                    
                    // Проверяем границы и проходимость
                    if (newX >= 0 && newX < Map::WIDTH &&
                        newY >= 0 && newY < Map::HEIGHT &&
                        map.isWalkable(newX, newY)) {
                        // Проверяем, нет ли там врага
                        bool canMove = true;
                        for (const auto& e : enemies) {
                            if (e.isAlive() && e.pos.x == newX && e.pos.y == newY) {
                                canMove = false;
                                break;
                            }
                        }
                        
                        if (canMove) {
                            player.move(knockbackDx, knockbackDy);
                        } else {
                            // Если уперлись во врага, останавливаемся
                            break;
                        }
                    } else {
                        // Если уперлись в стену или границу, останавливаемся
                        break;
                    }
                }
            }
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
        // Завершаем квест и обнуляем состояние
        questActive = false;
        questKills = 0;
        questTarget = 0;
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

            // Прозрачный предмет '.' наносит 15% от максимального здоровья.
            if (item.symbol == SYM_GHOST_ITEM) {
                int damage = std::max(1, player.maxHealth * 15 / 100);
                player.health -= damage;
                if (player.health < 0) {
                    player.health = 0;
                }
            }

            // Предмет-щит 'O' даёт защиту от откидывания (30 ходов).
            if (item.symbol == SYM_SHIELD) {
                shieldTurns = 30;
            }

            // Квестовый предмет '?' запускает задание на убийство N монстров.
            if (item.symbol == SYM_QUEST && !questActive) {
                questActive = true;
                questKills = 0;
                questTarget = 5 + (std::rand() % 6); // от 5 до 10
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
        // Если действует щит — уменьшаем количество ходов
        if (state.shieldTurns > 0) {
            state.shieldTurns--;
        }

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
    int killsBefore = state.questKills;
    state.processCombat();
    state.processItems();
    // Проверяем выполнение квеста после возможных убийств
    if (state.questActive && state.questKills >= state.questTarget) {
        state.questActive = false;
        state.questKills = 0;
        state.questTarget = 0;
        // Активируем эффект полной подсветки на следующий ход
        state.visionTurns = 1;
    }

    // Обновляем врагов
    state.updateEnemies();

    // Обрабатываем бой еще раз (на случай, если враг переместился на игрока)
    state.processCombat();

    // Обновляем FOV
    if (state.visionTurns > 0) {
        // Полная подсветка карты: игнорируем обычный FOV
        state.map.revealAll();
        // Эффект действует только до следующего хода
        state.visionTurns--;
    } else {
        // Обычный FOV с учетом стен
        state.map.computeFOV(state.player.pos.x, state.player.pos.y, state.torchRadius, true);
    }
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

    // Спавним призрачные предметы '.' в случайных местах (не больше 5 за карту)
    const int ghostToSpawn = 1 + (std::rand() % 5); // от 1 до 5 штук
    for (int i = 0; i < ghostToSpawn; ++i) {
        for (int attempt = 0; attempt < 200; ++attempt) {
            int gx = std::rand() % Map::WIDTH;
            int gy = std::rand() % Map::HEIGHT;

            // Свободная клетка: пол, не игрок, не выход, нет врага и предмета
            bool occupiedByEnemy = false;
            for (const auto& e : enemies) {
                if (e.isAlive() && e.pos.x == gx && e.pos.y == gy) {
                    occupiedByEnemy = true;
                    break;
                }
            }
            if (occupiedByEnemy) continue;

            if (map.getCell(gx, gy) == SYM_FLOOR &&
                !(gx == player.pos.x && gy == player.pos.y) &&
                !map.isExit(gx, gy) &&
                map.getItemAt(gx, gy) == nullptr) {
                map.addGhostItem(gx, gy);
                break;
            }
        }
    }

    // Спавним предмет-щиты 'O' (до 3 на карту)
    const int shieldsToSpawn = 3;
    for (int shield = 0; shield < shieldsToSpawn; ++shield) {
        for (int attempt = 0; attempt < 200; ++attempt) {
            int sx = std::rand() % Map::WIDTH;
            int sy = std::rand() % Map::HEIGHT;
            bool occupiedByEnemy = false;
            for (const auto& e : enemies) {
                if (e.isAlive() && e.pos.x == sx && e.pos.y == sy) {
                    occupiedByEnemy = true;
                    break;
                }
            }
            if (occupiedByEnemy) continue;
            if (map.getCell(sx, sy) == SYM_FLOOR &&
                !(sx == player.pos.x && sy == player.pos.y) &&
                !map.isExit(sx, sy) &&
                map.getItemAt(sx, sy) == nullptr) {
                map.addShieldItem(sx, sy);
                break;
            }
        }
    }

    // Спавним один квестовый предмет '?' (запускает убийство монстров)
    for (int attempt = 0; attempt < 200; ++attempt) {
        int qx = std::rand() % Map::WIDTH;
        int qy = std::rand() % Map::HEIGHT;
        bool occupiedByEnemy = false;
        for (const auto& e : enemies) {
            if (e.isAlive() && e.pos.x == qx && e.pos.y == qy) {
                occupiedByEnemy = true;
                break;
            }
        }
        if (occupiedByEnemy) continue;
        if (map.getCell(qx, qy) == SYM_FLOOR &&
            !(qx == player.pos.x && qy == player.pos.y) &&
            !map.isExit(qx, qy) &&
            map.getItemAt(qx, qy) == nullptr) {
            map.addQuestItem(qx, qy);
            break;
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
    
    // Сбросим максимальное здоровье и щит
    player.maxHealth = 20;
    player.health = player.maxHealth;
    shieldTurns = 0;
    visionTurns = 0;
    // Генерируем новый уровень (это также очистит карту и врагов)
    generateNewLevel();
}

