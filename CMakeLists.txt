cmake_minimum_required(VERSION 3.15)

# 1. Имя проекта
project(ASC11)

# 2. Указываем стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. Автоматически находим ВСЕ .cpp файлы в папке src
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

# 4. СОЗДАЁМ исполняемый файл (эта команда "строит" цель "Roguelike")
add_executable(ASC11 ${SOURCE_FILES})
set_target_properties(ASC11 PROPERTIES WIN32_EXECUTABLE TRUE)

# 5. ТОЛЬКО ТЕПЕРЬ, когда цель создана, мы можем её настраивать:
#    Подключаем папку с заголовками
target_include_directories(${PROJECT_NAME} PRIVATE include)

# Ключевое изменение: явный путь к toolchain файлу в C:\vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

# Говорим vcpkg, куда складывать скачанные архивы (папка с правами записи)
set(ENV{VCPKG_DOWNLOADS} "C:/vcpkg/downloads")

find_package(libtcod CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE libtcod::libtcod)

# 6. Копируем папку assets рядом с исполняемым файлом
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})