CC = clang
CFLAGS = -Wall -Wextra -Wno-unused-parameter -std=c11 \
         -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS

ifeq ($(OS),Windows_NT)
    SHELL = cmd.exe
    .SHELLFLAGS = /C
    EXE = .exe
    COMPILER = .\$(1)$(EXE)
    TEST_ENV = set "JIK_CC=$(CC)"&& set "JIK_TEST_COMPILER=.\$(1)$(EXE)"&&
    PLATFORM = windows
    ARCHIVE_EXT = zip
else
    EXE =
    PLATFORM_CFLAGS = -D_POSIX_C_SOURCE=200809L
    COMPILER = ./$(1)$(EXE)
    TEST_ENV = JIK_CC="$(CC)" JIK_TEST_COMPILER="./$(1)$(EXE)"
    PLATFORM = linux
    ARCHIVE_EXT = tar.gz
endif

SEED_SOURCES := $(wildcard src/seed/*.c)
ARCHIVE = jik-$(VERSION)-$(PLATFORM)-x64.$(ARCHIVE_EXT)
CLEAN_COMPILERS = jik jik-seed jik-stage1 jik-stage2 jik-stage3
CLEAN_EXECUTABLES = $(CLEAN_COMPILERS) $(addsuffix .exe,$(CLEAN_COMPILERS))

.PHONY: all seed test test-seed test-bootstrap-full release release-archive clean help

# Rebuild each time: source edits and changed flags always take effect.
all: seed
	"$(call COMPILER,jik-seed)" build src/jik/main.jik --cc "$(CC)" --ccflags "$(CFLAGS) $(PLATFORM_CFLAGS)" --out jik$(EXE)

seed:
	"$(CC)" $(CFLAGS) $(PLATFORM_CFLAGS) -o jik-seed$(EXE) $(SEED_SOURCES)

test: all
	$(call TEST_ENV,jik) "$(call COMPILER,jik)" run test/jik/tests.jik --cc "$(CC)"

test-seed: seed
	$(call TEST_ENV,jik-seed) "$(call COMPILER,jik-seed)" run test/seed/tests.jik --cc "$(CC)"

test-bootstrap-full: test-seed
	"$(call COMPILER,jik-seed)" build src/jik/main.jik --cc "$(CC)" --release --out jik-stage1$(EXE)
	$(call TEST_ENV,jik-stage1) "$(call COMPILER,jik-stage1)" run test/jik/tests.jik --cc "$(CC)"
	"$(call COMPILER,jik-stage1)" build src/jik/main.jik --cc "$(CC)" --release --out jik-stage2$(EXE)
	$(call TEST_ENV,jik-stage2) "$(call COMPILER,jik-stage2)" run test/jik/tests.jik --cc "$(CC)"
	"$(call COMPILER,jik-stage2)" build src/jik/main.jik --cc "$(CC)" --release --out jik-stage3$(EXE)
	$(call TEST_ENV,jik-stage3) "$(call COMPILER,jik-stage3)" run test/jik/tests.jik --cc "$(CC)"
	"$(call COMPILER,jik-stage3)" run scripts/compare-bootstrap.jik --cc "$(CC)"

# Package the exact stage-2 executable verified by the full bootstrap.
release: test-bootstrap-full
ifeq ($(OS),Windows_NT)
	@if not exist release mkdir release
	@copy /Y jik-stage2$(EXE) release\jik$(EXE) >nul
	@copy /Y LICENSE release\ >nul
	@for %%d in (jiklib support docs examples) do @if exist "release\%%d" rmdir /S /Q "release\%%d"
	@xcopy /E /I /Y jiklib release\jiklib\ >nul
	@xcopy /E /I /Y support release\support\ >nul
	@xcopy /E /I /Y docs release\docs\ >nul
	@xcopy /E /I /Y examples release\examples\ >nul
else
	mkdir -p release
	cp jik-stage2$(EXE) release/jik$(EXE)
	cp LICENSE release/
	rm -rf release/jiklib release/support release/docs release/examples
	cp -R jiklib support docs examples release/
endif

release-archive:
ifndef VERSION
	$(error VERSION is required. Use make release-archive VERSION=v0.1.0-beta.0)
else
release-archive: release
ifeq ($(OS),Windows_NT)
	@if not exist artifacts mkdir artifacts
	@powershell -NoProfile -Command "Compress-Archive -Path 'release\*' -DestinationPath 'artifacts\$(ARCHIVE)' -Force"
else
	mkdir -p artifacts
	tar -C release -czf "artifacts/$(ARCHIVE)" .
endif
	@echo Created artifacts/$(ARCHIVE)
endif

clean:
ifeq ($(OS),Windows_NT)
	@for %%f in ($(CLEAN_EXECUTABLES)) do @if exist "%%f" del /F /Q "%%f"
	@for %%d in (build release artifacts) do @if exist "%%d" rmdir /S /Q "%%d"
else
	rm -f $(CLEAN_EXECUTABLES)
	rm -rf build release artifacts
endif

help:
	@echo make - Build the development compiler as jik$(EXE)
	@echo make test - Build and run compiler/language tests
	@echo make seed - Build only the C seed
	@echo make test-seed - Build and test the C seed
	@echo make test-bootstrap-full - Test the seed and three optimized stages
	@echo make release - Verify all stages and stage release/jik$(EXE)
	@echo make release-archive VERSION=... - Verify and create an archive in artifacts/
	@echo make clean - Remove generated build and release outputs
	@echo Use CC=gcc or CC=clang to select the C compiler.
	@echo See docs/development.md for build and release details.
