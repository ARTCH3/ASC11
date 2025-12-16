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

        // Простое приближение к игроку.
        // Для обычных врагов (крыса, медведь) — как раньше: двигаемся по прямой.
        // Для змеи — особое поведение: ходит только по диагонали.
        if (enemies[i].symbol == SYM_SNAKE) {
            // Считаем направление до игрока по осям.
            int diffX = player.pos.x - enemies[i].pos.x;
            int diffY = player.pos.y - enemies[i].pos.y;

            if (diffX > 0) dx = 1;
            else if (diffX < 0) dx = -1;

            if (diffY > 0) dy = 1;
            else if (diffY < 0) dy = -1;

            // Змея ходит только по диагонали: если одна из осей совпадает,
            // она просто стоит на месте и ждет более удобного момента.
            if (dx == 0 || dy == 0) {
                dx = 0;
                dy = 0;
            }
        } else {
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
            // Игрок атакует врага.
            // Змея умирает от одного удара, остальные враги получают обычный урон.
            if (enemies[i].symbol == SYM_SNAKE) {
                enemies[i].health = 0;
            } else {
                enemies[i].takeDamage(player.damage);
            }
        }

        // Если враг рядом с игроком, он атакует.
        // Для обычных врагов — по вертикали/горизонтали,
        // для змеи — допускаем диагональное соседство (укус по диагонали).
        int dx = std::abs(enemies[i].pos.x - player.pos.x);
        int dy = std::abs(enemies[i].pos.y - player.pos.y);

        bool isAdjacent = false;
        if (enemies[i].symbol == SYM_SNAKE) {
            // Любая соседняя клетка (8 направлений), кроме самой клетки игрока.
            isAdjacent = (dx <= 1 && dy <= 1 && (dx + dy) > 0);
        } else {
            // Как раньше: только по кресту.
            isAdjacent = ((dx == 1 && dy == 0) || (dx == 0 && dy == 1));
        }

        if (isAdjacent) {
            if (enemies[i].symbol == SYM_SNAKE) {
                // Укус змеи: моментально снимаем ~1% HP
                // и вешаем яд, который будет снимать ~1% в ход.
                int instantDamage = std::max(1, player.maxHealth / 100);
                player.takeDamage(instantDamage);

                // Каждый новый укус не накапливает яд, а просто обновляет его длительность.
                applyPoisonToPlayer(5, 10);
            } else {
                // Обычная атака
                player.takeDamage(enemies[i].damage);
            }
            
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
                
                // Применяем отбрасывание
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
        // Вместо выхода из игры, перезапускаем её
        restartGame();
    }
}

// Обновление эффекта отравления.
void GameState::updatePoison()
{
    if (!isPlayerPoisoned) {
        return;
    }

    if (poisonTurnsRemaining <= 0) {
        // На всякий случай сбрасываем состояние.
        isPlayerPoisoned = false;
        poisonTurnsRemaining = 0;
        return;
    }

    // Яд наносит небольшой периодический урон — примерно 1% от максимального HP.
    int poisonDamage = std::max(1, player.maxHealth / 100);
    player.takeDamage(poisonDamage);
    poisonTurnsRemaining--;

    if (poisonTurnsRemaining <= 0) {
        isPlayerPoisoned = false;
        poisonTurnsRemaining = 0;
    }

    // Если яд добил игрока — перезапускаем игру так же, как и при обычной смерти.
    if (!player.isAlive()) {
        restartGame();
    }
}

// Вешаем яд на игрока. Эффект не накапливается, а просто обновляет время действия.
void GameState::applyPoisonToPlayer(int minTurns, int maxTurns)
{
    if (maxTurns < minTurns) {
        maxTurns = minTurns;
    }

    int range = maxTurns - minTurns + 1;
    int duration = minTurns + (range > 0 ? std::rand() % range : 0);

    isPlayerPoisoned = true;
    poisonTurnsRemaining = duration;
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

            // Если на новой клетке враг — атакуем его.
            // Змея умирает с одного удара, другие враги получают обычный урон.
            for (auto& e : state.enemies) {
                if (e.pos.x == newX && e.pos.y == newY && e.isAlive()) {
                    if (e.symbol == SYM_SNAKE) {
                        e.health = 0;
                    } else {
                        e.takeDamage(state.player.damage);
                    }
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

    // Обновляем эффект отравления после хода (яд тикает по ходам игрока).
    state.updatePoison();

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

    // Создаем змей на случайных позициях
    const int snakesToSpawn = 3 + level / 2; // Змей немного больше, чем медведей
    for (int i = 0; i < snakesToSpawn; ++i) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int sx = std::rand() % Map::WIDTH;
            int sy = std::rand() % Map::HEIGHT;

            // Ищем свободную клетку
            if (map.getCell(sx, sy) == SYM_FLOOR &&
                !(sx == player.pos.x && sy == player.pos.y) &&
                !map.isExit(sx, sy)) {
                // Болотно-зелёный цвет для змеи
                Entity snake(sx, sy, SYM_SNAKE, TCOD_ColorRGB{60, 130, 60});
                // Здоровье змеи чуть больше, чем у крысы, но меньше, чем у медведя
                snake.maxHealth = 4 + (std::rand() % 3); // 4–6
                snake.health = snake.maxHealth;
                // Урон через поле damage не используем (змея бьёт в процентах от HP),
                // но заполним его маленьким значением для наглядности.
                snake.damage = 1;
                enemies.push_back(snake);
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

    // Полностью очищаем эффект отравления,
    // чтобы он не "переезжал" в новую игру после смерти игрока.
    isPlayerPoisoned = false;
    poisonTurnsRemaining = 0;

    // Восстанавливаем здоровье игрока
    player.health = player.maxHealth;
    
    // Генерируем новый уровень (это также очистит карту и врагов)
    generateNewLevel();
}

