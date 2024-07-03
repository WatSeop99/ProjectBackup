#pragma once

struct Keyboard
{
	bool bPressed[256];
};
struct Mouse
{
	bool bMouseLeftButton;
	bool bMouseRightButton;
	bool bMouseDragStartFlag;
};
