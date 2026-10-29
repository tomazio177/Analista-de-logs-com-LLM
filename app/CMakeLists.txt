cmake_minimum_required(VERSION 3.21)
project(AnalistaApp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt 6.4 ou mais recente.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Test)

# A lógica da app (falar com o motor, limpar texto, desenhar a janela) fica numa biblioteca,
# para a app e os testes usarem exatamente o mesmo código.
add_library(analista_nucleo STATIC
    src/motor.cpp
    src/motor.h
    src/texto.cpp
    src/texto.h
    src/janela.cpp
    src/janela.h
    src/chave.cpp
    src/chave.h
    src/administrador.cpp
    src/administrador.h
)
target_include_directories(analista_nucleo PUBLIC src)
target_link_libraries(analista_nucleo PUBLIC Qt6::Widgets)
if(WIN32)
    # ShellExecuteEx (pedir administrador) e a leitura do token do processo.
    target_link_libraries(analista_nucleo PUBLIC shell32 advapi32)
endif()
if(MSVC)
    target_compile_options(analista_nucleo PRIVATE /W4)
else()
    target_compile_options(analista_nucleo PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(analista WIN32 src/main.cpp)
target_link_libraries(analista PRIVATE analista_nucleo)

enable_testing()
qt_add_executable(testes_app tests/testes.cpp)
target_link_libraries(testes_app PRIVATE analista_nucleo Qt6::Test)
# Os testes desenham janelas num ecrã virtual e falam com o motor a sério (Node), numa base de dados temporária.
add_test(NAME testes_app COMMAND testes_app)
set_tests_properties(testes_app PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ANALISTA_MOTOR=${CMAKE_CURRENT_SOURCE_DIR}/.."
    TIMEOUT 120)
