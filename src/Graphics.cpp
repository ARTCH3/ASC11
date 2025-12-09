#include "Graphics.h"

#include "Map.h"
#include "Entity.h"
#include <algorithm>
#include <cmath>
#include <SDL2/SDL.h>
#include <string>
#include <cstring>
#include <vector>
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

// Простая линейная интерполяция между двумя цветами.
static tcod::ColorRGB lerpColor(const tcod::ColorRGB& a, const tcod::ColorRGB& b, float t)
{
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return tcod::ColorRGB{
        static_cast<uint8_t>(a.r + (b.r - a.r) * clamped),
        static_cast<uint8_t>(a.g + (b.g - a.g) * clamped),
        static_cast<uint8_t>(a.b + (b.b - a.b) * clamped)};
}

void Graphics::drawUI(const Entity& player,
                      const std::vector<Entity>& enemies,
                      int level,
                      const Map& map)
{
    // Рисуем UI сразу под картой (используем первую доступную строку)
    int uiY = Map::HEIGHT;

    // Очищаем строки UI
    for (int y = uiY; y < screenHeight; ++y) {
        for (int x = 0; x < screenWidth; ++x) {
            console.at({x, y}).ch = ' ';
            console.at({x, y}).bg = tcod::ColorRGB{0, 0, 0};
        }
    }

    char buffer[256];

    // --- Линия 1: здоровье игрока — полоса, затем цифры справа ---
    const float healthPercent = std::clamp(static_cast<float>(player.health) / static_cast<float>(player.maxHealth), 0.0f, 1.0f);

    // Метка HP слева с символом
    try {
        tcod::print(console,
                    {0, uiY},
                    "✙ Hero HP:",
                    tcod::ColorRGB{200, 200, 200},
                    std::nullopt);
    } catch (const std::exception&) {}

    // Рисуем полосу сразу после метки
    const int barX = 10;
    // Оставляем место под пробел и числа справа: " 000/000"
    const int reserveForText = 8;
    const int barWidth = std::max(0, screenWidth - barX - reserveForText);
    const int filled = static_cast<int>(std::round(healthPercent * barWidth));

    // Градиент перевернутый: красный -> желтый -> зеленый -> синий (совпадает с состояниями игрока, но в обратном направлении)
    auto hpGradient = [](float t) {
        const tcod::ColorRGB red{255, 60, 60};
        const tcod::ColorRGB yellow{255, 220, 80};
        const tcod::ColorRGB green{80, 220, 120};
        const tcod::ColorRGB blue{80, 160, 255};
        const float p = std::clamp(t, 0.0f, 1.0f);
        if (p < 0.33f) {
            const float lt = p / 0.33f;
            return lerpColor(red, yellow, lt);
        } else if (p < 0.66f) {
            const float lt = (p - 0.33f) / 0.33f;
            return lerpColor(yellow, green, lt);
        } else {
            const float lt = (p - 0.66f) / 0.34f;
            return lerpColor(green, blue, lt);
        }
    };

    for (int i = 0; i < barWidth; ++i) {
        const bool isFilled = i < filled;
        tcod::ColorRGB c = tcod::ColorRGB{60, 60, 60}; // фон для пустых
        if (isFilled) {
            const float t = (barWidth <= 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(barWidth - 1);
            c = hpGradient(t);
        }
        if (console.in_bounds({barX + i, uiY})) {
            console.at({barX + i, uiY}).ch = 0x2588; // блок
            console.at({barX + i, uiY}).fg = c;
        }
    }

    // Цифры справа от полосы
    snprintf(buffer, sizeof(buffer), " %d/%d", player.health, player.maxHealth);
    const int hpTextX = barX + barWidth;
    try {
        tcod::print(console,
                    {hpTextX, uiY},
                    buffer,
                    tcod::ColorRGB{240, 240, 240},
                    std::nullopt);
    } catch (const std::exception&) {}

    // --- Линия-разделитель между HP и уровнем ---
    if (uiY + 1 < screenHeight) {
        for (int x = 0; x < screenWidth; ++x) {
            if (console.in_bounds({x, uiY + 1})) {
                console.at({x, uiY + 1}).ch = 0x2500; // горизонтальная линия
                console.at({x, uiY + 1}).fg = tcod::ColorRGB{80, 80, 80};
            }
        }
    }

    // --- Линия 2: уровень и позиция компактно ---
    snprintf(buffer,
             sizeof(buffer),
             "↑ Level: %s | Pos %d,%d",
             toRoman(level).c_str(),
             player.pos.x,
             player.pos.y);
    try {
        tcod::print(console, {0, uiY + 2}, buffer, tcod::ColorRGB{180, 180, 180}, std::nullopt);
    } catch (const std::exception&) {}

    // --- Линия 3: управление ---
    try {
        tcod::print(console,
                    {0, uiY + 3},
                    "→ Move: WASD | Diag QEZC | Full F11 | ESC",
                    tcod::ColorRGB{200, 200, 200},
                    std::nullopt);
    } catch (const std::exception&) {}

    // --- Линия 4: легенда с цветами ---
    int legendY = uiY + 4;
    int cursorX = 0;
    auto safePrint = [&](const std::string& text, tcod::ColorRGB color) {
        if (cursorX < screenWidth) {
            tcod::print(console, {cursorX, legendY}, text.c_str(), color, std::nullopt);
            cursorX += static_cast<int>(text.size());
        }
    };

    safePrint("🛠Legend ", tcod::ColorRGB{180, 180, 180});
    safePrint("@", tcod::ColorRGB{100, 200, 255});
    safePrint(" Hero ", tcod::ColorRGB{180, 180, 180});
    safePrint("r", tcod::ColorRGB{255, 50, 50});
    safePrint(" Rat ", tcod::ColorRGB{180, 180, 180});
    // Сердечко предмета
    if (cursorX < screenWidth && console.in_bounds({cursorX, legendY})) {
        console.at({cursorX, legendY}).ch = SYM_ITEM;
        console.at({cursorX, legendY}).fg = tcod::ColorRGB{255, 180, 120};
    }
    cursorX += 1;
    safePrint(" Medkit  ", tcod::ColorRGB{180, 180, 180});
    safePrint("^ Exit", tcod::ColorRGB{200, 200, 120});

    // --- Линии 5+: список видимых врагов с их HP ---
    const int headerY = uiY + 4;
    try {
        tcod::print(console,
                    {0, headerY},
                    "Enemies in sight:",
                    tcod::ColorRGB{210, 210, 210},
                    std::nullopt);
    } catch (const std::exception&) {}

    const int columnWidth = 14; // компактнее, без полосок баров
    const int maxColumns = std::max(1, screenWidth / columnWidth);
    const int maxRows = std::max(0, screenHeight - (headerY + 1));

    int slot = 0;
    for (const auto& enemy : enemies) {
        if (!map.isVisible(enemy.pos.x, enemy.pos.y) || !enemy.isAlive()) {
            continue;
        }

        const int row = slot / maxColumns;
        if (row >= maxRows) {
            break; // Больше нет места
        }
        const int col = slot % maxColumns;

        const int enemyY = headerY + 1 + row;
        cursorX = col * columnWidth;

        // Название моба его цветом
        const std::string mobName = "Rat";
        try {
            tcod::print(console, {cursorX, enemyY}, mobName.c_str(), enemy.color, std::nullopt);
        } catch (const std::exception&) {}
        cursorX += static_cast<int>(mobName.size());

        // Координаты в скобках
        snprintf(buffer, sizeof(buffer), "(%d,%d)", enemy.pos.x, enemy.pos.y);
        try {
            tcod::print(console, {cursorX, enemyY}, buffer, tcod::ColorRGB{170, 170, 170}, std::nullopt);
        } catch (const std::exception&) {}
        cursorX += static_cast<int>(strlen(buffer));

        // Цветные цифры HP (градиент от синего к красному)
        const float enemyPct = static_cast<float>(enemy.health) / static_cast<float>(enemy.maxHealth);
        const tcod::ColorRGB enemyHpColor = lerpColor(
            tcod::ColorRGB{80, 120, 255},   // полный HP — сине-голубой
            tcod::ColorRGB{255, 50, 50},    // низкий HP — красный
            1.0f - std::clamp(enemyPct, 0.0f, 1.0f));

        snprintf(buffer, sizeof(buffer), "%d/%d", enemy.health, enemy.maxHealth);
        try {
            tcod::print(console, {cursorX, enemyY}, buffer, enemyHpColor, std::nullopt);
        } catch (const std::exception&) {}

        slot++;
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
