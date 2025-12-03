#include "Graphics.h"

#include "Map.h"
#include "Entity.h"
#include <cmath>
#include <SDL2/SDL.h>
#include <string>
#include <cstring>
#include <SDL2/SDL.h>

Graphics::Graphics(int width, int height)
    : screenWidth(width),
      screenHeight(height),
      colorPlayer{100, 200, 255},
      colorWall{0, 0, 100},        // #000064
      colorFloor{50, 50, 150},     // #323296
      colorEnemy{255, 50, 50},
      colorItem{255, 255, 100},
      colorDark{20, 20, 20},
      colorExplored{60, 60, 60},
      torchNoise(1),               // 1D noise для эффекта факела
      torchX(0.0f)
{
    // Проверяем размеры
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid console dimensions");
    }
    
    // Создаем консоль
    console = tcod::Console(screenWidth, screenHeight);
    
    // Создаем контекст окна (как в samples_cpp.cpp)
    TCOD_ContextParams params{};
    params.tcod_version = TCOD_COMPILEDVERSION;
    params.console = console.get();
    params.window_title = "ASC11 - Roguelike";
    params.vsync = true;
    params.argc = 0;
    params.argv = nullptr;
    // Используем SDL2 рендерер по умолчанию (как в samples)
    params.renderer_type = TCOD_RENDERER_SDL2;

    context = tcod::new_context(params);
    
    if (!context) {
        throw std::runtime_error("Failed to create context");
    }
}

Graphics::~Graphics() = default;

void Graphics::drawMap(const Map& map, int playerX, int playerY, int torchRadius)
{
    // Обновляем эффект факела с пульсацией
    // Используем время для плавной пульсации (даже когда игрок стоит)
    static float time = 0.0f;
    time += 0.03f; // Медленное увеличение времени для плавной пульсации
    
    torchX += 0.1f; // Медленнее обновляем для более плавного эффекта
    float dx = 0.0f, dy = 0.0f, di = 0.0f;
    
    // Очень слабое случайное смещение позиции факела для эффекта мерцания
    float tdx = torchX + 20.0f;
    dx = torchNoise.get(&tdx) * 0.5f; // Еще больше уменьшили амплитуду
    tdx += 30.0f;
    dy = torchNoise.get(&tdx) * 0.5f;
    
    // Пульсация интенсивности света (даже когда игрок стоит)
    // Используем синус для плавной пульсации
    float pulse = 0.25f + 0.1f * std::sin(time * 1.5f); // Мягкая пульсация от 0.15 до 0.35
    di = pulse * (0.5f + 0.5f * torchNoise.get(&torchX)); // Комбинируем с шумом для естественности
    
    const float TORCH_RADIUS = static_cast<float>(torchRadius);
    const float SQUARED_TORCH_RADIUS = TORCH_RADIUS * TORCH_RADIUS;
    
    // Цвета для стен и пола
    const tcod::ColorRGB darkWall{0, 0, 100};      // Темные стены
    const tcod::ColorRGB lightWall{130, 110, 50};  // Светлые стены
    const tcod::ColorRGB darkGround{50, 50, 150};  // Темный пол
    const tcod::ColorRGB lightGround{200, 180, 50}; // Светлый пол
    
    // Отрисовываем карту с учетом FOV и эффекта факела
    for (int y = 0; y < Map::HEIGHT && y < screenHeight; ++y) {
        for (int x = 0; x < Map::WIDTH && x < screenWidth; ++x) {
            // Проверяем границы консоли
            if (!console.in_bounds({x, y})) {
                continue;
            }
            
            const bool isVisible = map.isVisible(x, y);
            const bool isExplored = map.isExplored(x, y);
            const bool isWall = map.isWall(x, y);
            
            if (!isVisible) {
                // Невидимые клетки
                if (isExplored) {
                    // Исследованные, но невидимые - затемненные
                    console.at({x, y}).bg = isWall ? darkWall : darkGround;
                    console.at({x, y}).ch = 0x2588; // Блок
                    console.at({x, y}).fg = isWall ? darkWall : darkGround;
                } else {
                    // Не исследованные - черные
                    console.at({x, y}).bg = colorDark;
                    console.at({x, y}).ch = ' ';
                    console.at({x, y}).fg = colorDark;
                }
            } else {
                // Видимые клетки с эффектом факела
                tcod::ColorRGB base = isWall ? darkWall : darkGround;
                tcod::ColorRGB light = isWall ? lightWall : lightGround;
                
                // Вычисляем расстояние до факела (с учетом смещения)
                const float r = static_cast<float>((x - playerX + dx) * (x - playerX + dx) + 
                                                   (y - playerY + dy) * (y - playerY + dy));
                
                if (r < SQUARED_TORCH_RADIUS) {
                    // Интерполируем цвет в зависимости от расстояния
                    // Делаем более мягкий переход (используем квадратный корень для более плавного затухания)
                    float normalizedDist = r / SQUARED_TORCH_RADIUS;
                    float smoothDist = std::sqrt(normalizedDist); // Более плавное затухание
                    // l = 1.0 в позиции игрока, 0.0 на границе радиуса, с учетом пульсации
                    const float l = std::clamp((1.0f - smoothDist) * (0.7f + di), 0.0f, 1.0f);
                    base = tcod::ColorRGB{TCODColor::lerp(base, light, l)};
                }
                
                // Рисуем цветной блок
                console.at({x, y}).bg = base;
                console.at({x, y}).ch = 0x2588; // Символ блока (█)
                console.at({x, y}).fg = base;
            }
        }
    }
}

