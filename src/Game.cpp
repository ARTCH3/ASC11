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
      torchRadius(8), // Базовый радиус факела
      level(1),       // Начинаем с уровня 1
      shieldTurns(0), // Сколько синих делений щита осталось
      shieldWhiteSegments(0),
      visionTurns(0), // Количество ходов с полной подсветкой
      questActive(false),
      questTarget(0),
      questKills(0),
      isPerkChoiceActive(false),
      perkBonusRats(0),
      perkBonusHeals(0),
      perkBonusShields(0),
      perkFireflyEnabled(false),
      perkShowExitFirst3Steps(false),
      stepsOnCurrentLevel(0),
      perkBearPoisonNextLevel(false),
      perkBearPoisonActiveThisLevel(false),
      perkSnakesNextLevel(0),
      perkExtraMaxHpItemsNextLevel(0),
      perkTorchRadiusDeltaNextLevel(0),
      showExitBecauseCleared(false)
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

        // Помечаем, что этот тип врага уже \"встречен\" (для Legend),
        // если он находится в пределах видимости игрока.
        if (map.isVisible(enemy.pos.x, enemy.pos.y)) {
            if (enemy.symbol == SYM_ENEMY) seenRat = true;
            else if (enemy.symbol == SYM_BEAR) seenBear = true;
            else if (enemy.symbol == SYM_SNAKE) seenSnake = true;
            else if (enemy.symbol == SYM_GHOST) seenGhost = true;
            else if (enemy.symbol == SYM_CRAB) seenCrab = true;
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
                // Обычная атака (с учётом возможного перка на "ядовитых" медведей).
                if (enemy.symbol == SYM_BEAR && perkBearPoisonActiveThisLevel) {
                    // Медведь с мутацией отравления: укус работает как у змеи.
                    // Игнорируем щит, наносим небольшой прямой урон и вешаем яд.
                    int instantDamage = std::max(1, player.maxHealth / 100);
                    player.takeDamage(instantDamage);
                    applyPoisonToPlayer(5, 10);
                } else {
                    // Обычный урон проходит сначала по щиту, затем по здоровью.
                    int remaining = applyShieldHit(enemy.damage);
                    if (remaining > 0) {
                        player.takeDamage(remaining);
                    }
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

    // Удаляем мертвых врагов и считаем статистику убийств
    // Проверяем: если после этого хода врагов не останется – показываем лестницу
    int enemiesAlive = 0;
    for (auto it = enemies.begin(); it != enemies.end(); ) {
        if (!it->isAlive()) {
            // Подсчитываем убийства по типам мобов
            if (it->symbol == SYM_ENEMY) killsRat++;
            else if (it->symbol == SYM_BEAR) killsBear++;
            else if (it->symbol == SYM_SNAKE) killsSnake++;
            else if (it->symbol == SYM_GHOST) killsGhost++;
            else if (it->symbol == SYM_CRAB) killsCrab++;
            it = enemies.erase(it);
        } else {
            ++it;
            enemiesAlive++;
        }
    }
    // Если больше нет живых врагов и лестница еще не была раскрыта этим способом, включаем флаг.
    if (enemiesAlive == 0 && !showExitBecauseCleared) {
        showExitBecauseCleared = true;
    }
    // Если враги снова появились (но такого быть не должно), флаг сбрасываем (на всякий случай)
    if (enemiesAlive > 0 && showExitBecauseCleared) {
        showExitBecauseCleared = false;
    }

    // Проверяем, не умер ли игрок
    if (!player.isAlive()) {
        // Завершаем квест и обнуляем состояние
        questActive = false;
        questKills = 0;
        questTarget = 0;
        // Показываем экран смерти вместо мгновенного рестарта
        isDeathScreenActive = true;
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

    // Если яд добил игрока — показываем экран смерти.
    if (!player.isAlive()) {
        questActive = false;
        questKills = 0;
        questTarget = 0;
        isDeathScreenActive = true;
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

    // Если урон от отцепления убил игрока — показываем экран смерти.
    if (!player.isAlive()) {
        questActive = false;
        questKills = 0;
        questTarget = 0;
        isDeathScreenActive = true;
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

            // Подсчитываем собранные предметы для статистики
            if (item.symbol == SYM_ITEM) itemsMedkit++;
            else if (item.symbol == SYM_MAX_HP) itemsMaxHP++;
            else if (item.symbol == SYM_SHIELD) itemsShield++;
            else if (item.symbol == SYM_TRAP) itemsTrap++;
            else if (item.symbol == SYM_QUEST) itemsQuest++;

            // Помечаем, что игрок уже встречал этот тип предмета (для Legend)
            if (item.symbol == SYM_ITEM) seenMedkit = true;
            else if (item.symbol == SYM_MAX_HP) seenMaxHP = true;
            else if (item.symbol == SYM_SHIELD) seenShield = true;
            else if (item.symbol == SYM_TRAP) seenTrap = true;
            else if (item.symbol == SYM_QUEST) seenQuest = true;

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
                // Считаем шаги на уровне (для будущих эффектов, вроде "покажи лестницу первые 3 хода").
                state.stepsOnCurrentLevel++;

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

    // Обновляем положение светлячков и раскрываем вокруг них туман войны.
    // Также проверяем столкновения с мобами (светлячки умирают при столкновении).
    if (state.perkFireflyEnabled && !state.fireflies.empty()) {
        // Радиус факела светлячка (меньше чем у игрока)
        const int FIREFLY_TORCH_RADIUS = 1; // Маленький радиус факела для светлячка (измени здесь для настройки)
        
        // Обрабатываем каждого светлячка
        for (size_t i = state.fireflies.size(); i > 0; --i) {
            size_t idx = i - 1;
            GameState::Firefly& fly = state.fireflies[idx];
            
            // Проверяем столкновение с мобами - если моб наступил на светлячка, он умирает
            bool killed = false;
            for (const auto& enemy : state.enemies) {
                if (enemy.isAlive() && enemy.pos.x == fly.x && enemy.pos.y == fly.y) {
                    killed = true;
                    break;
                }
            }
            if (killed) {
                // Удаляем светлячка
                state.fireflies.erase(state.fireflies.begin() + idx);
                continue;
            }
            
            // Светлячок пытается случайно сдвинуться на одну клетку в одном из 8 направлений.
            const int dirs[8][2] = {
                { 1, 0}, {-1, 0}, {0, 1}, {0,-1},
                { 1, 1}, { 1,-1}, {-1, 1}, {-1,-1}
            };
            for (int attempt = 0; attempt < 8; ++attempt) {
                int dirIdx = std::rand() % 8;
                int nx = fly.x + dirs[dirIdx][0];
                int ny = fly.y + dirs[dirIdx][1];
                if (nx < 0 || nx >= Map::WIDTH || ny < 0 || ny >= Map::HEIGHT) continue;
                // Летаем только по проходимым клеткам-полу.
                if (!state.map.isWalkable(nx, ny)) continue;
                // Не садимся на игрока.
                if (nx == state.player.pos.x && ny == state.player.pos.y) continue;
                // Не садимся на других светлячков
                bool occupiedByFirefly = false;
                for (const auto& other : state.fireflies) {
                    if (other.x == nx && other.y == ny) {
                        occupiedByFirefly = true;
                        break;
                    }
                }
                if (occupiedByFirefly) continue;
                fly.x = nx;
                fly.y = ny;
                break;
            }
            // Каждый ход светлячок открывает туман войны с радиусом факела (как у игрока, но меньше)
            // Используем revealCircle только для отметки как исследованных (FOV будет добавлен в main.cpp через addFOV)
            state.map.revealCircle(fly.x, fly.y, FIREFLY_TORCH_RADIUS);
        }
    }

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
        // Обычный FOV с учетом стен (уменьшенный радиус относительно визуального факела)
        // <<< ДЛЯ ИЗМЕНЕНИЯ РАДИУСА FOV ОТНОСИТЕЛЬНО ФАКЕЛА: измени множитель здесь >>>
        const float FOV_RADIUS_MULTIPLIER = 0.7f; // FOV будет 70% от визуального радиуса факела
        int fovRadius = static_cast<int>(state.torchRadius * FOV_RADIUS_MULTIPLIER);
        if (fovRadius < 1) fovRadius = 1; // Минимум 1
        state.map.computeFOV(state.player.pos.x, state.player.pos.y, fovRadius, true);
        
        // Добавляем FOV от светлячков (если они есть)
        if (state.perkFireflyEnabled && !state.fireflies.empty()) {
            const int FIREFLY_TORCH_RADIUS = 1; // Радиус факела светлячка (измени здесь для настройки)
            for (const auto& firefly : state.fireflies) {
                state.map.addFOV(firefly.x, firefly.y, FIREFLY_TORCH_RADIUS, true);
            }
        }
    }
}

// Генерация нового уровня
void GameState::generateNewLevel()
{
    // Сохраняем выживших светлячков перед очисткой карты
    std::vector<Firefly> survivingFireflies = fireflies;
    
    // Очищаем карту и врагов
    map.items.clear();
    enemies.clear();
    
    // Генерируем новую карту (передаем уровень для контроля спавна предметов на первом уровне)
    map.generate(level);
    
    // Счётчик шагов на уровне и временные эффекты "только на этот этаж"
    stepsOnCurrentLevel = 0;
    showExitBecauseCleared = false;
    // Активируем ядовитых медведей, если перк был выбран "на следующий уровень".
    perkBearPoisonActiveThisLevel = perkBearPoisonNextLevel;
    perkBearPoisonNextLevel = false;

    // На каждом новом уровне пересчитываем радиус факела:
    // берём базовое значение (чуть больше чем у светлячка) и добавляем временный сдвиг, если он есть.
    // <<< ДЛЯ ИЗМЕНЕНИЯ НАЧАЛЬНОГО РАДИУСА ФАКЕЛА: измени здесь (светлячок = 1) >>>
    torchRadius = 3; // Уменьшен с 8 до 3 (чуть больше чем у светлячка = 1)
    if (perkTorchRadiusDeltaNextLevel != 0) {
        torchRadius += perkTorchRadiusDeltaNextLevel;
        if (torchRadius < 2) {
            torchRadius = 2; // Не даём факелу стать совсем маленьким.
        }
        perkTorchRadiusDeltaNextLevel = 0; // Сдвиг отработал только для этого уровня.
    }

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

    // Если перк светлячка активирован — создаём новые светлячки (накапливаются при каждом выборе перка).
    // На первом уровне создаём одного светлячка, на последующих добавляем ещё одного при выборе перка.
    // Здесь мы создаём светлячков только при генерации нового уровня (они уже были добавлены при выборе перка).
    // Но нужно создать первого светлячка, если перк был выбран, но светлячков ещё нет.
    if (perkFireflyEnabled && fireflies.empty()) {
        // Пробуем найти проходимую клетку пола для первого светлячка.
        for (int attempt = 0; attempt < 200; ++attempt) {
            int fx = std::rand() % Map::WIDTH;
            int fy = std::rand() % Map::HEIGHT;
            if (map.getCell(fx, fy) == SYM_FLOOR &&
                !(fx == player.pos.x && fy == player.pos.y) &&
                !map.isExit(fx, fy)) {
                fireflies.push_back(GameState::Firefly(fx, fy));
                // Сразу открываем туман войны вокруг светлячка с радиусом факела
                const int FIREFLY_TORCH_RADIUS = 2;
                map.computeFOV(fx, fy, FIREFLY_TORCH_RADIUS, true);
                break;
            }
        }
    }
    
    // На первом уровне выбираем один случайный тип врага и один случайный тип "сложного" предмета.
    int enemyChoiceLevel1 = -1; // 0 - Rat, 1 - Bear, 2 - Snake, 3 - Ghost, 4 - Crab
    int itemChoiceLevel1 = -1;  // 0 - Trap, 1 - Shield, 2 - Quest
    if (level == 1) {
        enemyChoiceLevel1 = std::rand() % 5;
        itemChoiceLevel1 = std::rand() % 3;
        // Разблокируем выбранные моб и предмет на первом уровне
        if (enemyChoiceLevel1 == 0) unlockedRat = true;
        else if (enemyChoiceLevel1 == 1) unlockedBear = true;
        else if (enemyChoiceLevel1 == 2) unlockedSnake = true;
        else if (enemyChoiceLevel1 == 3) unlockedGhost = true;
        else if (enemyChoiceLevel1 == 4) unlockedCrab = true;
        
        if (itemChoiceLevel1 == 0) unlockedTrap = true;
        else if (itemChoiceLevel1 == 1) unlockedShield = true;
        else if (itemChoiceLevel1 == 2) unlockedQuest = true;
        
        // На первом уровне также разблокируем базовые предметы (Medkit или MaxHP - один случайный)
        // Это уже обрабатывается в Map::generate(), но нужно разблокировать оба типа для будущих уровней
        unlockedMedkit = true; // Базовые предметы всегда доступны после первого уровня
        unlockedMaxHP = true;
    }

    // Создаем несколько крыс на случайных позициях
    // Базовое количество + бонус от перков (накопительный, "навсегда").
    // Спавним только если разблокированы
    int ratsToSpawn = 0;
    if (unlockedRat) {
        if (level == 1) {
            // На первом уровне случайное количество от 3 до 7
            if (enemyChoiceLevel1 == 0) {
                ratsToSpawn = 3 + (std::rand() % 5); // 3-7 крыс
            }
        } else {
            ratsToSpawn = 5 + level + perkBonusRats;
        }
    }
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
    // Спавним только если разблокированы
    int bearsToSpawn = 0;
    if (unlockedBear) {
        if (level == 1) {
            // На первом уровне случайное количество от 1 до 3
            if (enemyChoiceLevel1 == 1) {
                bearsToSpawn = 1 + (std::rand() % 3); // 1-3 медведя
            }
        } else {
            bearsToSpawn = 2 + level / 2; // Медведей меньше, чем крыс
        }
    }
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

    // Создаем змей на случайных позициях.
    // Базовое количество + временный бонус только на текущий уровень.
    // Спавним только если разблокированы
    int snakesToSpawn = 0;
    if (unlockedSnake) {
        if (level == 1) {
            // На первом уровне случайное количество от 2 до 4
            if (enemyChoiceLevel1 == 2) {
                snakesToSpawn = 2 + (std::rand() % 3); // 2-4 змеи
            }
        } else {
            snakesToSpawn = 3 + level / 2 + perkSnakesNextLevel;
        }
    }
    // Эффект "+змеи" был только на один уровень — обнуляем.
    perkSnakesNextLevel = 0;
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
    // Спавним только если разблокированы
    int ghostsToSpawn = 0;
    if (unlockedGhost) {
        if (level == 1) {
            // На первом уровне случайное количество от 1 до 2
            if (enemyChoiceLevel1 == 3) {
                ghostsToSpawn = 1 + (std::rand() % 2); // 1-2 призрака
            }
        } else {
            ghostsToSpawn = 1 + level / 2; // Немного, но они опасные
        }
    }
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
    // Спавним только если разблокированы
    int crabsToSpawn = 0;
    if (unlockedCrab) {
        if (level == 1) {
            // На первом уровне случайное количество от 1 до 3
            if (enemyChoiceLevel1 == 4) {
                crabsToSpawn = 1 + (std::rand() % 3); // 1-3 краба
            }
        } else {
            crabsToSpawn = 2 + level / 2;
        }
    }
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

    // Спавним призрачные предметы '.' в случайных местах (не больше 5 за карту).
    // Спавним только если разблокированы
    if (unlockedTrap && (level != 1 || itemChoiceLevel1 == 0)) {
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
    }

    // Спавним предмет-щиты 'O' (до 3 на карту + бонус за перки)
    // Спавним только если разблокированы
    if (unlockedShield && (level != 1 || itemChoiceLevel1 == 1)) {
        const int shieldsToSpawn = std::max(0, 3 + perkBonusShields);
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
    }

    // Спавним один квестовый предмет '?' (запускает убийство монстров)
    // Спавним только если разблокированы
    if (unlockedQuest && (level != 1 || itemChoiceLevel1 == 2)) {
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
    }

    // Дополнительные аптечки за постоянный перк.
    for (int extra = 0; extra < perkBonusHeals; ++extra) {
        for (int attempt = 0; attempt < 200; ++attempt) {
            int hx = std::rand() % Map::WIDTH;
            int hy = std::rand() % Map::HEIGHT;

            if (map.getCell(hx, hy) == SYM_FLOOR &&
                !(hx == player.pos.x && hy == player.pos.y) &&
                !map.isExit(hx, hy) &&
                map.getItemAt(hx, hy) == nullptr) {
                // Аптечка на 5 HP, как базовые.
                map.addHealItem(hx, hy, 5);
                break;
            }
        }
    }

    // Дополнительные MaxHP‑предметы только на этот уровень.
    for (int extra = 0; extra < perkExtraMaxHpItemsNextLevel; ++extra) {
        for (int attempt = 0; attempt < 200; ++attempt) {
            int mx = std::rand() % Map::WIDTH;
            int my = std::rand() % Map::HEIGHT;

            if (map.getCell(mx, my) == SYM_FLOOR &&
                !(mx == player.pos.x && my == player.pos.y) &&
                !map.isExit(mx, my) &&
                map.getItemAt(mx, my) == nullptr) {
                int bonus = 1 + (std::rand() % 5); // как в Map::generate
                map.addMaxHealthItem(mx, my, bonus);
                break;
            }
        }
    }
    // Этот бонус действует только на один этаж.
    perkExtraMaxHpItemsNextLevel = 0;

    // Инициализируем FOV
    map.computeFOV(player.pos.x, player.pos.y, torchRadius, true);
}

// Проверка перехода на следующий уровень
bool GameState::checkExit()
{
    if (map.isExit(player.pos.x, player.pos.y)) {
        // Не сразу переходим на следующий уровень, а показываем экран выбора перка.
        // Сам переход произойдет после того, как игрок выберет 1, 2 или 3.
        isPerkChoiceActive = true;
        // Генерируем случайные варианты модификаторов один раз при активации экрана
        perkChoiceVariant1 = std::rand() % 6;
        perkChoiceVariant2 = std::rand() % 6;
        perkChoiceVariant3 = std::rand() % 6;
        return true;
    }
    return false;
}

// Применить выбранный перк (1, 2 или 3) и перейти на следующий уровень.
void GameState::applyLevelChoice(int choiceIndex)
{
    // Защита от некорректных значений
    if (choiceIndex < 1 || choiceIndex > 3) {
        return;
    }

    if (choiceIndex == 1) {
        // 1) +5 rat, +2 аптечки, "светлячок" (ИИ‑факел по карте).
        // Добавляем постоянные бонусы к количеству крыс и аптечек.
        perkBonusRats += 5;
        perkBonusHeals += 2;
        // Флаг светлячка — пока только сохраняем, логику движения можно будет
        // аккуратно добавить отдельно, чтобы не усложнять этот шаг.
        perkFireflyEnabled = true;
        
        // Разблокируем крыс и аптечки
        unlockedRat = true;
        unlockedMedkit = true;
        
        // Добавляем нового светлячка (накапливаются)
        // Пробуем найти проходимую клетку пола для нового светлячка.
        for (int attempt = 0; attempt < 200; ++attempt) {
            int fx = std::rand() % Map::WIDTH;
            int fy = std::rand() % Map::HEIGHT;
            if (map.getCell(fx, fy) == SYM_FLOOR &&
                !(fx == player.pos.x && fy == player.pos.y) &&
                !map.isExit(fx, fy)) {
                // Проверяем, что на этой позиции нет других светлячков
                bool occupied = false;
                for (const auto& existing : fireflies) {
                    if (existing.x == fx && existing.y == fy) {
                        occupied = true;
                        break;
                    }
                }
                if (!occupied) {
                    fireflies.push_back(GameState::Firefly(fx, fy));
                    break;
                }
            }
        }
        
        // Сохраняем перк в список для экрана смерти (каждый параметр на отдельной строке)
        collectedPerks.push_back("+5 rats");
        collectedPerks.push_back("+2 medkits");
        collectedPerks.push_back("Firefly reveals fog");
    } else if (choiceIndex == 2) {
        // 2) Случайный выбор: каждый эффект либо только на следующий этаж, либо навсегда.
        // Медведь с мутацией отравления
        if (std::rand() % 2 == 0) {
            perkBearPoisonNextLevel = true;  // Только на следующий уровень
        } else {
            // Навсегда: медведи всегда ядовитые (нужно добавить флаг для постоянного эффекта)
            // Пока используем временный флаг, но он будет применяться каждый уровень
            perkBearPoisonNextLevel = true;  // Временно, но можно сделать постоянным
        }
        // Больше предметов щитов
        if (std::rand() % 2 == 0) {
            perkBonusShields += 1;  // Навсегда
        } else {
            // Только на следующий уровень (временный бонус)
            perkBonusShields += 1;  // Временно применяем как постоянный
        }
        // Показывать лестницу первые 3 шага
        if (std::rand() % 2 == 0) {
            perkShowExitFirst3Steps = true;  // Навсегда
        } else {
            // Только на следующий уровень (можно добавить временный флаг)
            perkShowExitFirst3Steps = true;  // Временно
        }
        
        // Разблокируем медведей и щиты
        unlockedBear = true;
        unlockedShield = true;
        
        // Сохраняем перк в список для экрана смерти (каждый параметр на отдельной строке)
        collectedPerks.push_back("Bear with poison");
        collectedPerks.push_back("More shields");
        collectedPerks.push_back("Show exit hint");
    } else if (choiceIndex == 3) {
        // 3) +2 snake, выше шанс spawna maxHP‑предметов, радиус факела сильно сужен
        // ТОЛЬКО на следующий уровень.
        perkSnakesNextLevel += 2;           // Больше змей только на следующем этаже.
        perkExtraMaxHpItemsNextLevel += 1;  // +1 доп. предмет Max HP на следующем этаже.
        perkTorchRadiusDeltaNextLevel -= 3; // Сужаем радиус факела на следующем этаже.
        
        // Разблокируем змей и MaxHP предметы
        unlockedSnake = true;
        unlockedMaxHP = true;
        
        // Сохраняем перк в список для экрана смерти (каждый параметр на отдельной строке)
        collectedPerks.push_back("+2 snakes");
        collectedPerks.push_back("More MaxHP items");
        collectedPerks.push_back("Smaller torch");
    }

    // Экран выбора закрываем и реально переходим на следующий уровень.
    isPerkChoiceActive = false;
    level++;
    generateNewLevel();
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
    shieldWhiteSegments = 0;
    visionTurns = 0;
    
    // СБРАСЫВАЕМ ВСЕ ПЕРКИ И УЛУЧШЕНИЯ
    perkBonusRats = 0;
    perkBonusHeals = 0;
    perkBonusShields = 0;
    perkFireflyEnabled = false;
    fireflies.clear();
    perkShowExitFirst3Steps = false;
    perkBearPoisonNextLevel = false;
    perkBearPoisonActiveThisLevel = false;
    perkSnakesNextLevel = 0;
    perkExtraMaxHpItemsNextLevel = 0;
    perkTorchRadiusDeltaNextLevel = 0;
    torchRadius = 3; // Возвращаем базовый радиус факела (чуть больше чем у светлячка = 1)
    stepsOnCurrentLevel = 0;
    showExitBecauseCleared = false;
    
    // Сбрасываем статистику
    killsRat = 0;
    killsBear = 0;
    killsSnake = 0;
    killsGhost = 0;
    killsCrab = 0;
    itemsMedkit = 0;
    itemsMaxHP = 0;
    itemsShield = 0;
    itemsTrap = 0;
    itemsQuest = 0;
    collectedPerks.clear();
    
    // Сбрасываем флаги разблокировки
    unlockedRat = false;
    unlockedBear = false;
    unlockedSnake = false;
    unlockedGhost = false;
    unlockedCrab = false;
    unlockedMedkit = false;
    unlockedMaxHP = false;
    unlockedShield = false;
    unlockedTrap = false;
    unlockedQuest = false;

    // Сбрасываем изученных в легенде (правый интерфейс)
    seenRat = false;
    seenBear = false;
    seenSnake = false;
    seenGhost = false;
    seenCrab = false;
    seenMedkit = false;
    seenMaxHP = false;
    seenShield = false;
    seenTrap = false;
    seenQuest = false;
    
    // Закрываем экран смерти
    isDeathScreenActive = false;
    
    // Генерируем новый уровень (это также очистит карту и врагов)
    generateNewLevel();
}

