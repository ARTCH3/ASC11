#include "Game.h"
#include "Graphics.h"

// Главная функция игры.
// Создаем состояние игры и объект для рисования,
// затем запускаем основной игровой цикл.
int main()
{
    // Размеры окна: карта в центре (80x36).
    // <<< Можешь менять ширину боковых панелей и высоту нижней панели вот здесь >>>
    const int leftPanelWidth = 15;   // ширина левого UI-слоя
    const int rightPanelWidth = 15; // ширина правого UI-слоя
    const int topPanelHeight = 1;   // верхняя панель (HP‑линия)
    const int bottomPanelHeight = 6; // нижняя панель (управление + Floor + центр)
    const int screenWidth = leftPanelWidth + Map::WIDTH + rightPanelWidth;
    const int screenHeight = topPanelHeight + Map::HEIGHT + bottomPanelHeight;

    // Создаем состояние игры
    GameState game;

    // Создаем объект для рисования и передаем размеры панелей.
    Graphics graphics(screenWidth,
                      screenHeight,
                      leftPanelWidth,
                      rightPanelWidth,
                      topPanelHeight,
                      bottomPanelHeight);

    // Инициализируем FOV
    game.map.computeFOV(game.player.pos.x, game.player.pos.y, game.torchRadius, true);

    // Основной игровой цикл
    while (game.isRunning) {
        // Если активен экран смерти — рисуем его поверх игры и обрабатываем ввод
        if (game.isDeathScreenActive) {
            // Рисуем обычный игровой экран (карту, UI панели и т.д.)
            graphics.clearScreen();
            
            // Пересчитываем FOV для отображения карты
            const float FOV_RADIUS_MULTIPLIER = 0.7f; // FOV будет 70% от визуального радиуса факела
            int fovRadius = static_cast<int>(game.torchRadius * FOV_RADIUS_MULTIPLIER);
            if (fovRadius < 1) fovRadius = 1;
            game.map.computeFOV(game.player.pos.x, game.player.pos.y, fovRadius, true);
            
            // Рисуем карту
            bool showExitHint = false;
            std::vector<std::pair<int, int>> fireflyPositions;
            graphics.drawMap(game.map, game.player.pos.x, game.player.pos.y, game.torchRadius, showExitHint, fireflyPositions);
            
            // Рисуем UI панели
            graphics.drawUI(game.player,
                           game.enemies,
                           game.level,
                           game.map,
                           game.isPlayerPoisoned,
                           game.isPlayerGhostCursed,
                           game.shieldTurns,
                           game.shieldWhiteSegments,
                           game.questActive,
                           game.questKills,
                           game.questTarget,
                           game.seenRat,
                           game.seenBear,
                           game.seenSnake,
                           game.seenGhost,
                           game.seenCrab,
                           game.seenMedkit,
                           game.seenMaxHP,
                           game.seenShield,
                           game.seenTrap,
                           game.seenQuest);
            
            // Рисуем экран смерти поверх всего
            graphics.drawDeathScreen(game.level,
                                     game.killsRat, game.killsBear, game.killsSnake, game.killsGhost, game.killsCrab,
                                     game.itemsMedkit, game.itemsMaxHP, game.itemsShield, game.itemsTrap, game.itemsQuest,
                                     game.collectedPerks);
            graphics.refreshScreen();
            
            // Обрабатываем ввод
            int key = 0;
            if (graphics.getInput(key)) {
                // Проверяем и F (строчную), и ESC (для совместимости)
                if (key == 'f' || key == 'F' || key == TCODK_ESCAPE || key == 27) {
                    game.restartGame();
                }
            }
            continue; // Пропускаем остальной цикл
        }
        
        // Очищаем экран
        graphics.clearScreen();
        
        // Пересчитываем FOV каждый кадр с учетом текущего радиуса факела игрока
        // <<< ДЛЯ ИЗМЕНЕНИЯ РАДИУСА FOV ОТНОСИТЕЛЬНО ФАКЕЛА: измени множитель здесь (меньше = меньше радиус FOV) >>>
        const float FOV_RADIUS_MULTIPLIER = 0.7f; // FOV будет 70% от визуального радиуса факела
        int fovRadius = static_cast<int>(game.torchRadius * FOV_RADIUS_MULTIPLIER);
        if (fovRadius < 1) fovRadius = 1; // Минимум 1
        game.map.computeFOV(game.player.pos.x, game.player.pos.y, fovRadius, true);
        
        // Также добавляем видимость от каждого светлячка (используем addFOV, чтобы не перезаписать FOV игрока)
        if (game.perkFireflyEnabled && !game.fireflies.empty()) {
            const int FIREFLY_TORCH_RADIUS = 1; // Радиус факела светлячка (измени здесь для настройки)
            for (const auto& firefly : game.fireflies) {
                // Используем addFOV вместо computeFOV, чтобы не перезаписать FOV игрока
                game.map.addFOV(firefly.x, firefly.y, FIREFLY_TORCH_RADIUS, true);
            }
        }

        // Рисуем карту (с учетом FOV и факела)
        bool showExitHint = (game.perkShowExitFirst3Steps && game.stepsOnCurrentLevel <= 3) || game.showExitBecauseCleared;
        // Собираем позиции светлячков для передачи в drawMap
        std::vector<std::pair<int, int>> fireflyPositions;
        for (const auto& firefly : game.fireflies) {
            fireflyPositions.push_back({firefly.x, firefly.y});
        }
        graphics.drawMap(game.map, game.player.pos.x, game.player.pos.y, game.torchRadius, showExitHint, fireflyPositions);

        // Рисуем врагов (только если они видны)
        for (const auto& enemy : game.enemies) {
            if (enemy.isAlive() && game.map.isVisible(enemy.pos.x, enemy.pos.y)) {
                graphics.drawEntity(enemy);
            }
        }

        // Рисуем предметы (только если они видны)
        for (const auto& item : game.map.items) {
            if (game.map.isVisible(item.pos.x, item.pos.y)) {
                graphics.drawItem(item);
            }
        }

        // Рисуем выход (только если виден через FOV)
        if (game.map.exitPos.x >= 0 && game.map.exitPos.y >= 0 &&
            game.map.isVisible(game.map.exitPos.x, game.map.exitPos.y)) {
            Entity exitEntity(game.map.exitPos.x, game.map.exitPos.y, SYM_EXIT, TCOD_ColorRGB{255, 255, 100});
            graphics.drawEntity(exitEntity);
        }

        // Рисуем игрока (цвет зависит от здоровья, эффектов яда и щита).
        graphics.drawPlayer(game.player,
                            game.isPlayerPoisoned,
                            game.shieldTurns > 0);

        // Рисуем UI.
        // При действии яда полоска HP меняет цвет на "ядовитый" зелёный,
        // а при действии эффекта призрака все квадраты становятся серыми,
        // и вместо цифр отображаются вопросительные знаки.
        graphics.drawUI(game.player,
                        game.enemies,
                        game.level,
                        game.map,
                        game.isPlayerPoisoned,
                        game.isPlayerGhostCursed,
                        game.shieldTurns,
                        game.shieldWhiteSegments,
                        game.questActive,
                        game.questKills,
                        game.questTarget,
                        game.seenRat,
                        game.seenBear,
                        game.seenSnake,
                        game.seenGhost,
                        game.seenCrab,
                        game.seenMedkit,
                        game.seenMaxHP,
                        game.seenShield,
                        game.seenTrap,
                        game.seenQuest);

        // Проверяем наведение мыши и отображаем названия
        int mouseMapX, mouseMapY;
        if (graphics.getMousePosition(mouseMapX, mouseMapY)) {
            // Проверяем мобов
            for (const auto& enemy : game.enemies) {
                if (enemy.isAlive() && enemy.pos.x == mouseMapX && enemy.pos.y == mouseMapY &&
                    game.map.isVisible(enemy.pos.x, enemy.pos.y)) {
                    std::string name;
                    if (enemy.symbol == SYM_BEAR) name = "Bear";
                    else if (enemy.symbol == SYM_SNAKE) name = "Snake";
                    else if (enemy.symbol == SYM_GHOST) name = "Ghost";
                    else if (enemy.symbol == SYM_CRAB) name = "Crab";
                    else name = "Rat";
                    // Добавляем HP к имени: имя 1/3
                    char buffer[32];
                    snprintf(buffer, sizeof(buffer), "%d/%d", enemy.health, enemy.maxHealth);
                    std::string nameWithHP = name + " " + buffer;
                    graphics.drawHoverName(mouseMapX, mouseMapY, nameWithHP, tcod::ColorRGB{enemy.color.r, enemy.color.g, enemy.color.b});
                    break;
                }
            }
            // Проверяем предметы
            for (const auto& item : game.map.items) {
                if (item.pos.x == mouseMapX && item.pos.y == mouseMapY &&
                    game.map.isVisible(item.pos.x, item.pos.y)) {
                    std::string name;
                    tcod::ColorRGB color{200, 200, 200};
                    if (item.symbol == SYM_ITEM) { name = "Medkit"; color = tcod::ColorRGB{255, 255, 0}; }
                    else if (item.symbol == SYM_MAX_HP) { name = "Max HP"; color = tcod::ColorRGB{0, 204, 0}; }
                    else if (item.symbol == SYM_SHIELD) { name = "Shield"; color = tcod::ColorRGB{255, 255, 255}; }
                    else if (item.symbol == SYM_TRAP) { name = "Trap"; color = tcod::ColorRGB{40, 40, 40}; }
                    if (!name.empty()) {
                        graphics.drawHoverName(mouseMapX, mouseMapY, name, color);
                        break;
                    }
                }
            }
            // Проверяем лестницу
            if (game.map.isExit(mouseMapX, mouseMapY) &&
                game.map.isVisible(mouseMapX, mouseMapY)) {
                graphics.drawHoverName(mouseMapX, mouseMapY, "Stair", tcod::ColorRGB{200, 200, 200});
            }
        }

        // Если игрок стоит на лестнице и уже вошёл в "экран выбора" — рисуем поверх центральной части
        // специальный чёрный оверлей с тремя вариантами 1/2/3.
        if (game.isPerkChoiceActive) {
            graphics.drawLevelChoiceMenu(game.perkChoiceVariant1, game.perkChoiceVariant2, game.perkChoiceVariant3);
        }

        // Обновляем экран
        graphics.refreshScreen();

        // Обрабатываем ввод
        int key = 0;
        if (graphics.getInput(key)) {
            // Если активен экран выбора перка – обрабатываем только клавиши 1/2/3.
            if (game.isPerkChoiceActive) {
                if (key == '1') {
                    game.applyLevelChoice(1);
                } else if (key == '2') {
                    game.applyLevelChoice(2);
                } else if (key == '3') {
                    game.applyLevelChoice(3);
                }
            } else {
                // Обычный режим игры
                if (key == TCODK_F11) {
                    graphics.toggleFullscreen();
                } else {
                    handleInput(game, key);
                }
            }
        }
    }

    return 0;
}

#ifdef _WIN32
#include <windows.h>
// Обертка WinMain для сборки без консольного окна (subsystem:windows)
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;
    return main();
}
#endif

