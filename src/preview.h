#pragma once

extern GLuint pbo;

std::string currentTimeString();
bool init();
void mainLoop(bool& camchanged);

bool MouseOverImGuiWindow();
void InitImguiData(GuiDataContainer* guiData);