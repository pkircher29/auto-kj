# Rotation Fairness Engine Tests
#
# ── Integration ──────────────────────────────────────────────────────────
# Option A: Add to the top-level CMakeLists.txt inside the existing
#           if (AUTOKJ_ENABLE_QT_TESTS) block:
#
#   # --- after the existing test targets ---
#   add_subdirectory(tests/cpp)
#
# Option B: Copy the add_executable / target_link_libraries / add_test
#           block below directly into the parent CMakeLists.txt.
#
# The test links against production source files because
# RotationFairnessEngine has deep transitive dependencies (Settings,
# TableModelRotation, okjtypes, spdlog) that are not easily mocked
# without a larger refactor.

# ── Source files (paths relative to project root) ────────────────────────
set(FAIRNESS_TEST_SOURCES
        ${CMAKE_SOURCE_DIR}/tests/cpp/test_rotation_fairness.cpp

        # Core fairness logic
        ${CMAKE_SOURCE_DIR}/src/rotationfairnessengine.cpp
        ${CMAKE_SOURCE_DIR}/src/rotationfairnessengine.h
        ${CMAKE_SOURCE_DIR}/src/songcanonicalizer.cpp
        ${CMAKE_SOURCE_DIR}/src/songcanonicalizer.h

        # Rotation model (used by fairness engine)
        ${CMAKE_SOURCE_DIR}/src/models/tablemodelrotation.cpp
        ${CMAKE_SOURCE_DIR}/src/models/tablemodelrotation.h

        # Settings (fairness flags, DJ keys)
        ${CMAKE_SOURCE_DIR}/src/settings.cpp
        ${CMAKE_SOURCE_DIR}/src/settings.h
        ${CMAKE_SOURCE_DIR}/src/okjmigration.h

        # Credential storage (pulled in by Settings)
        ${CMAKE_SOURCE_DIR}/src/securecredentialstore.cpp
        ${CMAKE_SOURCE_DIR}/src/securecredentialstore.h

        # Encryption used by Settings / credential storage
        ${CMAKE_SOURCE_DIR}/src/simplecrypt.cpp
        ${CMAKE_SOURCE_DIR}/src/simplecrypt.h

        # Shared types
        ${CMAKE_SOURCE_DIR}/src/okjtypes.cpp
        ${CMAKE_SOURCE_DIR}/src/okjtypes.h
        ${CMAKE_SOURCE_DIR}/src/okjutil.h
)

add_executable(autokj-rotation-fairness-tests
        ${FAIRNESS_TEST_SOURCES}
)

target_include_directories(autokj-rotation-fairness-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/models
)

target_link_libraries(autokj-rotation-fairness-tests
        spdlog
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Svg
        Qt${QT_VERSION_MAJOR}::Test
)

if (TARGET qt5keychain)
    target_link_libraries(autokj-rotation-fairness-tests qt5keychain)
    target_compile_definitions(autokj-rotation-fairness-tests PRIVATE HAVE_QTKEYCHAIN)
endif ()

add_test(NAME autokj-rotation-fairness-tests COMMAND autokj-rotation-fairness-tests)