# ------------------------------------------------------------------------------
# CMake to build SLADE in Windows/MSVC (using vcpkg for dependencies)
# ------------------------------------------------------------------------------

# Build Settings ---------------------------------------------------------------

# Static linking
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Enable big objects
add_compile_options(/bigobj)

# Define a SLADE_DEBUG macro
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSLADE_DEBUG")


# Dependencies -----------------------------------------------------------------

# wxWidgets
if (NOT BUILD_WX)
	find_package(wxWidgets CONFIG REQUIRED)
endif ()
set(WX_LIBS wx::core wx::base wx::stc wx::aui wx::gl wx::propgrid wx::net)

# FreeType
find_package(Freetype REQUIRED)

# Lua
if (NOT NO_LUA)
	find_package(Lua REQUIRED)
endif ()

# FluidSynth
if (NOT NO_FLUIDSYNTH)
	find_package(FluidSynth CONFIG REQUIRED)
endif ()

# Other
find_package(freeimage CONFIG REQUIRED)
find_package(MPG123 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SFML COMPONENTS system audio window network CONFIG REQUIRED)
find_package(glm REQUIRED)


# Include Search Paths ---------------------------------------------------------

include_directories(
	${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
	.
	..
	../thirdparty/glad/include
	./Application
)


# Sources ----------------------------------------------------------------------

# Extra MSVC-specific files to build
set(SLADE_SOURCES ${SLADE_SOURCES} "../msvc/SLADE.rc" "../msvc/SLADE.manifest")

# External libraries are compiled separately to enable unity builds
add_subdirectory(../thirdparty external)


# Build ------------------------------------------------------------------------

add_executable(slade WIN32
	${SLADE_SOURCES}
	${SLADE_HEADERS}
)

if(NOT SLADE_EXE_NAME)
set(SLADE_EXE_NAME SLADE)
endif()

if(NOT SLADE_EXE_DIR)
set(SLADE_EXE_DIR dist)
endif()

# Properties
set_target_properties(slade
	PROPERTIES
	LINK_FLAGS "/subsystem:windows"
	OUTPUT_NAME "${SLADE_EXE_NAME}"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${SLADE_EXE_DIR}"
)

# Precompiled Header
target_precompile_headers(slade PRIVATE "Application/Main.h")

# Link
target_link_libraries(slade
	${BZIP2_LIBRARIES}
	${EXTERNAL_LIBRARIES}
	${FREETYPE_LIBRARIES}
	${OPENGL_LIBRARIES}
	${WX_LIBS}
	${ZLIB_LIBRARY}
	freeimage::FreeImage
	MPG123::libmpg123
	sfml-audio
	sfml-main
	sfml-network
	sfml-window
	glm::glm
)

if (NOT NO_LUA)
	target_link_libraries(slade ${LUA_LIBRARIES})
endif ()

if (NOT NO_FLUIDSYNTH)
	target_link_libraries(slade FluidSynth::libfluidsynth)
endif ()
