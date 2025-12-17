#include "Map.h"

#include <cstdlib>
#include <ctime>

Map::Map()
    : fovMap(WIDTH, HEIGHT),
      exitPos(-1, -1) // Выход пока не установлен
{
    // Инициализируем карту пустыми клетками-полом и флаги FOV.
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            cells[y][x] = SYM_FLOOR;
            explored[y][x] = false;
            visible[y][x] = false;
        }
    }
}

Map::~Map() = default;

void Map::generate()
{
    // --- Новый генератор комнат и коридоров для более логичной карты ---
    struct Room {
        int x1, y1, x2, y2;
        int center_x() const { return (x1 + x2) / 2; }
        int center_y() const { return (y1 + y2) / 2; }
    };
    std::vector<Room> rooms;
    // 0. Все клетки делаем стенами
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            cells[y][x] = SYM_WALL;
        }
    }
    // 1. Сначала размещаем большие основные комнаты
    const int minRoomW = 8, minRoomH = 6, maxRoomW = 14, maxRoomH = 10;
    int numBigRooms = 5 + (std::rand() % 4); // 5-8 больших комнат
    int maxAttempts = 200;
    for (int n = 0; n < numBigRooms; ++n) {
        bool placed = false;
        for (int attempt = 0; attempt < maxAttempts && !placed; ++attempt) {
            int w = minRoomW + std::rand() % (maxRoomW - minRoomW + 1);
            int h = minRoomH + std::rand() % (maxRoomH - minRoomH + 1);
            int x = 2 + std::rand() % (WIDTH - w - 3);
            int y = 2 + std::rand() % (HEIGHT - h - 3);
            Room new_room {x, y, x + w - 1, y + h - 1};
            bool failed = false;
            for (const Room& other : rooms) {
                if (new_room.x1 <= other.x2 + 2 && new_room.x2 + 2 >= other.x1 &&
                    new_room.y1 <= other.y2 + 2 && new_room.y2 + 2 >= other.y1) {
                    failed = true;
                    break;
                }
            }
            if (!failed) {
                // Рисуем комнату: внутри всё пол
                for (int yy = new_room.y1; yy <= new_room.y2; ++yy)
                    for (int xx = new_room.x1; xx <= new_room.x2; ++xx)
                        cells[yy][xx] = SYM_FLOOR;
                rooms.push_back(new_room);
                placed = true;
            }
        }
    }
    // 2. Добавляем много маленьких комнат (3-5x3-5) для разнообразия
    const int minSmallW = 3, minSmallH = 3, maxSmallW = 5, maxSmallH = 5;
    int numSmallRooms = 10 + (std::rand() % 10); // 10-19 маленьких комнат
    for (int n = 0; n < numSmallRooms; ++n) {
        bool placed = false;
        for (int attempt = 0; attempt < 100 && !placed; ++attempt) {
            int w = minSmallW + std::rand() % (maxSmallW - minSmallW + 1);
            int h = minSmallH + std::rand() % (maxSmallH - minSmallH + 1);
            int x = 1 + std::rand() % (WIDTH - w - 2);
            int y = 1 + std::rand() % (HEIGHT - h - 2);
            Room new_room {x, y, x + w - 1, y + h - 1};
            bool failed = false;
            for (const Room& other : rooms) {
                if (new_room.x1 <= other.x2 + 1 && new_room.x2 + 1 >= other.x1 &&
                    new_room.y1 <= other.y2 + 1 && new_room.y2 + 1 >= other.y1) {
                    failed = true;
                    break;
                }
            }
            if (!failed) {
                // Рисуем маленькую комнату
                for (int yy = new_room.y1; yy <= new_room.y2; ++yy)
                    for (int xx = new_room.x1; xx <= new_room.x2; ++xx)
                        cells[yy][xx] = SYM_FLOOR;
                rooms.push_back(new_room);
                placed = true;
            }
        }
    }
    // 3. Добавляем преграды и разрушенные участки внутри больших комнат
    for (const Room& room : rooms) {
        int roomW = room.x2 - room.x1 + 1;
        int roomH = room.y2 - room.y1 + 1;
        // Только в больших комнатах (ширина или высота >= 8)
        if (roomW >= 8 || roomH >= 8) {
            // 30% шанс добавить преграды (столбы/разрушенные участки)
            if (std::rand() % 100 < 30) {
                int numObstacles = 2 + (std::rand() % 4); // 2-5 преград
                for (int o = 0; o < numObstacles; ++o) {
                    int ox = room.x1 + 2 + std::rand() % (roomW - 4);
                    int oy = room.y1 + 2 + std::rand() % (roomH - 4);
                    // Создаём маленькую преграду (1x1 или 2x2)
                    int obsW = 1 + (std::rand() % 2);
                    int obsH = 1 + (std::rand() % 2);
                    for (int yy = oy; yy < oy + obsH && yy <= room.y2; ++yy) {
                        for (int xx = ox; xx < ox + obsW && xx <= room.x2; ++xx) {
                            // 50% шанс стена, 50% шанс пол (разрушенный участок остаётся проходимым)
                            if (std::rand() % 2 == 0) {
                                cells[yy][xx] = SYM_WALL;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 4. Соединяем ВСЕ комнаты коридорами (короткие сегменты с препятствиями, не длинные!)
    // Сначала соединяем каждую комнату с предыдущей
    for (size_t i = 1; i < rooms.size(); ++i) {
        int prev_cx = rooms[i-1].center_x(), prev_cy = rooms[i-1].center_y();
        int curr_cx = rooms[i].center_x(), curr_cy = rooms[i].center_y();

        // Новый универсальный короткий, разрушенный, интересный переход с боковыми точками
        int sx = prev_cx, sy = prev_cy;
        while (sx != curr_cx || sy != curr_cy) {
            // 1. Двигаем к цели по одной клетке (случайно меняем приоритет X/Y)
            bool stepX = (sx != curr_cx) && (sy == curr_cy || std::rand() % 2 == 0);
            if (stepX) sx += (curr_cx > sx ? 1 : -1);
            else if (sy != curr_cy) sy += (curr_cy > sy ? 1 : -1);
            cells[sy][sx] = SYM_FLOOR;
            // --- Разруха/карман/боковой разлом ---
            // 70% шанс разбить коридор (боковые дыры или стенки)
            if (std::rand() % 100 < 70) {
                int wallOrHole = std::rand() % 4; // 0-1: дырка, 2-3: боковая стенка
                int dx = (std::rand() % 2 == 0 ? 1 : -1), dy = 0;
                if (std::rand() % 2) std::swap(dx, dy);
                int rx = sx + dx, ry = sy + dy;
                if (rx > 1 && rx < WIDTH-2 && ry > 1 && ry < HEIGHT-2) {
                    if (wallOrHole < 2)
                        cells[ry][rx] = SYM_FLOOR; // боковая дырка
                    else
                        cells[ry][rx] = SYM_WALL;  // нависающая стена
                }
            }
            // 50% шанс добавить сбоку дополнительную мини-комнату
            if (std::rand() % 100 < 50) {
                int bx = sx + ((std::rand()%2) ? 2 : -2); int by = sy + ((std::rand()%2) ? 2 : -2);
                if (bx > 2 && bx < WIDTH-2 && by > 2 && by < HEIGHT-2 && std::rand() % 3 == 0) {
                    int bsize = 1 + std::rand() % 2;
                    for (int xx = bx; xx < bx+bsize; ++xx)
                        for (int yy = by; yy < by+bsize; ++yy)
                            if (xx > 0 && xx < WIDTH && yy > 0 && yy < HEIGHT)
                                cells[yy][xx] = SYM_FLOOR;
                }
            }
            // 20% шанс: зигзаг или поворот (делаем короткий кракозябристый поворот)
            if (std::rand() % 100 < 20) {
                for (int j = 0; j < 2 + std::rand() % 2; ++j) {
                    int zigX = sx + ((std::rand()%2) ? 0 : (std::rand()%2 ? 1 : -1));
                    int zigY = sy + ((std::rand()%2) ? 0 : (std::rand()%2 ? 1 : -1));
                    if (zigX > 1 && zigX < WIDTH-2 && zigY > 1 && zigY < HEIGHT-2)
                        cells[zigY][zigX] = SYM_FLOOR;
                }
            }
            // 15% шанс добавить сбоку тупиковую комнату-ответвление
            if (std::rand() % 100 < 15) {
                int tx = sx + ((std::rand()%2) ? 1 : -1)*2;
                int ty = sy + ((std::rand()%2) ? 1 : -1)*2;
                if (tx > 2 && tx < WIDTH-2 && ty > 2 && ty < HEIGHT-2) {
                    for (int xx = tx-1; xx <= tx+1; ++xx)
                        for (int yy = ty-1; yy <= ty+1; ++yy)
                            if (xx > 0 && xx < WIDTH && yy > 0 && yy < HEIGHT)
                                cells[yy][xx] = SYM_FLOOR;
                }
            }
        }
        // --- Конец нового разрушенного соединителя ---
        continue;
        // Старый код генерации коридоров был удалён.
    }
    // Дополнительно: соединяем некоторые комнаты случайными связями для лучшей проходимости
    for (size_t i = 0; i < rooms.size() && i < 8; ++i) {
        size_t j = std::rand() % rooms.size();
        if (i != j) {
            int cx1 = rooms[i].center_x(), cy1 = rooms[i].center_y();
            int cx2 = rooms[j].center_x(), cy2 = rooms[j].center_y();
            if (std::rand() % 2) {
                for (int x = std::min(cx1, cx2); x <= std::max(cx1, cx2); ++x)
                    cells[cy1][x] = SYM_FLOOR;
                for (int y = std::min(cy1, cy2); y <= std::max(cy1, cy2); ++y)
                    cells[y][cx2] = SYM_FLOOR;
            } else {
                for (int y = std::min(cy1, cy2); y <= std::max(cy1, cy2); ++y)
                    cells[y][cx1] = SYM_FLOOR;
                for (int x = std::min(cx1, cx2); x <= std::max(cx1, cx2); ++x)
                    cells[cy2][x] = SYM_FLOOR;
            }
        }
    }
    
    // 5. Добавляем "внешнюю среду" - разбитые участки карты (как выходы наружу)
    int numBrokenAreas = 3 + (std::rand() % 4); // 3-6 разбитых участков
    for (int b = 0; b < numBrokenAreas; ++b) {
        int bx = 3 + std::rand() % (WIDTH - 6);
        int by = 3 + std::rand() % (HEIGHT - 6);
        int bw = 4 + std::rand() % 6; // 4-9 ширины
        int bh = 4 + std::rand() % 6; // 4-9 высоты
        
        // Создаём разбитую область с препятствиями
        for (int yy = by; yy < by + bh && yy < HEIGHT - 1; ++yy) {
            for (int xx = bx; xx < bx + bw && xx < WIDTH - 1; ++xx) {
                // 60% пол, 40% стена (разрушенная область)
                if (std::rand() % 100 < 60) {
                    cells[yy][xx] = SYM_FLOOR;
                } else {
                    cells[yy][xx] = SYM_WALL;
                }
            }
        }
        // Соединяем разбитую область с ближайшей комнатой
        if (!rooms.empty()) {
            Room nearest = rooms[std::rand() % rooms.size()];
            int nx = nearest.center_x();
            int ny = nearest.center_y();
            int broken_cx = bx + bw / 2;
            int broken_cy = by + bh / 2;
            // Простой коридор к разбитой области
            for (int x = std::min(nx, broken_cx); x <= std::max(nx, broken_cx); ++x)
                cells[ny][x] = SYM_FLOOR;
            for (int y = std::min(ny, broken_cy); y <= std::max(ny, broken_cy); ++y)
                cells[y][broken_cx] = SYM_FLOOR;
        }
    }
    
    // 6. Добавляем МНОГО препятствий и разрухи в комнатах (чтобы пустые комнаты были редкостью)
    for (const Room& room : rooms) {
        int roomW = room.x2 - room.x1 + 1;
        int roomH = room.y2 - room.y1 + 1;
        // В ВСЕХ комнатах добавляем препятствия (даже маленьких)
        // 90% шанс добавить препятствия (раньше было 70%)
        if (std::rand() % 100 < 90) {
            // Больше препятствий в больших комнатах
            int numObstacles = (roomW >= 8 || roomH >= 8) ? (5 + std::rand() % 6) : (3 + std::rand() % 4); // 5-10 или 3-6
            for (int o = 0; o < numObstacles; ++o) {
                int ox = room.x1 + 1 + std::rand() % (roomW - 2);
                int oy = room.y1 + 1 + std::rand() % (roomH - 2);
                // Создаём препятствие (1x1, 2x2, 3x3 или даже 4x4 для больших комнат)
                int maxSize = (roomW >= 8 || roomH >= 8) ? 4 : 3;
                int obsW = 1 + (std::rand() % maxSize);
                int obsH = 1 + (std::rand() % maxSize);
                for (int yy = oy; yy < oy + obsH && yy <= room.y2; ++yy) {
                    for (int xx = ox; xx < ox + obsW && xx <= room.x2; ++xx) {
                        // 75% шанс стена, 25% шанс пол (разрушенный участок - видно что тут была комната)
                        if (std::rand() % 100 < 75) {
                            cells[yy][xx] = SYM_WALL;
                        }
                    }
                }
            }
            // Дополнительно: добавляем разрушенные участки по краям комнат (как обвалившиеся стены)
            if (std::rand() % 100 < 40) {
                int numRubble = 2 + (std::rand() % 4);
                for (int r = 0; r < numRubble; ++r) {
                    // Разрушенные участки по краям
                    int edge = std::rand() % 4; // 0=верх, 1=низ, 2=лево, 3=право
                    int rx, ry;
                    if (edge == 0) { rx = room.x1 + 1 + std::rand() % (roomW - 2); ry = room.y1; }
                    else if (edge == 1) { rx = room.x1 + 1 + std::rand() % (roomW - 2); ry = room.y2; }
                    else if (edge == 2) { rx = room.x1; ry = room.y1 + 1 + std::rand() % (roomH - 2); }
                    else { rx = room.x2; ry = room.y1 + 1 + std::rand() % (roomH - 2); }
                    if (rx > 0 && rx < WIDTH - 1 && ry > 0 && ry < HEIGHT - 1) {
                        // 50% шанс стена (обвалившаяся), 50% пол (разрушенный проход)
                        cells[ry][rx] = (std::rand() % 2 == 0) ? SYM_WALL : SYM_FLOOR;
                    }
                }
            }
        }
    }
    // Сбрасываем FOV массивы при генерации новой карты
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            explored[y][x] = false;
            visible[y][x] = false;
        }
    }
    
    // Границы карты — стены (если они ещё не стены)
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) {
                cells[y][x] = SYM_WALL;
            }
        }
    }
    
    // Обновляем FOV карту на основе стен
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            bool isTransparent = !isWall(x, y);
            bool isWalkable = !isWall(x, y);
            fovMap.setProperties(x, y, isTransparent, isWalkable);
        }
    }

    // Добавим несколько предметов на случайные свободные клетки.
    const int healItemsToSpawn = 3;
    for (int i = 0; i < healItemsToSpawn; ++i) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int rx = std::rand() % WIDTH;
            int ry = std::rand() % HEIGHT;

            if (cells[ry][rx] == SYM_FLOOR) {
                addHealItem(rx, ry, 5); // Восстанавливает 5 здоровья
                break;
            }
        }
    }

    // Добавим предметы, увеличивающие максимум здоровья.
    const int maxHpItemsToSpawn = 2;
    for (int i = 0; i < maxHpItemsToSpawn; ++i) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int rx = std::rand() % WIDTH;
            int ry = std::rand() % HEIGHT;

            if (cells[ry][rx] == SYM_FLOOR) {
                // Бонус от 1 до 5
                int bonus = (std::rand() % 5) + 1;
                addMaxHealthItem(rx, ry, bonus);
                break;
            }
        }
    }
    
    // Добавляем выход на случайную свободную клетку (далеко от начала)
    for (int attempt = 0; attempt < 200; ++attempt) {
        int rx = std::rand() % WIDTH;
        int ry = std::rand() % HEIGHT;
        
        // Выход должен быть на свободной клетке и не слишком близко к началу
        if (cells[ry][rx] == SYM_FLOOR && 
            (rx > WIDTH / 2 || ry > HEIGHT / 2)) {
            addExit(rx, ry);
            break;
        }
    }
}

