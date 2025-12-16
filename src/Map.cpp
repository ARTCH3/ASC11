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
    // Сбрасываем FOV массивы при генерации новой карты
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            explored[y][x] = false;
            visible[y][x] = false;
        }
    }
    
    // Простейшая генерация случайной карты
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            // Границы карты — стены
            if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) {
                cells[y][x] = SYM_WALL;
            } else {
                // 80% шанс пола, 20% шанс стены
                if (std::rand() % 100 < 20) {
                    cells[y][x] = SYM_WALL;
                } else {
                    cells[y][x] = SYM_FLOOR;
                }
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
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        exitPos = Position(x, y);
        setCell(x, y, SYM_EXIT);
    }
}

bool Map::isExit(int x, int y) const
{
    return (exitPos.x == x && exitPos.y == y);
}
