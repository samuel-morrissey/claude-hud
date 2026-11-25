CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1
BUILD := out

.PHONY: firmware flash test preview clean

firmware:
	pio run

flash:
	pio run -t upload

test: $(BUILD)/test_tracker
	$(BUILD)/test_tracker
	python3 test/test_bridge.py

preview: $(BUILD)/preview
	$(BUILD)/preview | python3 tools/ppm2png.py $(BUILD)/preview.png
	@echo $(BUILD)/preview.png

$(BUILD)/test_tracker: test/test_tracker.cpp src/tracker.cpp src/tracker.h src/render.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ test/test_tracker.cpp src/tracker.cpp

$(BUILD)/preview: tools/preview.cpp src/render.cpp src/render.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ tools/preview.cpp src/render.cpp

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
