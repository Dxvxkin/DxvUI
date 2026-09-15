# Обёртка find_package(Freetype) для случая, когда freetype собрана из исходников
# через FetchContent (см. cmake/deps.cmake). Редиректит на таргет `freetype`,
# чтобы выдать SDL2_ttf ожидаемый импортируемый таргет Freetype::Freetype.
#
# Активируется только когда наше дерево уже содержит таргет `freetype`
# (фетчнутый на верхнем уровне). В остальных случаях честно работаем как
# стандартный FindFreetype: ищем системный пакет.
if(TARGET freetype AND NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype INTERFACE IMPORTED)
    set_target_properties(Freetype::Freetype PROPERTIES
            INTERFACE_LINK_LIBRARIES freetype
            INTERFACE_INCLUDE_DIRECTORIES "${freetype_SOURCE_DIR}/include")
    set(Freetype_FOUND TRUE)
    return()
endif()

# Не наш случай — прибегаем к штатному поиску системы.
find_path(FREETYPE_INCLUDE_DIR_FT2BUILD freetype/config/ftheader.h)
find_path(FREETYPE_INCLUDE_DIR_FREETYPE2 freetype/freetype.h)
find_library(FREETYPE_LIBRARY NAMES freetype libfreetype freetype219)

if(FREETYPE_LIBRARY AND FREETYPE_INCLUDE_DIR_FREETYPE2)
    set(FREETYPE_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIR_FREETYPE2}" "${FREETYPE_INCLUDE_DIR_FT2BUILD}")
    set(FREETYPE_LIBRARIES "${FREETYPE_LIBRARY}")
    set(Freetype_FOUND TRUE)
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype UNKNOWN IMPORTED)
        set_target_properties(Freetype::Freetype PROPERTIES
                IMPORTED_LOCATION "${FREETYPE_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}")
    endif()
else()
    set(Freetype_FOUND FALSE)
endif()