void Graphics::drawEntity(const Entity& entity)
{
    // Рисуем сущность только если она видна
    int x = entity.pos.x;
    int y = entity.pos.y;

    if (x >= 0 && x < Map::WIDTH && y >= 0 && y < Map::HEIGHT &&
        x < screenWidth && y < screenHeight &&
        console.in_bounds({x, y})) {
        console.at({x, y}).ch = entity.symbol;
        console.at({x, y}).fg = entity.color;
    }
}

void Graphics::drawItem(int x, int y)
{
    if (x >= 0 && x < Map::WIDTH && y >= 0 && y < Map::HEIGHT &&
        x < screenWidth && y < screenHeight &&
        console.in_bounds({x, y})) {
        console.at({x, y}).ch = SYM_ITEM;
        console.at({x, y}).fg = colorItem;
    }
}

void Graphics::drawPlayer(const Entity& player)
{
    // Рисуем игрока с динамическим цветом в зависимости от здоровья
    int x = player.pos.x;
    int y = player.pos.y;

    if (x >= 0 && x < Map::WIDTH && y >= 0 && y < Map::HEIGHT &&
        x < screenWidth && y < screenHeight &&
        console.in_bounds({x, y})) {
        // Вычисляем цвет в зависимости от здоровья с резкими переходами каждые 20%
        // Избегаем желто-оранжевых оттенков, которые сливаются с факелом
        float healthPercent = static_cast<float>(player.health) / static_cast<float>(player.maxHealth);
        
        tcod::ColorRGB playerColor;
        
        if (healthPercent > 0.8f) {
            // 80-100%: яркий голубой/бирюзовый
            playerColor = tcod::ColorRGB{100, 255, 255};
        } else if (healthPercent > 0.6f) {
            // 60-80%: яркий зеленый
            playerColor = tcod::ColorRGB{50, 255, 100};
        } else if (healthPercent > 0.4f) {
            // 40-60%: желто-зеленый (но не слишком желтый)
            playerColor = tcod::ColorRGB{150, 255, 50};
        } else if (healthPercent > 0.2f) {
            // 20-40%: оранжево-красный
            playerColor = tcod::ColorRGB{255, 150, 0};
        } else {
            // 0-20%: яркий красный
            playerColor = tcod::ColorRGB{255, 0, 0};
        }
        
        console.at({x, y}).ch = player.symbol;
        console.at({x, y}).fg = playerColor;
    }
}

