#include "Graphics.h"

#include "Map.h"
#include "Entity.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <SDL2/SDL.h>
#include <string>
#include <cstring>
#include <vector>

Graphics::Graphics(int width,
                   int height,
                   int leftPanelWidth_,
                   int rightPanelWidth_,
                   int topPanelHeight_,
                   int bottomPanelHeight_)
    : screenWidth(width),
      screenHeight(height),
      leftPanelWidth(leftPanelWidth_),
      rightPanelWidth(rightPanelWidth_),
      topPanelHeight(topPanelHeight_),
      bottomPanelHeight(bottomPanelHeight_),
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
    
    // ПОЛНОСТЬЮ ОТКЛЮЧАЕМ кастомный tileset - используем только стандартный встроенный libtcod
    // Кастомный PNG tileset (terminal12x12_gs_ro.png) не соответствует стандартному CP437 порядку
    // и вызывает "кракозябры" при отображении символов
    // Стандартный tileset libtcod гарантированно работает правильно с CP437 символами
    // Без установки params.tileset libtcod использует стандартный встроенный tileset

    context = tcod::new_context(params);
    
    if (!context) {
        throw std::runtime_error("Failed to create context");
    }

    // Включаем полноэкранный режим. Масштабирование и отрисовку оставляем libtcod/SDL,
    // чтобы клетки оставались квадратными и интерфейс выглядел так же, как в оконном режиме.
    if (auto sdl_window = context->get_sdl_window()) {
        SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

Graphics::~Graphics() = default;

// Вспомогательная функция для плавного перехода между двумя цветами.
// Объявление нужно здесь, чтобы её было видно в drawPlayer ниже.
static tcod::ColorRGB lerpColor(const tcod::ColorRGB& a, const tcod::ColorRGB& b, float t);

void Graphics::drawMap(const Map& map, int playerX, int playerY, int torchRadius, bool showExitHint, const std::vector<std::pair<int, int>>& fireflyPositions)
{
    // Обновляем эффект факела с пульсацией в реальном времени
    // Используем SDL_GetTicks() для надежного измерения времени (работает каждый кадр!)
    static Uint32 lastTime = SDL_GetTicks();
    Uint32 currentTime = SDL_GetTicks();
    float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f; // Конвертируем в секунды
    if (deltaTime > 0.1f) deltaTime = 0.1f; // Ограничиваем максимальный шаг для стабильности
    if (deltaTime < 0.0f) deltaTime = 0.0f; // Защита от отрицательных значений
    lastTime = currentTime;
    
    static float time = 0.0f;
    // <<< ДЛЯ ИЗМЕНЕНИЯ СКОРОСТИ ПУЛЬСАЦИИ: измени множитель здесь (больше = быстрее) >>>
    time += deltaTime * 1.0f; // Пульсация в реальном времени (быстрая, как при движении)
    
    torchX += deltaTime * 6.0f; // Обновляем в реальном времени
    float dx = 0.0f, dy = 0.0f, di = 0.0f;
    
    // Очень слабое случайное смещение позиции факела для эффекта мерцания
    float tdx = torchX + 20.0f;
    dx = torchNoise.get(&tdx) * 0.5f; // Еще больше уменьшили амплитуду
    tdx += 30.0f;
    dy = torchNoise.get(&tdx) * 0.5f;
    
    // Пульсация интенсивности света (даже когда игрок стоит)
    // Используем синус для плавной пульсации
    // <<< ДЛЯ ИЗМЕНЕНИЯ СКОРОСТИ ПУЛЬСАЦИИ: измени множитель в sin() здесь (больше = быстрее) >>>
    float pulse = 0.25f + 0.1f * std::sin(time * 2.0f); // Быстрая пульсация от 0.15 до 0.35
    di = pulse * (0.5f + 0.5f * torchNoise.get(&torchX)); // Комбинируем с шумом для естественности
    
    const float TORCH_RADIUS = static_cast<float>(torchRadius);
    const float SQUARED_TORCH_RADIUS = TORCH_RADIUS * TORCH_RADIUS;
    
    // Цвета для стен и пола
    const tcod::ColorRGB darkWall{0, 0, 100};      // Темные стены
    const tcod::ColorRGB lightWall{130, 110, 50};  // Светлые стены
    const tcod::ColorRGB darkGround{50, 50, 150};  // Темный пол
    const tcod::ColorRGB lightGround{200, 180, 50}; // Светлый пол
    
    // Константы для позиционирования карты в центре экрана
    const int leftPanelWidth = this->leftPanelWidth;
    const int topPanelHeight = std::max(this->topPanelHeight, 2); // резервируем 2 строки под HP/Shield
    
    // Отрисовываем карту с учетом FOV и эффекта факела
    // Карта рисуется в центре экрана (смещение на leftPanelWidth по X и topPanelHeight по Y)
    for (int mapY = 0; mapY < Map::HEIGHT; ++mapY) {
        for (int mapX = 0; mapX < Map::WIDTH; ++mapX) {
            const int screenX = leftPanelWidth + mapX;
            const int screenY = topPanelHeight + mapY;
            
            // Проверяем границы консоли
            if (!console.in_bounds({screenX, screenY})) {
                continue;
            }
            
            const bool isVisible = map.isVisible(mapX, mapY);
            const bool isExplored = map.isExplored(mapX, mapY);
            const bool isWall = map.isWall(mapX, mapY);
            
            if (!isVisible) {
                // Невидимые клетки
                if (isExplored) {
                    // Исследованные, но невидимые - затемненные
                    console.at({screenX, screenY}).bg = isWall ? darkWall : darkGround;
                    console.at({screenX, screenY}).ch = 219; // Блок (CP437 код 219 = █)
                    console.at({screenX, screenY}).fg = isWall ? darkWall : darkGround;
                } else {
                    // Не исследованные - черные
                    console.at({screenX, screenY}).bg = colorDark;
                    console.at({screenX, screenY}).ch = ' ';
                    console.at({screenX, screenY}).fg = colorDark;
                }
            } else {
                // Видимые клетки с эффектом факела
                tcod::ColorRGB base = isWall ? darkWall : darkGround;
                tcod::ColorRGB light = isWall ? lightWall : lightGround;
                
                // Вычисляем расстояние до факела (с учетом смещения)
                const float r = static_cast<float>((mapX - playerX + dx) * (mapX - playerX + dx) + 
                                                   (mapY - playerY + dy) * (mapY - playerY + dy));
                
                if (r < SQUARED_TORCH_RADIUS) {
                    // Интерполируем цвет в зависимости от расстояния
                    // Делаем более мягкий переход (используем квадратный корень для более плавного затухания)
                    float normalizedDist = r / SQUARED_TORCH_RADIUS;
                    float smoothDist = std::sqrt(normalizedDist); // Более плавное затухание
                    // Старая красивая пульсирующая формула: l = 1.0 в позиции игрока, 0.0 на границе радиуса, с учетом пульсации
                    const float l = std::clamp((1.0f - smoothDist) * (0.7f + di), 0.0f, 1.0f);
                    base = tcod::ColorRGB{TCODColor::lerp(base, light, l)};
                }
                
                // Добавляем свет от светлячков (факел с меньшим радиусом, пульсирующий в реальном времени)
                if (!fireflyPositions.empty()) {
                    // Радиус факела светлячка (меньше чем у игрока)
                    const float FIREFLY_TORCH_RADIUS = 2.0f;
                    const float SQUARED_FIREFLY_RADIUS = FIREFLY_TORCH_RADIUS * FIREFLY_TORCH_RADIUS;
                    
                    // Пульсация для светлячков (синхронизирована с основным временем, как у игрока)
                    float fireflyPulse = 0.25f + 0.1f * std::sin(time * 2.0f); // Та же пульсация что у игрока
                    float fireflyDi = fireflyPulse * (0.5f + 0.5f * torchNoise.get(&torchX)); // Комбинируем с шумом
                    
                    for (const auto& fireflyPos : fireflyPositions) {
                        // Вычисляем расстояние до светлячка (с учетом небольшого смещения для мерцания)
                        float fireflyDx = torchNoise.get(&torchX) * 0.3f;
                        float fireflyDy = torchNoise.get(&torchX) * 0.3f;
                        const float fireflyDist = static_cast<float>((mapX - fireflyPos.first + fireflyDx) * (mapX - fireflyPos.first + fireflyDx) + 
                                                                     (mapY - fireflyPos.second + fireflyDy) * (mapY - fireflyPos.second + fireflyDy));
                        
                        if (fireflyDist < SQUARED_FIREFLY_RADIUS) {
                            // Интерполируем цвет с учетом пульсации (как у игрока)
                            float normalizedFireflyDist = fireflyDist / SQUARED_FIREFLY_RADIUS;
                            float smoothFireflyDist = std::sqrt(normalizedFireflyDist);
                            const float fireflyLight = std::clamp((1.0f - smoothFireflyDist) * (0.7f + fireflyDi), 0.0f, 1.0f);
                            base = tcod::ColorRGB{TCODColor::lerp(base, light, fireflyLight)};
                        }
                    }
                }
                
                // Рисуем цветной блок
                console.at({screenX, screenY}).bg = base;
                console.at({screenX, screenY}).ch = 219; // Символ блока (CP437 код 219 = █)
                console.at({screenX, screenY}).fg = base;
            }
        }
    }

    // Дополнительно: если нужно подсветить лестницу (подсказка направления),
    // рисуем символ выхода поверх всего, даже если он вне текущего FOV.
    if (showExitHint && map.exitPos.x >= 0 && map.exitPos.y >= 0) {
        int ex = map.exitPos.x;
        int ey = map.exitPos.y;
        int sx = leftPanelWidth + ex;
        int sy = topPanelHeight + ey;
        if (console.in_bounds({sx, sy})) {
            auto& cell = console.at({sx, sy});
            cell.ch = '#';
            cell.fg = tcod::ColorRGB{255, 255, 255};
        }
    }
}

void Graphics::drawEntity(const Entity& entity)
{
    // Рисуем сущность только если она видна
    // Учитываем смещение карты в центре экрана
    const int leftPanelWidth = this->leftPanelWidth;
    const int topPanelHeight = std::max(this->topPanelHeight, 2);
    
    int mapX = entity.pos.x;
    int mapY = entity.pos.y;
    int screenX = leftPanelWidth + mapX;
    int screenY = topPanelHeight + mapY;

    if (mapX >= 0 && mapX < Map::WIDTH && mapY >= 0 && mapY < Map::HEIGHT &&
        screenX < screenWidth && screenY < screenHeight &&
        console.in_bounds({screenX, screenY})) {
        console.at({screenX, screenY}).ch = entity.symbol;
        console.at({screenX, screenY}).fg = entity.color;
    }
}

void Graphics::drawItem(const Item& item)
{
    // Учитываем смещение карты в центре экрана
    const int leftPanelWidth = this->leftPanelWidth;
    const int topPanelHeight = std::max(this->topPanelHeight, 2);
    
    int mapX = item.pos.x;
    int mapY = item.pos.y;
    int screenX = leftPanelWidth + mapX;
    int screenY = topPanelHeight + mapY;

    if (mapX >= 0 && mapX < Map::WIDTH && mapY >= 0 && mapY < Map::HEIGHT &&
        screenX < screenWidth && screenY < screenHeight &&
        console.in_bounds({screenX, screenY})) {
        // Цвет предмета зависит от типа:
        // + : увеличение максимального HP (зеленый)
        // . : "прозрачный" предмет (более темный серый)
        // O : щит
        // ? : квестовый предмет
        // $ : обычный аптечный предмет
        tcod::ColorRGB itemColor = colorItem;
        if (item.symbol == static_cast<char>(SYM_MAX_HP)) {
            itemColor = tcod::ColorRGB{0, 204, 0};
        } else if (item.symbol == static_cast<char>(SYM_TRAP)) {
            itemColor = tcod::ColorRGB{40, 40, 40};
        } else if (item.symbol == static_cast<char>(SYM_SHIELD)) {
            // Щит — чисто белый, чтобы сразу бросался в глаза.
            itemColor = tcod::ColorRGB{255, 255, 255};
        } else if (item.symbol == static_cast<char>(SYM_QUEST)) {
            itemColor = tcod::ColorRGB{255, 255, 255};
        }

        console.at({screenX, screenY}).ch = item.symbol;
        console.at({screenX, screenY}).fg = itemColor;
    }
}

// Оверлей выбора перка при переходе по лестнице.
// Закрашивает центральный слой (игровой мир) в чёрный и рисует три колонки с вариантами.
void Graphics::drawLevelChoiceMenu(int variant1, int variant2, int variant3)
{
    const int gameAreaStartX = leftPanelWidth;
    const int gameAreaStartY = std::max(topPanelHeight, 2);
    const int gameWidth = Map::WIDTH;
    const int gameHeight = Map::HEIGHT;

    // 1. Полностью "затемняем" центральную область (игровой мир).
    for (int y = 0; y < gameHeight; ++y) {
        for (int x = 0; x < gameWidth; ++x) {
            int sx = gameAreaStartX + x;
            int sy = gameAreaStartY + y;
            if (!console.in_bounds({sx, sy})) continue;
            auto& cell = console.at({sx, sy});
            cell.bg = tcod::ColorRGB{0, 0, 0};
            cell.ch = ' ';
            cell.fg = tcod::ColorRGB{255, 255, 255};
        }
    }

    // Небольшой helper для рисования текста по центру.
    auto drawText = [&](int sx, int sy, const std::string& text, const tcod::ColorRGB& color = tcod::ColorRGB{255, 255, 255}) {
        for (int i = 0; i < static_cast<int>(text.size()); ++i) {
            int x = sx + i;
            int y = sy;
            if (!console.in_bounds({x, y})) continue;
            console.at({x, y}).ch = text[i];
            console.at({x, y}).fg = color;
        }
    };

    // 2. Заголовок чуть выше центра.
    const std::string title = "Make one choice";
    int centerX = gameAreaStartX + gameWidth / 2;
    int titleX = centerX - static_cast<int>(title.size()) / 2;
    int titleY = gameAreaStartY + gameHeight / 4;
    drawText(titleX, titleY, title);

    // 3. Три колонки: левая, центральная и правая.
    int colYStart = titleY + 3;
    int col1Center = gameAreaStartX + gameWidth / 6;
    int col2Center = gameAreaStartX + gameWidth / 2;
    int col3Center = gameAreaStartX + (gameWidth * 5) / 6;

    // Функция для рисования одной колонки с новым оформлением
    auto drawColumn = [&](int center, int num, const std::string& category, 
                          const std::vector<std::string>& effects) {
        int y = colYStart;
        
        // Первая строка: [1], [2] или [3]
        std::string numStr = "[" + std::to_string(num) + "]";
        int numX = center - static_cast<int>(numStr.size()) / 2;
        drawText(numX, y, numStr, tcod::ColorRGB{255, 255, 100}); // Жёлтый цвет для номера
        y += 2;
        
        // Вторая строка: "-On each floor-" или "-Next floor only-" и т.д.
        std::string categoryLine = "-" + category + "-";
        int catX = center - static_cast<int>(categoryLine.size()) / 2;
        drawText(catX, y, categoryLine);
        y += 2;
        
        // Далее список эффектов, каждый на новой строке
        for (const auto& effect : effects) {
            int effX = center - static_cast<int>(effect.size()) / 2;
            drawText(effX, y, effect);
            y += 1;
        }
    };

    // Используем переданные варианты из расширенного пула модификаторов
    // Вариант 1: Постоянные эффекты (On each floor)
    int v1 = variant1;
    std::vector<std::string> effects1;
    if (v1 == 0) {
        effects1 = {"+5 rats", "+2 medkits", "Firefly reveals fog"};
    } else if (v1 == 1) {
        effects1 = {"+3 bears", "+1 shield", "Max HP +5"};
    } else if (v1 == 2) {
        effects1 = {"+4 snakes", "+3 medkits", "Torch +2 radius"};
    } else if (v1 == 3) {
        effects1 = {"+2 ghosts", "+2 MaxHP items", "Firefly reveals fog"};
    } else if (v1 == 4) {
        effects1 = {"+3 crabs", "+4 medkits", "Shield +2"};
    } else {
        effects1 = {"+6 rats", "+1 Firefly", "Torch +1 radius"};
    }
    drawColumn(col1Center, 1, "On each floor", effects1);

    // Вариант 2: Случайные эффекты (Random)
    int v2 = variant2;
    std::vector<std::string> effects2;
    if (v2 == 0) {
        effects2 = {"Poison bears", "More shields", "Show stair hint"};
    } else if (v2 == 1) {
        effects2 = {"+3 rats next", "Ghost curse", "Torch -2 radius"};
    } else if (v2 == 2) {
        effects2 = {"Crab inversion", "+2 MaxHP next", "Firefly reveals"};
    } else if (v2 == 3) {
        effects2 = {"Snake poison", "+4 medkits next", "Shield +1"};
    } else if (v2 == 4) {
        effects2 = {"Bear poison", "Quest items", "Torch +1 radius"};
    } else {
        effects2 = {"Ghost curse", "+3 snakes next", "MaxHP +3"};
    }
    drawColumn(col2Center, 2, "Random", effects2);

    // Вариант 3: Временные эффекты (Next floor only)
    int v3 = variant3;
    std::vector<std::string> effects3;
    if (v3 == 0) {
        effects3 = {"+2 snakes", "More MaxHP items", "Torch radius -3"};
    } else if (v3 == 1) {
        effects3 = {"+3 rats", "Torch radius -2", "+2 medkits"};
    } else if (v3 == 2) {
        effects3 = {"+1 bear", "MaxHP +2 items", "Torch radius -4"};
    } else if (v3 == 3) {
        effects3 = {"+2 ghosts", "Torch radius -3", "+3 medkits"};
    } else if (v3 == 4) {
        effects3 = {"+4 snakes", "Torch radius -2", "MaxHP +1 item"};
    } else {
        effects3 = {"+1 crab", "Torch radius -5", "+1 MaxHP item"};
    }
    drawColumn(col3Center, 3, "Next floor only", effects3);
}

// Вспомогательная функция для преобразования числа в римскую цифру
static std::string intToRoman(int num) {
    if (num <= 0) return "I";
    std::string result;
    const std::vector<std::pair<int, std::string>> values = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"},
        {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
        {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}
    };
    for (const auto& [value, symbol] : values) {
        while (num >= value) {
            result += symbol;
            num -= value;
        }
    }
    return result;
}

void Graphics::drawDeathScreen(int level,
                                int killsRat, int killsBear, int killsSnake, int killsGhost, int killsCrab,
                                int itemsMedkit, int itemsMaxHP, int itemsShield, int itemsTrap, int itemsQuest,
                                const std::vector<std::string>& collectedPerks)
{
    const int gameAreaStartX = leftPanelWidth;
    const int gameAreaStartY = std::max(topPanelHeight, 2);
    const int gameWidth = Map::WIDTH;
    const int gameHeight = Map::HEIGHT;

    // 1. Полностью "затемняем" центральную область (игровой мир).
    for (int y = 0; y < gameHeight; ++y) {
        for (int x = 0; x < gameWidth; ++x) {
            int sx = gameAreaStartX + x;
            int sy = gameAreaStartY + y;
            if (!console.in_bounds({sx, sy})) continue;
            auto& cell = console.at({sx, sy});
            cell.bg = tcod::ColorRGB{0, 0, 0};
            cell.ch = ' ';
            cell.fg = tcod::ColorRGB{255, 255, 255};
        }
    }

    // Небольшой helper для рисования текста по центру.
    auto drawText = [&](int sx, int sy, const std::string& text, const tcod::ColorRGB& color = tcod::ColorRGB{255, 255, 255}) {
        for (int i = 0; i < static_cast<int>(text.size()); ++i) {
            int x = sx + i;
            int y = sy;
            if (!console.in_bounds({x, y})) continue;
            console.at({x, y}).ch = text[i];
            console.at({x, y}).fg = color;
        }
    };

    // 2. Заголовок "Game Over" чуть выше центра
    const std::string title = "Game Over";
    int centerX = gameAreaStartX + gameWidth / 2;
    int titleX = centerX - static_cast<int>(title.size()) / 2;
    int titleY = gameAreaStartY + gameHeight / 6;
    drawText(titleX, titleY, title, tcod::ColorRGB{255, 0, 0}); // Красный цвет

    // 3. Уровень в римской цифре
    std::string floorText = "Floor " + intToRoman(level);
    int floorX = centerX - static_cast<int>(floorText.size()) / 2;
    int floorY = titleY + 2;
    drawText(floorX, floorY, floorText);

    // 4. Три столбца: убитые мобы, собранные предметы, модификации
    int colYStart = floorY + 3;
    int col1Center = gameAreaStartX + gameWidth / 6;
    int col2Center = gameAreaStartX + gameWidth / 2;
    int col3Center = gameAreaStartX + (gameWidth * 5) / 6;

    // Столбец 1: Убитые мобы
    int y1 = colYStart;
    std::string erasedLabel = "Erased:";
    drawText(col1Center - static_cast<int>(erasedLabel.size()) / 2, y1, erasedLabel, tcod::ColorRGB{255, 100, 100});
    y1 += 2;
    if (killsRat > 0) {
        std::string text = "r - Rat (" + std::to_string(killsRat) + ")";
        drawText(col1Center - static_cast<int>(text.size()) / 2, y1, text);
        y1 += 1;
    }
    if (killsBear > 0) {
        std::string text = "b - Bear (" + std::to_string(killsBear) + ")";
        drawText(col1Center - static_cast<int>(text.size()) / 2, y1, text);
        y1 += 1;
    }
    if (killsSnake > 0) {
        std::string text = "s - Snake (" + std::to_string(killsSnake) + ")";
        drawText(col1Center - static_cast<int>(text.size()) / 2, y1, text);
        y1 += 1;
    }
    if (killsGhost > 0) {
        std::string text = "g - Ghost (" + std::to_string(killsGhost) + ")";
        drawText(col1Center - static_cast<int>(text.size()) / 2, y1, text);
        y1 += 1;
    }
    if (killsCrab > 0) {
        std::string text = "c - Crab (" + std::to_string(killsCrab) + ")";
        drawText(col1Center - static_cast<int>(text.size()) / 2, y1, text);
        y1 += 1;
    }
    if (killsRat == 0 && killsBear == 0 && killsSnake == 0 && killsGhost == 0 && killsCrab == 0) {
        drawText(col1Center - 4, y1, "None");
    }

    // Столбец 2: Собранные предметы
    int y2 = colYStart;
    std::string itemsLabel = "Items:";
    drawText(col2Center - static_cast<int>(itemsLabel.size()) / 2, y2, itemsLabel, tcod::ColorRGB{100, 255, 100});
    y2 += 2;
    if (itemsMedkit > 0) {
        std::string text = "$ - Medkit (" + std::to_string(itemsMedkit) + ")";
        drawText(col2Center - static_cast<int>(text.size()) / 2, y2, text);
        y2 += 1;
    }
    if (itemsMaxHP > 0) {
        std::string text = "+ - MaxHP (" + std::to_string(itemsMaxHP) + ")";
        drawText(col2Center - static_cast<int>(text.size()) / 2, y2, text);
        y2 += 1;
    }
    if (itemsShield > 0) {
        std::string text = "O - Shield (" + std::to_string(itemsShield) + ")";
        drawText(col2Center - static_cast<int>(text.size()) / 2, y2, text);
        y2 += 1;
    }
    if (itemsTrap > 0) {
        std::string text = ". - Trap (" + std::to_string(itemsTrap) + ")";
        drawText(col2Center - static_cast<int>(text.size()) / 2, y2, text);
        y2 += 1;
    }
    if (itemsQuest > 0) {
        std::string text = "? - Quest (" + std::to_string(itemsQuest) + ")";
        drawText(col2Center - static_cast<int>(text.size()) / 2, y2, text);
        y2 += 1;
    }
    if (itemsMedkit == 0 && itemsMaxHP == 0 && itemsShield == 0 && itemsTrap == 0 && itemsQuest == 0) {
        drawText(col2Center - 4, y2, "None");
    }

    // Столбец 3: Модификации (перки)
    int y3 = colYStart;
    std::string perksLabel = "Perks:";
    drawText(col3Center - static_cast<int>(perksLabel.size()) / 2, y3, perksLabel, tcod::ColorRGB{100, 100, 255});
    y3 += 2;
    if (collectedPerks.empty()) {
        drawText(col3Center - 4, y3, "None");
    } else {
        for (const auto& perk : collectedPerks) {
            drawText(col3Center - static_cast<int>(perk.size()) / 2, y3, perk);
            y3 += 1;
        }
    }

    // Подсказка внизу
    std::string hint = "Press [F] to restart";
    int hintX = centerX - static_cast<int>(hint.size()) / 2;
    int hintY = gameAreaStartY + gameHeight - 3;
    drawText(hintX, hintY, hint, tcod::ColorRGB{200, 200, 200});
}

void Graphics::drawPlayer(const Entity& player, bool isPoisoned, bool hasShield)
{
    // Рисуем игрока с динамическим цветом в зависимости от здоровья
    // Учитываем смещение карты в центре экрана
    const int leftPanelWidth = this->leftPanelWidth;
    const int topPanelHeight = std::max(this->topPanelHeight, 2);
    
    int mapX = player.pos.x;
    int mapY = player.pos.y;
    int screenX = leftPanelWidth + mapX;
    int screenY = topPanelHeight + mapY;

    if (mapX >= 0 && mapX < Map::WIDTH && mapY >= 0 && mapY < Map::HEIGHT &&
        screenX < screenWidth && screenY < screenHeight &&
        console.in_bounds({screenX, screenY})) {
        // Вычисляем цвет в зависимости от здоровья.
        // Если игрок отравлен — временно перекрашиваем его в ядовито‑зелёный цвет,
        // чтобы было сразу видно состояние.
        float healthPercent = static_cast<float>(player.health) / static_cast<float>(player.maxHealth);
        
        tcod::ColorRGB playerColor;
        
        if (hasShield) {
            // При активном щите игрок подсвечен белым, чтобы это было явно видно.
            playerColor = tcod::ColorRGB{255, 255, 255};
        } else if (isPoisoned) {
            // Ядовитый/болотный зелёный, немного темнее при низком HP.
            const tcod::ColorRGB highHpPoison{80, 240, 120};
            const tcod::ColorRGB lowHpPoison{20, 100, 40};
            playerColor = lerpColor(lowHpPoison, highHpPoison, std::clamp(healthPercent, 0.0f, 1.0f));
        } else {
            // Обычная схема цвета игрока по здоровью.
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
        }
        
        console.at({screenX, screenY}).ch = player.symbol;
        console.at({screenX, screenY}).fg = playerColor;
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
                      const Map& map,
                      bool isPlayerPoisoned,
                      bool isPlayerGhostCursed,
                      int shieldTurns,
                int shieldWhiteSegments,
                      bool questActive,
                      int questKills,
                int questTarget,
                bool seenRat,
                bool seenBear,
                bool seenSnake,
                bool seenGhost,
                bool seenCrab,
                bool seenMedkit,
                bool seenMaxHP,
                bool seenShield,
                bool seenTrap,
                bool seenQuest)
{
    char buffer[256];

    // Константы для позиционирования
    const int leftPanelWidth = this->leftPanelWidth;
    const int rightPanelWidth = this->rightPanelWidth;
    const int topPanelHeight = std::max(this->topPanelHeight, 2); // резерв 2 строки под HP/Shield
    const int bottomPanelHeight = this->bottomPanelHeight;
    const int gameAreaStartX = leftPanelWidth;
    const int gameAreaStartY = topPanelHeight;
    const int bottomPanelY = screenHeight - bottomPanelHeight;
    
    // Очищаем все UI области черным цветом
    const tcod::ColorRGB black{0, 0, 0};
    const tcod::ColorRGB white{255, 255, 255};
    
    // Очищаем верхнюю панель
    for (int x = 0; x < screenWidth; ++x) {
            if (console.in_bounds({x, 0})) {
            console.at({x, 0}).ch = ' ';
            console.at({x, 0}).bg = black;
            console.at({x, 0}).fg = white;
        }
    }
    
    // Очищаем левую боковую панель
    for (int y = gameAreaStartY; y < bottomPanelY; ++y) {
        for (int x = 0; x < leftPanelWidth; ++x) {
            if (console.in_bounds({x, y})) {
                console.at({x, y}).ch = ' ';
                console.at({x, y}).bg = black;
                console.at({x, y}).fg = white;
            }
        }
    }
    
    // Очищаем правую боковую панель
    for (int y = gameAreaStartY; y < bottomPanelY; ++y) {
        for (int x = gameAreaStartX + Map::WIDTH; x < screenWidth; ++x) {
            if (console.in_bounds({x, y})) {
                console.at({x, y}).ch = ' ';
                console.at({x, y}).bg = black;
                console.at({x, y}).fg = white;
            }
        }
    }
    
    // Очищаем нижнюю панель
        for (int x = 0; x < screenWidth; ++x) {
        if (console.in_bounds({x, bottomPanelY})) {
            console.at({x, bottomPanelY}).ch = ' ';
            console.at({x, bottomPanelY}).bg = black;
            console.at({x, bottomPanelY}).fg = white;
        }
    }

    // === ВЕРХНЯЯ ПАНЕЛЬ (y=0): Полоса здоровья с тире ===
    const float healthPercent = std::clamp(static_cast<float>(player.health) / static_cast<float>(player.maxHealth), 0.0f, 1.0f);

    // Градиент HP: красный -> желтый -> зеленый -> синий
    auto hpGradientNormal = [](float t) {
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

    // Рисуем полосу HP по ширине игрового мира (строго над картой), символ '-'
    for (int i = 0; i < Map::WIDTH; ++i) {
        int x = leftPanelWidth + i;
        const bool isFilled = i < static_cast<int>(healthPercent * Map::WIDTH);
        tcod::ColorRGB dashColor = tcod::ColorRGB{100, 100, 100};
        if (isPlayerGhostCursed) {
            dashColor = tcod::ColorRGB{80, 80, 80};
        } else if (isFilled) {
            if (isPlayerPoisoned) {
                const tcod::ColorRGB highHpGreen{90, 240, 120};
                const tcod::ColorRGB lowHpGreen{10, 80, 30};
                dashColor = lerpColor(lowHpGreen, highHpGreen, healthPercent);
            } else {
                const float t = Map::WIDTH <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(Map::WIDTH - 1);
                dashColor = hpGradientNormal(t);
            }
        }
        if (console.in_bounds({x, 0})) {
            console.at({x, 0}).ch = '-';
            console.at({x, 0}).fg = dashColor;
            console.at({x, 0}).bg = black;
        }
    }

    // Полоса щита под HP (строка 1). Слева синие (оставшиеся), справа серые (потраченные).
    int shieldY = 1;
    // shieldTurns — сколько синих делений осталось, shieldWhiteSegments — сколько белых (урон по щиту).
    int blueCount  = std::clamp(shieldTurns, 0, Map::WIDTH);
    int whiteCount = std::clamp(shieldWhiteSegments, 0, Map::WIDTH - blueCount);
    for (int i = 0; i < Map::WIDTH; ++i) {
        int x = leftPanelWidth + i;
        tcod::ColorRGB dashColor{60, 60, 60}; // по умолчанию серый
        if (i < blueCount) {
            dashColor = tcod::ColorRGB{80, 120, 255}; // синий
        } else if (i < blueCount + whiteCount) {
            dashColor = tcod::ColorRGB{230, 230, 230}; // белый (побитый, но ещё живой щит)
        }
        if (console.in_bounds({x, shieldY})) {
            console.at({x, shieldY}).ch = '-';
            console.at({x, shieldY}).fg = dashColor;
            console.at({x, shieldY}).bg = black;
        }
    }

    
    // Число HP — по центру левой панели (верхняя строка)
    if (isPlayerGhostCursed) {
        snprintf(buffer, sizeof(buffer), "??");
    } else {
        snprintf(buffer, sizeof(buffer), "%d", player.health);
    }
    int currentHpX = (leftPanelWidth - static_cast<int>(strlen(buffer))) / 2;
    try {
        tcod::print(console, {currentHpX, 0}, buffer, white, std::nullopt);
    } catch (const std::exception&) {}

    // Оформление блока "Nearby" так же, как Legend: линия сверху, заголовок, линия снизу.
    const tcod::ColorRGB nearbyLabelColor{200, 200, 200};

    // Линия '-' прямо под HP по ширине левой панели
    int nearbyTopY = 1;
    for (int x = 0; x < leftPanelWidth; ++x) {
        if (console.in_bounds({x, nearbyTopY})) {
            console.at({x, nearbyTopY}).ch = '-';
            console.at({x, nearbyTopY}).fg = nearbyLabelColor;
            console.at({x, nearbyTopY}).bg = black;
        }
    }

    // Надпись "Nearby" по центру
    int nearbyLabelY = nearbyTopY + 1;
    int nearbyLabelX = (leftPanelWidth - 6) / 2; // "Nearby" длина 6
    try {
        tcod::print(console, {nearbyLabelX, nearbyLabelY}, "Nearby", nearbyLabelColor, std::nullopt);
    } catch (const std::exception&) {}

    // Линия '-' под подписью
    int nearbyBottomY = nearbyLabelY + 1;
    for (int x = 0; x < leftPanelWidth; ++x) {
        if (console.in_bounds({x, nearbyBottomY})) {
            console.at({x, nearbyBottomY}).ch = '-';
            console.at({x, nearbyBottomY}).fg = nearbyLabelColor;
            console.at({x, nearbyBottomY}).bg = black;
        }
    }

    // Список мобов начинаем ещё на строку ниже, чтобы не прилипали к заголовку
    int nearbyY = nearbyBottomY + 1;

    // Максимальное ХП справа сверху по центру правой панели
    if (isPlayerGhostCursed) {
        snprintf(buffer, sizeof(buffer), "??");
    } else {
        snprintf(buffer, sizeof(buffer), "%d", player.maxHealth);
    }
    int rightPanelStartX = gameAreaStartX + Map::WIDTH;
    int maxHpX = rightPanelStartX + (rightPanelWidth - static_cast<int>(strlen(buffer))) / 2;
    try {
        tcod::print(console, {maxHpX, 0}, buffer, white, std::nullopt);
    } catch (const std::exception&) {}

    
    // Функция для определения направления моба относительно игрока
    auto getDirectionString = [](int enemyX, int enemyY, int playerX, int playerY) -> std::string {
        int dx = enemyX - playerX;
        int dy = enemyY - playerY;
        
        if (dx == 0 && dy == 0) return "(*)"; // На одной клетке
        if (dx == 0 && dy < 0) return "(^)";  // Вверх
        if (dx == 0 && dy > 0) return "(v)";  // Вниз
        if (dx > 0 && dy == 0) return "(->)"; // Вправо
        if (dx < 0 && dy == 0) return "(<-)"; // Влево
        if (dx > 0 && dy < 0) return "(-v>)"; // Вправо-вверх (диагональ)
        if (dx > 0 && dy > 0) return "(-v>)"; // Вправо-вниз (диагональ)
        if (dx < 0 && dy < 0) return "(-v>)"; // Влево-вверх (диагональ)
        if (dx < 0 && dy > 0) return "(-v>)"; // Влево-вниз (диагональ)
        return "(?)";
    };
    
    // Собираем видимых врагов
    std::vector<std::pair<const Entity*, std::string>> nearbyEnemies;
    for (const auto& enemy : enemies) {
        if (enemy.isAlive() && map.isVisible(enemy.pos.x, enemy.pos.y)) {
        std::string mobName;
        if (enemy.symbol == SYM_BEAR) {
            mobName = "Bear";
        } else if (enemy.symbol == SYM_SNAKE) {
            mobName = "Snake";
        } else if (enemy.symbol == SYM_GHOST) {
            mobName = "Ghost";
        } else if (enemy.symbol == SYM_CRAB) {
            mobName = "Crab";
        } else {
            mobName = "Rat";
        }
            nearbyEnemies.push_back({&enemy, mobName});
        }
    }
    
    // Выводим мобов (каждый на своей строке)
    for (const auto& pair : nearbyEnemies) {
        if (nearbyY >= bottomPanelY) break; // Не выходим за нижнюю панель
        
        const Entity& enemy = *pair.first;
        const std::string& mobName = pair.second;
        std::string direction = getDirectionString(enemy.pos.x, enemy.pos.y, player.pos.x, player.pos.y);
        
        // Название моба его цветом
        try {
            tcod::print(console, {0, nearbyY}, mobName.c_str(), enemy.color, std::nullopt);
        } catch (const std::exception&) {}
        
        int textX = static_cast<int>(mobName.size());

        // Направление
        try {
            tcod::print(console, {textX, nearbyY}, direction.c_str(), white, std::nullopt);
        } catch (const std::exception&) {}
        textX += static_cast<int>(direction.size());

        // Здоровье
        snprintf(buffer, sizeof(buffer), " %d/%d", enemy.health, enemy.maxHealth);
        const float enemyPct = static_cast<float>(enemy.health) / static_cast<float>(enemy.maxHealth);
        const tcod::ColorRGB enemyHpColor = lerpColor(
            tcod::ColorRGB{80, 120, 255},
            tcod::ColorRGB{255, 50, 50},
            1.0f - std::clamp(enemyPct, 0.0f, 1.0f));
        try {
            tcod::print(console, {textX, nearbyY}, buffer, enemyHpColor, std::nullopt);
        } catch (const std::exception&) {}

        nearbyY++;
    }
    
    // === ПРАВАЯ БОКОВАЯ ПАНЕЛЬ: Legend ===
    const tcod::ColorRGB legendLabelColor{200, 200, 200};
    
    // Legend расположен так же, как Nearby: тире на y=1, текст на y=2, тире на y=3, список на y=4+
    // Максимальное HP уже на y=0, поэтому Legend начинается ниже
    int legendTopY = 1; // Верхняя тире на y=1 (как у Nearby)
    for (int x = 0; x < rightPanelWidth; ++x) {
        if (console.in_bounds({rightPanelStartX + x, legendTopY})) {
            console.at({rightPanelStartX + x, legendTopY}).ch = '-';
            console.at({rightPanelStartX + x, legendTopY}).fg = legendLabelColor;
            console.at({rightPanelStartX + x, legendTopY}).bg = black;
        }
    }
    int legendLabelY = legendTopY + 1; // y=2
    int legendLabelX = rightPanelStartX + (rightPanelWidth - 6) / 2;
    try { tcod::print(console, {legendLabelX, legendLabelY}, "Legend", legendLabelColor, std::nullopt); } catch (const std::exception&) {}
    int legendLineBelowLabelY = legendLabelY + 1; // y=3
    for (int x = 0; x < rightPanelWidth; ++x) {
        if (console.in_bounds({rightPanelStartX + x, legendLineBelowLabelY})) {
            console.at({rightPanelStartX + x, legendLineBelowLabelY}).ch = '-';
            console.at({rightPanelStartX + x, legendLineBelowLabelY}).fg = legendLabelColor;
            console.at({rightPanelStartX + x, legendLineBelowLabelY}).bg = black;
        }
    }
    int legendY = legendLineBelowLabelY + 1; // y=4, список начинается отсюда

    // Функция для вывода элемента легенды (строго символ в одну колонку, — и текст, ровно)
    auto printLegendEntry = [&](char symbol, const std::string& text, const tcod::ColorRGB& symColor, bool isKnown) {
        if (legendY >= bottomPanelY) return;
        int entryX = rightPanelStartX;
        // Символ ровно по левому краю панели
        if (console.in_bounds({entryX, legendY})) {
            char drawSym = isKnown ? symbol : '?';
            console.at({entryX, legendY}).ch = drawSym;
            console.at({entryX, legendY}).fg = symColor;
            console.at({entryX, legendY}).bg = black;
        }
        // ' - ' и текст, строго после символа ровно, без попытки центрирования
        try {
            std::string rest = " - " + (isKnown ? text : std::string("?"));
            tcod::print(console, {entryX + 1, legendY}, rest.c_str(), legendLabelColor, std::nullopt);
        } catch (const std::exception&) {}
        legendY++;
    };

    
    // Элементы легенды - Мобы
    printLegendEntry('@', "Hero", tcod::ColorRGB{100, 200, 255}, true);
    printLegendEntry('r', "Rat", tcod::ColorRGB{255, 50, 50}, seenRat);
    printLegendEntry('B', "Bear", tcod::ColorRGB{139, 69, 19}, seenBear);
    printLegendEntry('S', "Snake", tcod::ColorRGB{60, 130, 60}, seenSnake);
    printLegendEntry('g', "Ghost", tcod::ColorRGB{170, 170, 170}, seenGhost);
    printLegendEntry('C', "Crab", tcod::ColorRGB{255, 140, 0}, seenCrab);
    
    // Строка тире после мобов
    for (int x = 0; x < rightPanelWidth; ++x) {
        if (console.in_bounds({rightPanelStartX + x, legendY})) {
            console.at({rightPanelStartX + x, legendY}).ch = '-';
            console.at({rightPanelStartX + x, legendY}).fg = legendLabelColor;
            console.at({rightPanelStartX + x, legendY}).bg = black;
        }
    }
    legendY++;
    
    // Предметы (не квесты!)
    printLegendEntry('$', "Medkit", tcod::ColorRGB{255, 255, 0}, seenMedkit);
    printLegendEntry('+', "Max HP", tcod::ColorRGB{0, 204, 0}, seenMaxHP);
    printLegendEntry('O', "Shield", tcod::ColorRGB{255, 255, 255}, seenShield);
    
    // Строка тире после предметов
    for (int x = 0; x < rightPanelWidth; ++x) {
        if (console.in_bounds({rightPanelStartX + x, legendY})) {
            console.at({rightPanelStartX + x, legendY}).ch = '-';
            console.at({rightPanelStartX + x, legendY}).fg = legendLabelColor;
            console.at({rightPanelStartX + x, legendY}).bg = black;
        }
    }
    legendY++;
    
    // Другие элементы (не мобы и не предметы)
    printLegendEntry('#', "Stair", tcod::ColorRGB{200, 200, 200}, true);
    printLegendEntry('.', "Trap", tcod::ColorRGB{40, 40, 40}, seenTrap);
    
    // === НИЖНЯЯ ПАНЕЛЬ ===
    const tcod::ColorRGB bottomPanelColor{200, 200, 200};
    
    // Слева: управление в виде столбца (сверху вниз)
    bool isPlayerControlsInverted = false;
    for (const auto& enemy : enemies) {
        if (enemy.symbol == SYM_CRAB && enemy.crabAttachedToPlayer) {
            isPlayerControlsInverted = true;
            break;
        }
    }
    
    // Слева внизу: каждый блок управления в отдельной "рамке" из тире (как на твоём скриншоте)
    int controlsBlockStartY = bottomPanelY - 9; // Блок WASD, QEZC, ESC по 3 строки вниз
    if (controlsBlockStartY < gameAreaStartY) controlsBlockStartY = gameAreaStartY;
    auto printControlBox = [&](int boxTopY, const char* txt) {
        // Верхняя линия
        for (int x = 0; x < leftPanelWidth; ++x) {
            if (console.in_bounds({x, boxTopY})) {
                console.at({x, boxTopY}).ch = '-';
                console.at({x, boxTopY}).fg = bottomPanelColor;
                console.at({x, boxTopY}).bg = black;
            }
        }
        // Надпись по центру
        int len = static_cast<int>(strlen(txt));
        int cx = std::max(0, leftPanelWidth / 2 - len / 2);
        if (console.in_bounds({cx, boxTopY + 1})) {
            try { tcod::print(console, {cx, boxTopY + 1}, txt, bottomPanelColor, std::nullopt); } catch (const std::exception&) {}
        }
        // Нижняя линия
        for (int x = 0; x < leftPanelWidth; ++x) {
            if (console.in_bounds({x, boxTopY + 2})) {
                console.at({x, boxTopY + 2}).ch = '-';
                console.at({x, boxTopY + 2}).fg = bottomPanelColor;
                console.at({x, boxTopY + 2}).bg = black;
            }
        }
    };
    const char* wasdText = isPlayerControlsInverted ? "[?][?][?][?]" : "[W] [A] [S] [D]";
    const char* qezcText = isPlayerControlsInverted ? "[?] [E] [?] [C]" : "[Q] [E] [Z] [C]";
    const char* escText = "[ESC]";
    printControlBox(controlsBlockStartY, wasdText);
    printControlBox(controlsBlockStartY + 3, qezcText);
    printControlBox(controlsBlockStartY + 6, escText);
    
    // Справа внизу: блок Floor оформляем как Legend — линия, подпись, линия, под ней римская цифра
    std::string floorLabel = "Floor";
    std::string floorLevel = toRoman(level);
    // Управляемое положение блока Floor (можно поднимать/опускать весь блок)
    // Делаем так, чтобы нижняя тире совпадала с нижней линией последнего блока управления (ESC).
    int controlBottom = controlsBlockStartY + 8; // ESC блок: controlsBlockStartY + 6, плюс 2 строки (текст + нижняя линия)
    int floorBlockBottomLineY = controlBottom; // если сдвинешь controlsBlockStartY — Floor поедет синхронно
    int floorBlockTop = floorBlockBottomLineY - 4; // 5 строк: линия, текст, линия, римская, линия
    // верхняя линия
    for (int x = 0; x < rightPanelWidth; ++x) {
        if (console.in_bounds({rightPanelStartX + x, floorBlockTop})) {
            console.at({rightPanelStartX + x, floorBlockTop}).ch = '-';
            console.at({rightPanelStartX + x, floorBlockTop}).fg = bottomPanelColor;
            console.at({rightPanelStartX + x, floorBlockTop}).bg = black;
        }
    }
    // подпись Floor по центру
    int floorLabelX = rightPanelStartX + (rightPanelWidth - static_cast<int>(floorLabel.size())) / 2;
    try {
        tcod::print(console, {floorLabelX, floorBlockTop + 1}, floorLabel.c_str(), bottomPanelColor, std::nullopt);
        } catch (const std::exception&) {}
    // линия после подписи Floor
    for (int x = 0; x < rightPanelWidth; ++x) {
        if (console.in_bounds({rightPanelStartX + x, floorBlockTop + 2})) {
            console.at({rightPanelStartX + x, floorBlockTop + 2}).ch = '-';
            console.at({rightPanelStartX + x, floorBlockTop + 2}).fg = bottomPanelColor;
            console.at({rightPanelStartX + x, floorBlockTop + 2}).bg = black;
        }
    }
    // римская цифра по центру ниже блока
    int floorLevelX = rightPanelStartX + (rightPanelWidth - static_cast<int>(floorLevel.size())) / 2;
    try {
        tcod::print(console, {floorLevelX, floorBlockTop + 3}, floorLevel.c_str(), bottomPanelColor, std::nullopt);
        } catch (const std::exception&) {}
    // новая нижняя линия (легко двигается и совпадает с controlBottom)
    for (int x = 0; x < rightPanelWidth; ++x) {
        if (console.in_bounds({rightPanelStartX + x, floorBlockTop + 4})) {
            console.at({rightPanelStartX + x, floorBlockTop + 4}).ch = '-';
            console.at({rightPanelStartX + x, floorBlockTop + 4}).fg = bottomPanelColor;
            console.at({rightPanelStartX + x, floorBlockTop + 4}).bg = black;
        }
    }
    
    // По центру нижней панели: дополнительная информация (щит, квесты)
    int centerX = (gameAreaStartX + gameAreaStartX + Map::WIDTH) / 2;
    std::vector<std::string> centerInfo;
    
    // Не пишем больше текст о щите, только визуальная полоса!
    // if (shieldTurns > 0) {
    //     snprintf(buffer, sizeof(buffer), "Shield: %d", shieldTurns);
    //     centerInfo.push_back(buffer);
    // }
    
    if (questActive && questTarget > 0) {
        snprintf(buffer, sizeof(buffer), "Quest: %d/%d", questKills, questTarget);
        centerInfo.push_back(buffer);
    }
    
    // Выводим информацию по центру нижней панели
    int centerY = bottomPanelY;
    for (const std::string& info : centerInfo) {
        int infoX = centerX - static_cast<int>(info.size()) / 2;
        if (infoX < gameAreaStartX) infoX = gameAreaStartX;
        if (infoX + static_cast<int>(info.size()) > gameAreaStartX + Map::WIDTH) {
            infoX = gameAreaStartX + Map::WIDTH - static_cast<int>(info.size());
        }
        try {
            tcod::print(console, {infoX, centerY}, info.c_str(), bottomPanelColor, std::nullopt);
        } catch (const std::exception&) {}
        centerY++;
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
    // Обрабатываем все события SDL (включая закрытие окна) - НЕБЛОКИРУЮЩИЙ ввод!
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            key = TCODK_ESCAPE;
            return true;
        }
        // Обрабатываем нажатия клавиш через SDL
        if (ev.type == SDL_KEYDOWN) {
            SDL_Keycode sym = ev.key.keysym.sym;
            SDL_Keymod mod = static_cast<SDL_Keymod>(ev.key.keysym.mod);
            
            // F11 для полноэкранного режима
            if (sym == SDLK_F11) {
                key = TCODK_F11;
                return true;
            }
            
            // ESC
            if (sym == SDLK_ESCAPE) {
                key = TCODK_ESCAPE;
                return true;
            }
            
            // Стрелки
            if (sym == SDLK_UP) {
                key = TCODK_UP;
                return true;
            }
            if (sym == SDLK_DOWN) {
                key = TCODK_DOWN;
                return true;
            }
            if (sym == SDLK_LEFT) {
                key = TCODK_LEFT;
                return true;
            }
            if (sym == SDLK_RIGHT) {
                key = TCODK_RIGHT;
                return true;
            }
            
            // Буквы и цифры (только если не зажаты модификаторы)
            if ((mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) == 0) {
                if (sym >= SDLK_a && sym <= SDLK_z) {
                    // SDL использует только строчные коды для букв
                    // Если зажат Shift, преобразуем в заглавную букву
                    if (mod & KMOD_SHIFT) {
                        key = 'A' + (sym - SDLK_a); // Преобразуем в заглавную ('A'-'Z')
                    } else {
                        key = static_cast<int>(sym); // SDLK_a = 97 = 'a', SDLK_b = 98 = 'b' и т.д.
                    }
                    return true;
                }
                // Обрабатываем цифры 0-9 (явно преобразуем в символы для надёжности)
                if (sym >= SDLK_0 && sym <= SDLK_9) {
                    key = '0' + (sym - SDLK_0); // Гарантируем символ '0'-'9'
                    return true;
                }
            }
        }
    }
    
    // Также проверяем через libtcod (на случай если SDL не поймал)
    TCOD_key_t k = TCOD_console_check_for_keypress(TCOD_KEY_PRESSED);
    if (k.vk != TCODK_NONE) {
    if (k.vk == TCODK_F11) {
        key = TCODK_F11;
        return true;
    }
    if (k.c != 0) {
            // Преобразуем символ в нижний регистр для единообразия ('F' -> 'f')
            key = (k.c >= 'A' && k.c <= 'Z') ? (k.c + 32) : k.c;
        return true;
    }
    key = k.vk;
        return true;
    }
    
    return false; // Ничего не нажато - НЕ БЛОКИРУЕМ, просто возвращаем false
}

// Получает позицию мыши на карте. Возвращает true если мышь над игровой областью.
bool Graphics::getMousePosition(int& mapX, int& mapY)
{
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    
    // Конвертируем координаты мыши в координаты консоли через context
    // Используем явное указание типа для устранения неоднозначности
    std::array<double, 2> pixelPos = {static_cast<double>(mouseX), static_cast<double>(mouseY)};
    auto mouseTile = context->pixel_to_tile_coordinates(pixelPos);
    int consoleX = static_cast<int>(mouseTile[0]);
    int consoleY = static_cast<int>(mouseTile[1]);
    
    // Проверяем, что мышь в игровой области (не в UI панелях)
    const int gameAreaStartX = leftPanelWidth;
    const int gameAreaStartY = std::max(topPanelHeight, 2);
    
    if (consoleX >= gameAreaStartX && consoleX < gameAreaStartX + Map::WIDTH &&
        consoleY >= gameAreaStartY && consoleY < gameAreaStartY + Map::HEIGHT) {
        mapX = consoleX - gameAreaStartX;
        mapY = consoleY - gameAreaStartY;
        return true;
    }
    
    return false;
}

// Рисует название справа от символа при наведении мыши
void Graphics::drawHoverName(int mapX, int mapY, const std::string& name, const tcod::ColorRGB& color)
{
    const int gameAreaStartX = leftPanelWidth;
    const int gameAreaStartY = std::max(topPanelHeight, 2);
    
    // Рисуем каждую букву справа от символа (начиная с позиции mapX + 1)
    for (size_t i = 0; i < name.size(); ++i) {
        int screenX = gameAreaStartX + mapX + 1 + static_cast<int>(i);
        int screenY = gameAreaStartY + mapY;
        
        if (console.in_bounds({screenX, screenY})) {
            console.at({screenX, screenY}).ch = name[i];
            console.at({screenX, screenY}).fg = color;
            console.at({screenX, screenY}).bg = tcod::ColorRGB{0, 0, 0};
        }
    }
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
