#pragma once
#include "stdafx.h"


namespace g_menu
{
	extern bool b_menu_open;
	extern std::string str_config_name;

	void Render(ImFont* font);
	void VisualTab();
	void AimbotTab();
	void MiscTab();
	void ColorsTab();
	void ConfigTab();
}