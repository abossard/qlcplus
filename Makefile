BUILD_DIR ?= build
CMAKE ?= cmake
JOBS ?= 8
QMLUI ?= ON

APP_TARGET ?= qlcplus-qml
MCP_TARGET ?= qlcplusmcp

.DEFAULT_GOAL := help

.PHONY: help configure reconfigure build app qml mcp target run check test clean distclean dmg mac-install

help:
	@printf "QLC+ build shortcuts\n\n"
	@printf "  make configure         Configure CMake in $(BUILD_DIR)\n"
	@printf "  make build             Build $(APP_TARGET)\n"
	@printf "  make mcp               Build $(MCP_TARGET)\n"
	@printf "  make target TARGET=x   Build any CMake target\n"
	@printf "  make run               Build and run $(APP_TARGET) with debug output\n"
	@printf "  make check             Build the CMake check target\n"
	@printf "  make clean             Clean CMake build outputs\n"
	@printf "  make distclean         Remove $(BUILD_DIR)\n"
	@printf "  make dmg               Build signed/unsigned macOS .dmg (requires QTDIR)\n"
	@printf "  make mac-install       Strip quarantine xattrs from installed QLC+.app (sudo)\n"

configure:
	@mkdir -p "$(BUILD_DIR)"
	@cd "$(BUILD_DIR)" && $(CMAKE) .. -Dqmlui=$(QMLUI)

reconfigure: configure

build: app

app qml: configure
	@cd "$(BUILD_DIR)" && $(CMAKE) --build . --target $(APP_TARGET) -j$(JOBS)

mcp: configure
	@cd "$(BUILD_DIR)" && $(CMAKE) --build . --target $(MCP_TARGET) -j$(JOBS)

target: configure
	@test -n "$(TARGET)" || (printf "Usage: make target TARGET=<cmake-target>\n" >&2; exit 2)
	@cd "$(BUILD_DIR)" && $(CMAKE) --build . --target $(TARGET) -j$(JOBS)

run: app
	@cd "$(BUILD_DIR)" && ./qmlui/$(APP_TARGET) -d

check test: configure
	@cd "$(BUILD_DIR)" && $(CMAKE) --build . --target check -j$(JOBS)

clean:
	@cd "$(BUILD_DIR)" && $(CMAKE) --build . --target clean

distclean:
	@rm -rf "$(BUILD_DIR)"

