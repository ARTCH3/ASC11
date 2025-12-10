#include "Game.h"
#include "Graphics.h"

// Главная функция игры.
// Создаем состояние игры и объект для рисования,
// затем запускаем основной игровой цикл.
int main()
{
    // Размеры экрана
    const int screenWidth = Map::WIDTH;
    const int screenHeight = Map::HEIGHT + 9; // Дополнительные строки для цветного HUD

    // Создаем состояние игры
    GameState game;

    // Создаем объект для рисования
    Graphics graphics(screenWidth, screenHeight);

    // Инициализируем FOV
    game.map.computeFOV(game.player.pos.x, game.player.pos.y, game.torchRadius, true);

    // Основной игровой цикл
    while (game.isRunning) {
        // Очищаем экран
        graphics.clearScreen();

        // Рисуем карту (с учетом FOV и факела)
        graphics.drawMap(game.map, game.player.pos.x, game.player.pos.y, game.torchRadius);

        // Рисуем врагов (только если они видны)
        for (const auto& enemy : game.enemies) {
            if (enemy.isAlive() && game.map.isVisible(enemy.pos.x, enemy.pos.y)) {
                graphics.drawEntity(enemy);
            }
        }

        // Рисуем предметы (только если они видны)
        for (const auto& item : game.map.items) {
            if (game.map.isVisible(item.pos.x, item.pos.y)) {
                graphics.drawItem(item.pos.x, item.pos.y);
            }
        }

        // Рисуем выход (только если виден через FOV)
        if (game.map.exitPos.x >= 0 && game.map.exitPos.y >= 0 &&
            game.map.isVisible(game.map.exitPos.x, game.map.exitPos.y)) {
            Entity exitEntity(game.map.exitPos.x, game.map.exitPos.y, SYM_EXIT, TCOD_ColorRGB{255, 255, 100});
            graphics.drawEntity(exitEntity);
        }

        // Рисуем игрока (с динамическим цветом в зависимости от здоровья)
        graphics.drawPlayer(game.player);

        // Рисуем UI
        graphics.drawUI(game.player, game.enemies, game.level, game.map);

        // Обновляем экран
        graphics.refreshScreen();

        // Обрабатываем ввод
        int key = 0;
        if (graphics.getInput(key)) {
            // Проверяем F11 для полноэкранного режима
            if (key == TCODK_F11) {
                graphics.toggleFullscreen();
            } else {
                handleInput(game, key);
            }
        }
    }

    return 0;
}

