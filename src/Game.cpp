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
      enemies(),
      isRunning(true),
      torchRadius(8), // Радиус факела
      level(1),       // Начинаем с уровня 1
      shieldTurns(0), // Сколько синих делений щита осталось
      shieldWhiteSegments(0),
      visionTurns(0), // Количество ходов с полной подсветкой
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
        } else if (enemies[i].symbol == SYM_GHOST) {
            // Призрак двигается свободно по диагонали и игнорирует стены.
            // Он всегда старается приблизиться к игроку на 1 клетку (как "король" в шахматах).
            int diffX = player.pos.x - enemies[i].pos.x;
            int diffY = player.pos.y - enemies[i].pos.y;

            if (diffX > 0) dx = 1;
            else if (diffX < 0) dx = -1;

            if (diffY > 0) dy = 1;
            else if (diffY < 0) dy = -1;
        } else if (enemies[i].symbol == SYM_CRAB) {
            // Краб: ходит только по вертикали/горизонтали.
            // В обычном состоянии старается приблизиться к игроку.
            // В состоянии "паники" после отцепления — наоборот убегает от него.
            Entity& crab = enemies[i];

            if (crab.crabAttachedToPlayer) {
                // Прицепленный краб не двигается по карте.
                dx = 0;
                dy = 0;
            } else {
                int diffX = player.pos.x - crab.pos.x;
                int diffY = player.pos.y - crab.pos.y;

                if (crab.crabAttachmentCooldown > 0) {
                    // Краб в панике убегает от игрока:
                    // выбираем направление, противоположное игроку.
                    if (diffX > 0) dx = -1;
                    else if (diffX < 0) dx = 1;

                    if (diffY > 0) dy = -1;
                    else if (diffY < 0) dy = 1;

                    // Каждый ход уменьшаем таймер "паники".
                    crab.crabAttachmentCooldown--;
                    if (crab.crabAttachmentCooldown <= 0) {
                        crab.crabAttachmentCooldown = 0;
                        // Как только откат закончился — возвращаем яркий цвет.
                        crab.color = TCOD_ColorRGB{255, 140, 0}; // ярко-оранжевый
                    }
                } else {
                    // Обычное состояние: краб хочет приблизиться к игроку.
                    if (diffX > 0) dx = 1;
                    else if (diffX < 0) dx = -1;

                    if (diffY > 0) dy = 1;
                    else if (diffY < 0) dy = -1;
                }

                // Краб может ходить только по вертикали или горизонтали,
                // поэтому случайно выбираем одно из направлений и обнуляем второе.
                if (std::rand() % 2 == 0) {
                    dy = 0;
                } else {
                    dx = 0;
                }
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
            !(newX == player.pos.x && newY == player.pos.y)) {

            // Обычные враги уважают стены, призрак — нет.
            bool canMoveThroughCell = true;
            if (enemies[i].symbol != SYM_GHOST) {
                canMoveThroughCell = map.isWalkable(newX, newY);
            }

            if (canMoveThroughCell) {
                enemies[i].move(dx, dy);
            }
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

        // Если враг на той же клетке, что и игрок
        if (enemy.pos.x == player.pos.x &&
            enemy.pos.y == player.pos.y) {
            // Игрок атакует врага.
            // Змея умирает от одного удара, остальные враги получают обычный урон.
            bool wasAlive = enemy.isAlive();
            if (enemy.symbol == SYM_SNAKE) {
                enemy.health = 0;
            } else {
                enemy.takeDamage(player.damage);
            }
            // Отслеживаем убийства для квеста
            if (questActive && wasAlive && !enemy.isAlive()) {
                questKills++;
            }
        }

        // Если враг рядом с игроком, он атакует.
        // Для обычных врагов — по вертикали/горизонтали,
        // для змеи — допускаем диагональное соседство (укус по диагонали).
        int dx = std::abs(enemy.pos.x - player.pos.x);
        int dy = std::abs(enemy.pos.y - player.pos.y);

        bool isAdjacent = false;
        if (enemy.symbol == SYM_SNAKE) {
            // Любая соседняя клетка (8 направлений), кроме самой клетки игрока.
            isAdjacent = (dx <= 1 && dy <= 1 && (dx + dy) > 0);
        } else if (enemy.symbol == SYM_GHOST) {
            // Призрак тоже атакует с любой соседней клетки (8 направлений).
            isAdjacent = (dx <= 1 && dy <= 1 && (dx + dy) > 0);
        } else {
            // Как раньше: только по кресту.
            isAdjacent = ((dx == 1 && dy == 0) || (dx == 0 && dy == 1));
        }

        if (isAdjacent) {
            if (enemy.symbol == SYM_SNAKE) {
                // Укус змеи игнорирует щит! Моментально снимаем ~1% HP
                int instantDamage = std::max(1, player.maxHealth / 100);
                player.takeDamage(instantDamage);

                // Яд игнорирует щит
                applyPoisonToPlayer(5, 10);
            } else if (enemy.symbol == SYM_GHOST) {
                // Призрак "прицепляется" к игроку:
                // наносит примерно 1% от максимального HP единоразово
                // и прячет информацию о здоровье на несколько ходов.
                int ghostDamage = std::max(1, player.maxHealth / 100);
                int remaining = applyShieldHit(ghostDamage);
                if (remaining > 0) {
                    player.takeDamage(remaining);

                    // Каждый новый контакт просто обновляет длительность эффекта.
                    applyGhostCurseToPlayer(8, 12);

                    // После успешной атаки и наложения эффекта призрак "рассеивается":
                    // он больше не существует на карте.
                    enemy.health = 0;
                }
            } else if (enemy.symbol == SYM_CRAB) {
                // Особое поведение краба.
                // Если управление ещё НЕ инвертировано и краб не в откате,
                // то при приближении он "прицепляется" к игроку.
                if (!isPlayerControlsInverted && enemy.crabAttachmentCooldown == 0) {
                    // Вешаем эффект инверсии управления.
                    applyCrabInversionToPlayer(8, 15);

                    // Помечаем, что именно этот краб прицепился к игроку.
                    enemy.crabAttachedToPlayer = true;
                    // Краб "садится" на игрока: его координаты становятся координатами игрока.
                    enemy.pos.x = player.pos.x;
                    enemy.pos.y = player.pos.y;
                } else {
                    // Если эффект уже висит (или краб недавно отцепился),
                    // он не может прицепиться и просто наносит небольшой урон (~1% HP).
                    int crabDamage = std::max(1, player.maxHealth / 100);
                    int remaining = applyShieldHit(crabDamage);
                    if (remaining > 0) {
                        player.takeDamage(remaining);
                    }
                }
            } else {
                // Обычная атака
                int remaining = applyShieldHit(enemy.damage);
                if (remaining > 0) {
                    player.takeDamage(remaining);
                }
            }
            
            // Если это медведь, отбрасываем игрока
            if (enemy.symbol == SYM_BEAR) {
                // Определяем направление от медведя к игроку (игрок отлетает в противоположную сторону)
                int knockbackDx = 0;
                int knockbackDy = 0;
                
                if (enemy.pos.x < player.pos.x) {
                    // Медведь слева, игрок отлетает вправо
                    knockbackDx = 1;
                } else if (enemy.pos.x > player.pos.x) {
                    // Медведь справа, игрок отлетает влево
                    knockbackDx = -1;
                }
                
                if (enemy.pos.y < player.pos.y) {
                    // Медведь сверху, игрок отлетает вниз
                    knockbackDy = 1;
                } else if (enemy.pos.y > player.pos.y) {
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

// Обновление эффекта "проклятия призрака" — он только скрывает HP,
// урона сам по себе не наносит.
void GameState::updateGhostCurse()
{
    if (!isPlayerGhostCursed) {
        return;
    }

    if (ghostCurseTurnsRemaining <= 0) {
        isPlayerGhostCursed = false;
        ghostCurseTurnsRemaining = 0;
        return;
    }

    // Каждый ход игрока уменьшаем таймер.
    ghostCurseTurnsRemaining--;

    if (ghostCurseTurnsRemaining <= 0) {
        isPlayerGhostCursed = false;
        ghostCurseTurnsRemaining = 0;
    }
}

// Вешаем на игрока эффект призрака на случайное количество ходов.
void GameState::applyGhostCurseToPlayer(int minTurns, int maxTurns)
{
    if (maxTurns < minTurns) {
        maxTurns = minTurns;
    }

    int range = maxTurns - minTurns + 1;
    int duration = minTurns + (range > 0 ? std::rand() % range : 0);

    isPlayerGhostCursed = true;
    ghostCurseTurnsRemaining = duration;
}

// Обновление эффекта краба — он инвертирует управление игрока
// на ограниченное количество ходов, а затем "отцепляется",
// нанося дополнительный урон и убегая от игрока.
void GameState::updateCrabInversion()
{
    if (!isPlayerControlsInverted) {
        return;
    }

    if (crabInversionTurnsRemaining > 0) {
        crabInversionTurnsRemaining--;
    }

    if (crabInversionTurnsRemaining > 0) {
        return;
    }

    // Эффект закончился: сбрасываем флаг.
    isPlayerControlsInverted = false;
    crabInversionTurnsRemaining = 0;

    // Ищем краба, который был прицеплен к игроку.
    // Краб при отцеплении наносит ~3% урона от максимального здоровья,
    // отскакивает на две клетки от игрока и впадает в "панический побег".
    for (auto& enemy : enemies) {
        if (!enemy.isAlive()) {
            continue;
        }
        if (enemy.symbol != SYM_CRAB) {
            continue;
        }
        if (!enemy.crabAttachedToPlayer) {
            continue;
        }

        // Наносим урон при отцеплении.
        int detachDamage = std::max(1, player.maxHealth * 3 / 100);
        player.takeDamage(detachDamage);

        // Переводим краба в состояние "убегает от игрока".
        enemy.crabAttachedToPlayer = false;
        // Краб некоторое время не может снова прицепляться.
        int minCooldown = 15;
        int maxCooldown = 35;
        if (maxCooldown < minCooldown) {
            maxCooldown = minCooldown;
        }
        int range = maxCooldown - minCooldown + 1;
        enemy.crabAttachmentCooldown = minCooldown + (range > 0 ? std::rand() % range : 0);

        // Отскакиваем краба на две клетки от игрока.
        // Выбираем одно из четырёх направлений, в котором получится поставить краба.
        const int dirs[4][2] = {
            { 1,  0},  // вправо
            {-1,  0},  // влево
            { 0,  1},  // вниз
            { 0, -1}   // вверх
        };
        for (int attempt = 0; attempt < 4; ++attempt) {
            int idx = std::rand() % 4;
            int dx = dirs[idx][0];
            int dy = dirs[idx][1];

            int targetX = player.pos.x + dx * 2;
            int targetY = player.pos.y + dy * 2;

            if (targetX < 0 || targetX >= Map::WIDTH ||
                targetY < 0 || targetY >= Map::HEIGHT) {
                continue;
            }
            if (!map.isWalkable(targetX, targetY)) {
                continue;
            }

            enemy.pos.x = targetX;
            enemy.pos.y = targetY;
            break;
        }

        // Делаем цвет краба более тусклым, чтобы можно было отличить
        // испуганного краба, который пока не может прицепляться.
        enemy.color = TCOD_ColorRGB{200, 120, 40}; // тускло-оранжевый

        break;
    }

    // Если урон от отцепления убил игрока — перезапускаем игру.
    if (!player.isAlive()) {
        restartGame();
    }
}

// Щит поглощает урон по делениям.
// Возвращает, сколько урона осталось нанести по здоровью игрока.
int GameState::applyShieldHit(int damage)
{
    int remaining = damage;
    while (remaining > 0 && (shieldTurns > 0 || shieldWhiteSegments > 0)) {
        if (shieldTurns > 0) {
            // Сначала тратим синее деление: оно становится белым.
            --shieldTurns;
            ++shieldWhiteSegments;
        } else if (shieldWhiteSegments > 0) {
            // Если синих нет, "сжигаем" белые в серые.
            --shieldWhiteSegments;
        }
        --remaining;
    }
    return remaining;
}

// Вешаем на игрока эффект краба: инвертируем управление на случайное число ходов.
void GameState::applyCrabInversionToPlayer(int minTurns, int maxTurns)
{
    if (maxTurns < minTurns) {
        maxTurns = minTurns;
    }

    int range = maxTurns - minTurns + 1;
    int duration = minTurns + (range > 0 ? std::rand() % range : 0);

    isPlayerControlsInverted = true;
    crabInversionTurnsRemaining = duration;
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

            // Ловушка (мина) '.' наносит 15% от максимального здоровья (щит не защищает).
            if (item.symbol == SYM_TRAP) {
                int damage = std::max(1, player.maxHealth * 15 / 100);
                player.health -= damage;
                if (player.health < 0) {
                    player.health = 0;
                }
            }

            // Предмет-щит 'O' даёт полный щит: количество делений равно ширине карты.
            if (item.symbol == SYM_SHIELD) {
                shieldTurns = Map::WIDTH;        // все деления синие
                shieldWhiteSegments = 0;         // нет "повреждённых" делений
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
    else if (key == TCODK_ESCAPE || key == 27) {
        state.isRunning = false;
        return;
    }

    // Если на игроке висит эффект краба — инвертируем направление движения.
    if (state.isPlayerControlsInverted) {
        dx = -dx;
        dy = -dy;
    }

    // Если активен эффект краба и нажата не WASD/WASD/QEZC — ручное снятие краба
    if (state.isPlayerControlsInverted) {
        // Комплект допустимых клавиш (верхний + нижний регистр)
        if (!(key == 'w' || key == 'a' || key == 's' || key == 'd' ||
              key == 'q' || key == 'e' || key == 'z' || key == 'c' ||
              key == 'W' || key == 'A' || key == 'S' || key == 'D' ||
              key == 'Q' || key == 'E' || key == 'Z' || key == 'C')) {
            // Найти прицепленного краба
            for (auto& crab : state.enemies) {
                if (crab.symbol == SYM_CRAB && crab.crabAttachedToPlayer && crab.isAlive()) {
                    crab.crabAttachedToPlayer = false;
                    // выставить откат
                    int minCooldown = 15;
                    int maxCooldown = 35;
                    int range = maxCooldown - minCooldown + 1;
                    crab.crabAttachmentCooldown = minCooldown + (range > 0 ? std::rand() % range : 0);
                    crab.color = TCOD_ColorRGB{200, 120, 40};
                    // Краб появляется на 1 клетку рядом с игроком (ищем первую свободную)
                    const int dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
                    for (int t = 0; t < 4; ++t) {
                        int dxTry = dirs[t][0];
                        int dyTry = dirs[t][1];
                        int nx = state.player.pos.x + dxTry;
                        int ny = state.player.pos.y + dyTry;
                        if (nx >= 0 && nx < Map::WIDTH && ny >= 0 && ny < Map::HEIGHT && state.map.isWalkable(nx, ny)) {
                            crab.pos.x = nx;
                            crab.pos.y = ny;
                            break;
                        }
                    }
                    break;
                }
            }
            // Снимаем эффект краба с игрока
            state.isPlayerControlsInverted = false;
            state.crabInversionTurnsRemaining = 0;
        }
    }
    // Перемещаем игрока
    if (dx != 0 || dy != 0) {
        int newX = state.player.pos.x + dx;
        int newY = state.player.pos.y + dy;
            // Если действует щит — сначала "сжигаем" белые деления, затем синие.
            if (state.shieldTurns > 0 || state.shieldWhiteSegments > 0) {
                if (state.shieldWhiteSegments > 0) {
                    state.shieldWhiteSegments--;
                } else if (state.shieldTurns > 0) {
                    state.shieldTurns--;
                }
            }

        // Проверяем границы
        if (state.map.inBounds(newX, newY)) {

            // Если на новой клетке враг — атакуем его.
            // Змея и краб умирают с одного удара (краб может иметь особое поведение,
            // если он был прицеплен к игроку).
            for (auto& e : state.enemies) {
                if (e.pos.x == newX && e.pos.y == newY && e.isAlive()) {
                    if (e.symbol == SYM_SNAKE) {
                        e.health = 0;
                    } else if (e.symbol == SYM_CRAB && e.crabAttachedToPlayer) {
                        // Игрок "наступает" на краба, который был прицеплен.
                        // Эффект инверсии снимается, краб отлетает на 2 клетки
                        // дальше по направлению шага и начинает убегать.
                        state.isPlayerControlsInverted = false;
                        state.crabInversionTurnsRemaining = 0;

                        e.crabAttachedToPlayer = false;

                        // Задаем откат, в течение которого краб не сможет снова прицепляться.
                        int minCooldown = 15;
                        int maxCooldown = 35;
                        if (maxCooldown < minCooldown) {
                            maxCooldown = minCooldown;
                        }
                        int range = maxCooldown - minCooldown + 1;
                        e.crabAttachmentCooldown = minCooldown + (range > 0 ? std::rand() % range : 0);

                        // Переносим краба на 2 клетки дальше от игрока по направлению шага.
                        e.pos.x = newX + dx;
                        e.pos.y = newY + dy;
                        e.pos.x += dx;
                        e.pos.y += dy;

                        // Делаем цвет тусклым — такой краб пока не может цепляться.
                        e.color = TCOD_ColorRGB{200, 120, 40};
                    } else if (e.symbol == SYM_CRAB) {
                        // Обычный краб без особого состояния умирает с одного удара.
                        e.health = 0;
                    } else {
                        e.takeDamage(state.player.damage);
                    }
                }
            }

            // Перемещаемся, если клетка не стена или это выход
            if (state.map.isWalkable(newX, newY) || state.map.isExit(newX, newY)) {
                state.player.move(dx, dy);

                // Если на игроке "сидит" краб, он должен оставаться на тех же координатах,
                // что и игрок, пока не отцепится.
                for (auto& e : state.enemies) {
                    if (e.symbol == SYM_CRAB && e.crabAttachedToPlayer && e.isAlive()) {
                        e.pos.x = state.player.pos.x;
                        e.pos.y = state.player.pos.y;
                    }
                }
                
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

    // Обновляем эффект отравления после хода (яд тикает по ходам игрока).
    state.updatePoison();

    // Обновляем эффект призрака (просто тикает таймер хода игрока).
    state.updateGhostCurse();

    // Обновляем эффект краба (инвертированное управление).
    state.updateCrabInversion();

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
    
    // Размещаем игрока в безопасном месте с выходами (не в коробке!)
    // Ищем проходимую клетку с минимум 2 выходами В ЦЕНТРЕ КАРТЫ (или очень рядом)
    bool playerPlaced = false;
    int centerX = Map::WIDTH / 2;
    int centerY = Map::HEIGHT / 2;
    int spawnRadius = 12; // Радиус поиска от центра (можно менять: меньше = ближе к центру)
    for (int attempt = 0; attempt < 500 && !playerPlaced; ++attempt) {
        // Спавним в центре или рядом с центром (±spawnRadius клеток)
        int px = centerX - spawnRadius + std::rand() % (spawnRadius * 2 + 1);
        int py = centerY - spawnRadius + std::rand() % (spawnRadius * 2 + 1);
        // Ограничиваем границами карты
        px = std::max(2, std::min(Map::WIDTH - 3, px));
        py = std::max(2, std::min(Map::HEIGHT - 3, py));
        
        if (map.getCell(px, py) == SYM_FLOOR) {
            // Проверяем что вокруг есть минимум 2 проходимых клетки (выходы)
            int exits = 0;
            if (map.isWalkable(px - 1, py)) exits++;
            if (map.isWalkable(px + 1, py)) exits++;
            if (map.isWalkable(px, py - 1)) exits++;
            if (map.isWalkable(px, py + 1)) exits++;
            
            if (exits >= 2) {
                player.pos.x = px;
                player.pos.y = py;
                map.setCell(player.pos.x, player.pos.y, SYM_FLOOR);
                playerPlaced = true;
            }
        }
    }
    // Если не нашли подходящее место, ставим в центр и гарантируем выходы
    if (!playerPlaced) {
        player.pos.x = Map::WIDTH / 2;
        player.pos.y = Map::HEIGHT / 2;
        map.setCell(player.pos.x, player.pos.y, SYM_FLOOR);
        // Гарантируем минимум 2 выхода вокруг игрока
        int dirs[4][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
        int exitsCreated = 0;
        for (int d = 0; d < 4 && exitsCreated < 2; ++d) {
            int nx = player.pos.x + dirs[d][0];
            int ny = player.pos.y + dirs[d][1];
            if (map.inBounds(nx, ny) && map.getCell(nx, ny) == SYM_WALL) {
                map.setCell(nx, ny, SYM_FLOOR);
                exitsCreated++;
            }
        }
    }

    // На новом уровне всегда начинаем без активного эффекта краба.
    isPlayerControlsInverted = false;
    crabInversionTurnsRemaining = 0;
    
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

    // Создаем призраков на случайных позициях
    const int ghostsToSpawn = 1 + level / 2; // Немного, но они опасные
    for (int i = 0; i < ghostsToSpawn; ++i) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int gx = std::rand() % Map::WIDTH;
            int gy = std::rand() % Map::HEIGHT;

            // Ищем свободную клетку пола (как для обычных врагов)
            if (map.getCell(gx, gy) == SYM_FLOOR &&
                !(gx == player.pos.x && gy == player.pos.y) &&
                !map.isExit(gx, gy)) {
                // Призрак — серый полупрозрачный враг
                Entity ghost(gx, gy, SYM_GHOST, TCOD_ColorRGB{170, 170, 170});
                ghost.maxHealth = 5;
                ghost.health = ghost.maxHealth;
                // Урон хранить тоже будем, но основной урон — процентный, как в описании.
                ghost.damage = 1;
                enemies.push_back(ghost);
                break;
            }
        }
    }

    // Создаем крабов на случайных позициях
    const int crabsToSpawn = 2 + level / 2;
    for (int i = 0; i < crabsToSpawn; ++i) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int cx = std::rand() % Map::WIDTH;
            int cy = std::rand() % Map::HEIGHT;

            if (map.getCell(cx, cy) == SYM_FLOOR &&
                !(cx == player.pos.x && cy == player.pos.y) &&
                !map.isExit(cx, cy)) {
                // Ярко-оранжевый цвет для обычного краба
                Entity crab(cx, cy, SYM_CRAB, TCOD_ColorRGB{255, 140, 0});
                crab.maxHealth = 4;
                crab.health = crab.maxHealth;
                crab.damage = 1; // основной "урон" краба — особые эффекты
                enemies.push_back(crab);
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
                map.addTrapItem(gx, gy);
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

    // Полностью очищаем эффект отравления,
    // чтобы он не "переезжал" в новую игру после смерти игрока.
    isPlayerPoisoned = false;
    poisonTurnsRemaining = 0;

    // Сбрасываем эффект призрака — после смерти начинаем "чисто".
    isPlayerGhostCursed = false;
    ghostCurseTurnsRemaining = 0;

    // Сбрасываем эффект краба (инверсия управления).
    isPlayerControlsInverted = false;
    crabInversionTurnsRemaining = 0;

    // Сбрасываем максимальное здоровье и щит
    player.maxHealth = 20;
    player.health = player.maxHealth;
    shieldTurns = 0;
    visionTurns = 0;
    // Генерируем новый уровень (это также очистит карту и врагов)
    generateNewLevel();
}

