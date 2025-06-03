SLUG = NeuralFM
VERSION = 2.0.0
SOURCES += src/NeuralFM.cpp
DISTRIBUTABLES += res plugin.json
RACK_DIR = ../../Rack-SDK
include $(RACK_DIR)/plugin.mk

.PHONY: clear build deploy run

clear:
	@echo "Cleaning..."
	@make -s clean
	@echo "Cleaned. ❇️"

build:
	@echo "Building..."
	@make -s all
	@echo "Built. 🎉"

deploy:
	@echo "Copying plugin.dylib..."
	@cp plugin.dylib "/Users/aurelien/Library/Application Support/Rack2/plugins-mac-arm64/NeuralFM/"
	@echo "Copied. ✨"

run: clear build deploy
	@echo "Starting VCV Rack..."
	@cd ../../ && ./Rack &
	@echo "⚡️ VCV Rack is running. ⚡️"