dmg:
	@set -e; \
	APP_DIR="$${QLCPLUS_MAC_APP_DIR:-$$PWD/dist/QLC+.app}"; \
	export QLCPLUS_MAC_APP_DIR="$$APP_DIR"; \
	BIN_DIR="$$APP_DIR/Contents/MacOS"; \
	QML_DIR="$$APP_DIR/Contents/Resources/qml"; \
	if [ -z "$$QTDIR" ]; then \
		for c in \
			"$$(brew --prefix qt 2>/dev/null)" \
			"$$(brew --prefix qt@6 2>/dev/null)" \
			"$$(brew --prefix qt@5 2>/dev/null)" \
			/opt/homebrew/opt/qt /opt/homebrew/opt/qt@6 /opt/homebrew/opt/qt@5 \
			/usr/local/opt/qt /usr/local/opt/qt@6 /usr/local/opt/qt@5; do \
			if [ -n "$$c" ] && [ -d "$$c/lib/cmake" ]; then \
				QTDIR="$$c"; export QTDIR; \
				printf "Auto-detected QTDIR=%s\n" "$$QTDIR"; \
				break; \
			fi; \
		done; \
		if [ -z "$$QTDIR" ]; then \
			printf "QTDIR not set and no Qt install found. Export QTDIR and retry.\n" >&2; \
			exit 2; \
		fi; \
	fi; \
	if [ -z "$$SIGNATURE" ]; then \
		echo "This build WILL NOT be signed. Export SIGNATURE to sign it."; \
	fi; \
	rm -rf "$$APP_DIR"; \
	rm -f *.dmg rw.*; \
	rm -rf "$(BUILD_DIR)"; \
	mkdir -p "$(BUILD_DIR)"; \
	cd "$(BUILD_DIR)"; \
	CMAKE_OSX_DEPLOYMENT_TARGET=12.0; \
	[ -d "$$QTDIR/lib/cmake/Qt5Core" ] && CMAKE_OSX_DEPLOYMENT_TARGET=10.13; \
	$(CMAKE) -DCMAKE_PREFIX_PATH="$$QTDIR/lib/cmake" \
		-DCMAKE_OSX_DEPLOYMENT_TARGET=$$CMAKE_OSX_DEPLOYMENT_TARGET \
		-Dqmlui=on ..; \
	NUM_CPUS=$$(sysctl -n hw.ncpu 2>/dev/null || echo $(JOBS)); \
	make -j$$NUM_CPUS; \
	make install/fast; \
	cd ..; \
	VERSION=$$(grep -E '^\#define APPVERSION' engine/src/qlcconfig.h | sed -E 's/^\#define APPVERSION[[:space:]]+"([^"]+)".*/\1/' | tr ' ' '-'); \
	echo "Fix non-Qt dependencies..."; \
	platforms/macos/fix_dylib_deps.sh "$$APP_DIR/Contents/Frameworks/libsndfile.1.dylib"; \
	if [ -f "$$BIN_DIR/qlcplus" ]; then \
		platforms/macos/fix_dylib_deps.sh "$$BIN_DIR/qlcplus"; \
		platforms/macos/fix_dylib_deps.sh "$$BIN_DIR/qlcplus-fixtureeditor"; \
	else \
		platforms/macos/fix_dylib_deps.sh "$$BIN_DIR/qlcplus-qml"; \
	fi; \
	echo "Run macdeployqt..."; \
	"$$QTDIR/bin/macdeployqt" "$$APP_DIR" -qmldir=qmlui/qml; \
	rm -rf "$$QML_DIR/QtQuick/Controls/FluentWinUI3" \
		"$$QML_DIR/QtQuick/Controls/Imagine" \
		"$$QML_DIR/QtQuick/Controls/iOS" \
		"$$QML_DIR/QtQuick/Controls/Material" \
		"$$QML_DIR/QtQuick/Controls/Universal" \
		"$$QML_DIR/QtQuick/Particles"; \
	if [ -n "$$SIGNATURE" ]; then \
		echo "Signing binaries..."; \
		ENTITLEMENTS="platforms/macos/qlcplus.entitlements"; \
		find "$$APP_DIR/Contents/Frameworks" -type f | while read f; do \
			codesign --force --sign "$$SIGNATURE" --timestamp "$$f"; \
		done; \
		find "$$APP_DIR/Contents/PlugIns" -type f -name "*.dylib" | while read f; do \
			codesign --force --sign "$$SIGNATURE" --timestamp "$$f"; \
		done; \
		find "$$QML_DIR" -type f -name "*.dylib" | while read f; do \
			codesign --force --sign "$$SIGNATURE" --timestamp "$$f"; \
		done; \
		codesign --sign "$$SIGNATURE" --timestamp --deep --entitlements $$ENTITLEMENTS --options runtime "$$APP_DIR"; \
		if [ -f "$$BIN_DIR/qlcplus" ]; then \
			codesign --force --sign "$$SIGNATURE" --timestamp --entitlements $$ENTITLEMENTS --options runtime "$$BIN_DIR/qlcplus"; \
			codesign --force --sign "$$SIGNATURE" --timestamp --entitlements $$ENTITLEMENTS --options runtime "$$BIN_DIR/qlcplus-launcher"; \
		else \
			codesign --force --sign "$$SIGNATURE" --timestamp --entitlements $$ENTITLEMENTS --options runtime "$$BIN_DIR/qlcplus-qml"; \
		fi; \
	fi; \
	OUTDIR="$$PWD"; \
	cd platforms/macos/dmg; \
	attempt=1; max=12; \
	while :; do \
		hdiutil info | awk '/^\/dev\/disk/ {dev=$$1} /Q Light Controller Plus/ {print dev}' | \
			while read d; do hdiutil detach "$$d" -force >/dev/null 2>&1 || true; done; \
		if ./create-dmg --volname "Q Light Controller Plus $$VERSION" \
			--volicon "$$OUTDIR/resources/icons/qlcplus.icns" \
			--background background.png \
			--window-size 400 300 \
			--window-pos 200 100 \
			--icon-size 64 \
			--icon "QLC+" 0 150 \
			--app-drop-link 200 150 \
			"$$OUTDIR/QLC+_$$VERSION.dmg" \
			"$$APP_DIR"; then \
			break; \
		fi; \
		if [ $$attempt -ge $$max ]; then \
			echo "create-dmg failed after $$max attempts" >&2; exit 1; \
		fi; \
		echo "create-dmg attempt $$attempt failed, retrying..." >&2; \
		attempt=$$((attempt+1)); \
		sleep 2; \
	done; \
	cd "$$OUTDIR"; \
	if [ -n "$$SIGNATURE" ]; then \
		codesign --sign "$$SIGNATURE" --timestamp "$$OUTDIR/QLC+_$$VERSION.dmg"; \
	fi; \
	echo "Created $$OUTDIR/QLC+_$$VERSION.dmg"

APP ?= /Applications/QLC+.app

mac-install:
	@if [ ! -d "$(APP)" ] && [ -d "$$HOME/QLC+.app" ]; then \
		APP_PATH="$$HOME/QLC+.app"; \
	else \
		APP_PATH="$(APP)"; \
	fi; \
	if [ ! -d "$$APP_PATH" ]; then \
		printf "App not found at %s. Override with: make mac-install APP=/path/to/QLC+.app\n" "$$APP_PATH" >&2; \
		exit 2; \
	fi; \
	printf "Removing extended attributes from %s (sudo)...\n" "$$APP_PATH"; \
	sudo /usr/bin/xattr -cr "$$APP_PATH"