char Map::getCell(int x, int y) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        // За пределами карты считаем стеной.
        return SYM_WALL;
    }
    return cells[y][x];
}

void Map::setCell(int x, int y, char symbol)
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return;
    }
    cells[y][x] = symbol;
    
    // Обновляем FOV карту
    bool isTransparent = !isWall(x, y);
    bool isWalkable = !isWall(x, y);
    fovMap.setProperties(x, y, isTransparent, isWalkable);
}

bool Map::isWall(int x, int y) const
{
    return getCell(x, y) == SYM_WALL;
}

bool Map::isWalkable(int x, int y) const
{
    // Проходимо, если не стена. Врага мы все равно храним в виде Entity, а не в клетке.
    return !isWall(x, y);
}

bool Map::inBounds(int x, int y) const
{
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

// FOV с использованием TCODMap (как в samples_cpp.cpp)
void Map::computeFOV(int playerX, int playerY, int radius, bool lightWalls)
{
    // Сначала все клетки невидимы
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            visible[y][x] = false;
        }
    }

    // Вычисляем FOV с помощью алгоритма libtcod
    fovMap.computeFov(playerX, playerY, radius, lightWalls, FOV_RESTRICTIVE);
    
    // Отмечаем видимые клетки
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (fovMap.isInFov(x, y)) {
                visible[y][x] = true;
                explored[y][x] = true; // Если видим, то и исследовали
            }
        }
    }
}