// Функция для конвертации числа в римские цифры
static std::string toRoman(int num)
{
    if (num <= 0) return "I";
    
    std::string result;
    const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char* numerals[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    
    for (int i = 0; i < 13; ++i) {
        while (num >= values[i]) {
            result += numerals[i];
            num -= values[i];
        }
    }
    return result;
}

void Graphics::drawUI(const Entity& player, int level)
{
    // Рисуем UI внизу экрана
    int uiY = Map::HEIGHT + 1;

    // Очищаем строки UI
    for (int y = uiY; y < screenHeight; ++y) {
        for (int x = 0; x < screenWidth; ++x) {
            console.at({x, y}).ch = ' ';
            console.at({x, y}).bg = tcod::ColorRGB{0, 0, 0};
        }
    }

    // Разбиваем текст на несколько строк, чтобы не переносился
    char buffer[256];
    
    // Вычисляем цвет здоровья (от зеленого к красному)
    float healthPercent = static_cast<float>(player.health) / static_cast<float>(player.maxHealth);
    int r = static_cast<int>(255 * (1.0f - healthPercent)); // Красный увеличивается
    int g = static_cast<int>(255 * healthPercent);           // Зеленый уменьшается
    int b = 0;
    tcod::ColorRGB healthColor{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
    
    // Первая строка: позиция, здоровье (с цветом) и уровень
    // Используем простой подход - сначала выводим всю строку белым, потом перезаписываем здоровье цветом
    snprintf(buffer, sizeof(buffer),
             "Position: (%d, %d) | Health: %d/%d | Ascension: %s",
             player.pos.x, player.pos.y, player.health, player.maxHealth, toRoman(level).c_str());
    
    // Проверяем границы перед выводом (используем screenWidth и screenHeight)
    if (uiY >= 0 && uiY < screenHeight) {
        // Выводим всю строку белым (без Unicode для безопасности)
        try {
            if (screenWidth > 0) {
                tcod::print(console, {0, uiY}, buffer, tcod::ColorRGB{255, 255, 255}, std::nullopt);
                
                // Теперь перезаписываем только число здоровья цветом
                // Находим позицию числа здоровья в строке
                int healthStartX = 0;
                snprintf(buffer, sizeof(buffer), "Position: (%d, %d) | Health: ", player.pos.x, player.pos.y);
                healthStartX = static_cast<int>(strlen(buffer));
                
                // Выводим только число здоровья цветом
                snprintf(buffer, sizeof(buffer), "%d", player.health);
                if (healthStartX >= 0 && healthStartX < screenWidth) {
                    tcod::print(console, {healthStartX, uiY}, buffer, healthColor, std::nullopt);
                }
            }
        } catch (const std::exception&) {
            // Игнорируем ошибки вывода UI
        }
    }
    
    // Вторая строка: управление
    if (uiY + 1 >= 0 && uiY + 1 < screenHeight) {
        try {
            if (screenWidth > 0) {
                snprintf(buffer, sizeof(buffer),
                         "Move: [WASD] | Diag: [QEZC] | Fullscreen: [F11] | Quit: [ESC]");
                tcod::print(console, {0, uiY + 1}, buffer, tcod::ColorRGB{200, 200, 200}, std::nullopt);
            }
        } catch (const std::exception&) {
            // Игнорируем ошибки вывода UI
        }
    }

    // Третья строка: легенда (обновленная)
    // Выводим текст по частям, чтобы правильно отобразить Unicode символы
    if (uiY + 2 >= 0 && uiY + 2 < screenHeight) {
        try {
            if (screenWidth > 0) {
                // Выводим текст с символами из enum GameSymbols
                // Player: символ игрока
                snprintf(buffer, sizeof(buffer), "Player: %c | Enemy: %c (rat) | Item: ", 
                         static_cast<char>(SYM_PLAYER), static_cast<char>(SYM_ENEMY));
                int xPos = static_cast<int>(strlen(buffer));
                tcod::print(console, {0, uiY + 2}, buffer, tcod::ColorRGB{180, 180, 180}, std::nullopt);
                
                // Выводим символ предмета (♥) напрямую
                if (xPos < screenWidth && console.in_bounds({xPos, uiY + 2})) {
                    console.at({xPos, uiY + 2}).ch = SYM_ITEM;
                    console.at({xPos, uiY + 2}).fg = tcod::ColorRGB{180, 180, 180};
                    xPos++;
                }
                
                // Выводим продолжение текста
                snprintf(buffer, sizeof(buffer), " (Cure)");
                if (xPos < screenWidth) {
                    tcod::print(console, {xPos, uiY + 2}, buffer, tcod::ColorRGB{180, 180, 180}, std::nullopt);
                }
            }
        } catch (const std::exception&) {
            // Игнорируем ошибки вывода UI
        }
    }
    
    // Четвертая строка: Exit с символом
    if (uiY + 3 >= 0 && uiY + 3 < screenHeight) {
        try {
            if (screenWidth > 0) {
                // Выводим текст "Exit: ^" с символом в строке
                snprintf(buffer, sizeof(buffer), "Exit: %c", SYM_EXIT);
                tcod::print(console, {0, uiY + 3}, buffer, tcod::ColorRGB{180, 180, 180}, std::nullopt);
            }
        } catch (const std::exception&) {
            // Игнорируем ошибки вывода UI
        }
    }
}

void Graphics::refreshScreen()
{
    context->present(console);
}

void Graphics::clearScreen()
{
    console.clear();
}

// Ждем нажатия клавиши. Возвращаем true если что-то нажали.
bool Graphics::getInput(int& key)
{
    TCOD_key_t k = TCOD_console_wait_for_keypress(true);

    // Проверяем F11 для полноэкранного режима
    if (k.vk == TCODK_F11) {
        key = TCODK_F11;
        return true;
    }

    // Если есть символ (буква), отдаем его
    if (k.c != 0) {
        key = k.c; // 'w','a','s','d','q' и т.п.
        return true;
    }

    // Иначе отдаем код специальной клавиши (стрелки, ESC)
    key = k.vk;
    return (key != TCODK_NONE);
}

// Переключение полноэкранного режима
void Graphics::toggleFullscreen()
{
    // Получаем SDL окно через libtcod API
    auto sdl_window = context->get_sdl_window();
    if (sdl_window) {
        // Используем SDL2 функции для переключения полноэкранного режима
        const auto flags = SDL_GetWindowFlags(sdl_window);
        const bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_SetWindowFullscreen(sdl_window, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}
