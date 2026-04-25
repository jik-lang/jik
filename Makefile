CC = clang
PYTHON ?= python
CFLAGS = -Wall -Wextra -Wno-unused-parameter -std=c11 \
         -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS
RELEASE_CFLAGS = -O3
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g
# IMPORTANT: use this to check for memory leaks
#		gcc -g -O0 -o tt tests.c && valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all --track-fds=yes ./tt


ifeq ($(OS),Windows_NT)
    SHELL = cmd.exe
    .SHELLFLAGS = /C
    RM = -del /F /Q
    RMDIR = -rmdir /S /Q
    MKDIR = mkdir
    EXE = .exe
	RUN = .\$(TARGET)
	CP = copy /Y
	CPDIR = xcopy /E /I /Y
	PLATFORM = windows
	ARCHIVE_EXT = zip
else
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    EXE =
	RUN = ./$(TARGET)
	CP = cp -f
	CPDIR = cp -R
	PLATFORM = linux
	ARCHIVE_EXT = tar.gz
endif


TARGET      = jik$(EXE)

OBJ_DIR  = obj
SRC_DIR  = src/jik

BIN_DIR = release
ARTIFACT_DIR = artifacts
ARCH = x64

TEST_DIR   = test/c
TEST_SRCS  := $(notdir $(wildcard $(TEST_DIR)/*.c))
TEST_OBJS  := $(patsubst %.c,$(OBJ_DIR)/test_%.o,$(TEST_SRCS))
TEST_TARGET = testlang$(EXE)


SRCS      := $(notdir $(wildcard $(SRC_DIR)/*.c))
OBJS      := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
CORE_OBJS := $(filter-out $(OBJ_DIR)/jik.o,$(OBJS))

DEPS = $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)
ARCHIVE_BASENAME = jik-$(VERSION)-$(PLATFORM)-$(ARCH)

.PHONY: all run test clean ensuredirs testlang release release-archive force-version

all: ensuredirs $(TARGET)

ensuredirs: $(OBJ_DIR)
$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(TARGET): $(OBJ_DIR)/jik.o $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(OBJ_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -Isrc -MMD -c $< -o $@

-include $(DEPS)

run: $(TARGET)
	$(RUN)

test:
	$(RUN) run test/jik/tests.jik

testlang: ensuredirs $(TEST_TARGET)

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^


clean:
	$(RM) $(OBJ_DIR)/* $(TARGET) $(TEST_TARGET)
	$(RMDIR) $(OBJ_DIR)
	$(RMDIR) $(BIN_DIR)
	$(RMDIR) $(ARTIFACT_DIR)

# --- Release builds

release:
	@"$(MAKE)" CFLAGS="$(CFLAGS) $(RELEASE_CFLAGS)" all
ifeq ($(OS),Windows_NT)
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
	@copy /Y "$(TARGET)" "$(BIN_DIR)\"
	@copy /Y "LICENSE" "$(BIN_DIR)\"
	@rmdir /S /Q "$(BIN_DIR)\jiklib" 2>nul || exit /B 0
	@rmdir /S /Q "$(BIN_DIR)\support" 2>nul || exit /B 0
	@rmdir /S /Q "$(BIN_DIR)\docs" 2>nul || exit /B 0
	@xcopy /E /I /Y "jiklib"  "$(BIN_DIR)\jiklib\"  >nul
	@xcopy /E /I /Y "support" "$(BIN_DIR)\support\" >nul
	@xcopy /E /I /Y "docs"    "$(BIN_DIR)\docs\"    >nul
else
	@mkdir -p "$(BIN_DIR)"
	@cp -f "$(TARGET)" "$(BIN_DIR)/"
	@cp -f "LICENSE" "$(BIN_DIR)/"
	@rm -rf "$(BIN_DIR)/jiklib" "$(BIN_DIR)/support" "$(BIN_DIR)/docs"
	@cp -R "jiklib"  "$(BIN_DIR)/"
	@cp -R "support" "$(BIN_DIR)/"
	@cp -R "docs"    "$(BIN_DIR)/"
endif

release-archive: $(if $(strip $(VERSION)),,force-version) release
ifeq ($(OS),Windows_NT)
	@if not exist "$(ARTIFACT_DIR)" mkdir "$(ARTIFACT_DIR)"
	@powershell -NoProfile -Command "if (Test-Path '$(ARTIFACT_DIR)\$(ARCHIVE_BASENAME).$(ARCHIVE_EXT)') { Remove-Item -Force '$(ARTIFACT_DIR)\$(ARCHIVE_BASENAME).$(ARCHIVE_EXT)' }; Compress-Archive -Path '$(BIN_DIR)\*' -DestinationPath '$(ARTIFACT_DIR)\$(ARCHIVE_BASENAME).$(ARCHIVE_EXT)' -Force"
	@echo Created $(ARTIFACT_DIR)\$(ARCHIVE_BASENAME).$(ARCHIVE_EXT)
else
	@mkdir -p "$(ARTIFACT_DIR)"
	@tar -C "$(BIN_DIR)" -czf "$(ARTIFACT_DIR)/$(ARCHIVE_BASENAME).$(ARCHIVE_EXT)" .
	@echo Created $(ARTIFACT_DIR)/$(ARCHIVE_BASENAME).$(ARCHIVE_EXT)
endif

force-version:
	$(error VERSION is required. Use make release-archive VERSION=v0.1.0)
