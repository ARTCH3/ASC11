cmake_minimum_required(VERSION 3.15)

# 1. Имя проекта
project(Ascii)

# 2. Указываем стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. Автоматически находим ВСЕ .cpp файлы в папке src
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

# 4. СОЗДАЁМ исполняемый файл (эта команда "строит" цель "Roguelike")
add_executable(Ascii ${SOURCE_FILES})

# 5. ТОЛЬКО ТЕПЕРЬ, когда цель создана, мы можем её настраивать:
#    Подключаем папку с заголовками
target_include_directories(Ascii PRIVATE include)

#    Подключаем библиотеку PDCurses
#    ЗАМЕНИТЕ C:/vcpkg на ПУТЬ К ВАШЕМУ vcpkg (где вы запускали install)
# target_include_directories(Ascii PRIVATE "C:/ASCIIproject/ASC11/vcpkg/installed/x64-windows/include")
# target_link_libraries(Ascii PRIVATE "C:/ASCIIproject/ASC11/vcpkg/installed/x64-windows/lib/pdcurses.lib")

find_package(libtcod CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE libtcod::libtcod)

# 6. Копируем папку assets рядом с исполняемым файлом
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})