void Map::revealAll()
{
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            visible[y][x] = true;
            explored[y][x] = true;
        }
    }
}

bool Map::isVisible(int x, int y) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return false;
    }
    return visible[y][x];
}

bool Map::isExplored(int x, int y) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return false;
    }
    return explored[y][x];
}

void Map::addItem(int x, int y, int healAmount, int maxHealthBoost, char symbol)
{
    items.push_back(Item(x, y, healAmount, maxHealthBoost, symbol));
    cells[y][x] = symbol;
}

void Map::addHealItem(int x, int y, int healAmount)
{
    addItem(x, y, healAmount, 0, SYM_ITEM);
}

void Map::addMaxHealthItem(int x, int y, int maxHealthBoost)
{
    // Если в будущем нужно будет менять символ, достаточно поправить здесь
    addItem(x, y, 0, maxHealthBoost, SYM_MAX_HP);
}

void Map::addTrapItem(int x, int y)
{
    // Ловушка (мина) '.' ничего не лечит и не бустит HP
    addItem(x, y, 0, 0, SYM_TRAP);
}

void Map::addShieldItem(int x, int y)
{
    // Щит: не лечит и не увеличивает максимум HP
    addItem(x, y, 0, 0, SYM_SHIELD);
}

void Map::addQuestItem(int x, int y)
{
    // Квестовый предмет '?'
    addItem(x, y, 0, 0, SYM_QUEST);
}

Item* Map::getItemAt(int x, int y)
{
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].pos.x == x && items[i].pos.y == y) {
            return &items[i];
        }
    }
    return nullptr;
}

void Map::removeItem(int index)
{
    if (index >= 0 && index < static_cast<int>(items.size())) {
        int x = items[index].pos.x;
        int y = items[index].pos.y;
        cells[y][x] = SYM_FLOOR; // Убираем символ предмета
        items.erase(items.begin() + index);
    }
}

void Map::addExit(int x, int y)
{
    // Лестница ставится ТОЛЬКО на пол (SYM_FLOOR), никогда на стену!
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && cells[y][x] == SYM_FLOOR) {
        exitPos = Position(x, y);
        setCell(x, y, SYM_EXIT);
    }
}

bool Map::isExit(int x, int y) const
{
    return (exitPos.x == x && exitPos.y == y);
}
