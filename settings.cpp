#include "settings.h"
#include "xor.hpp"
#include "Menu.h"

namespace Settings {

	namespace menu {
		bool b_auto_show = false;
	}
	namespace esp {
		bool b_auav = true;
		bool b_visible = true;
		bool b_visible_only = false;
		bool b_box = false;
		bool b_line = false;
		bool b_skeleton = true;
		bool b_names = true;
		bool b_distance = true;
		bool b_fov = true;
		bool b_crosshair = false;
		bool b_friendly = false;
		bool b_radar = false;
		bool b_health = false;
		bool b_aim_point = false;
		int fov_size = 75; // 5 ~ 800
		int max_distance = 350; //5 ~ 1000
		int radar_zoom = 221;
	}
	namespace lootEsp {
		bool b_cash = true;
		bool b_weapons = true;
		bool b_crates = true;
		bool b_crates2 = false;
		bool b_crates3 = true;
		bool b_killstreak = true;
		bool b_missions = false;
		bool b_ammo = true;
		bool b_name = true;
		bool b_distance = true;
		int max_dist = 500;
	}
	namespace aimbot {
		bool b_lock = true;
		bool b_recoil = false;
		bool b_spread = false;
		bool b_skip_knocked = false;
		bool b_target_bone = true;
		float f_bullet_speed = 2250.5f; // 1 ~ 5000
		float f_min_closest = 10.f; // 5 ~ 50
		float f_speed = 4.5f; // 1 ~ 30
		float f_smooth = 1.5f; //1 ~ 30
		int aim_key = -1;
		int i_bone = 2; //0 ~ 3
		int i_priority = 2; // 0 ~ 2
	}
	namespace misc {
		bool b_fov_scale = false;
		float f_fov_scale = 1.2f; // 1.2 ~ 2.0
	}
	namespace colors
	{

		utils::color_var White = utils::color_var(C_Color(255, 255, 255));
		utils::color_var Black = utils::color_var(C_Color(0, 0, 0));
		utils::color_var Red = utils::color_var(C_Color(255, 0, 0));
		utils::color_var Green = utils::color_var(C_Color(0, 255, 0));
		utils::color_var Blue = utils::color_var(C_Color(0, 0, 255));
		utils::color_var visible_team = utils::color_var(C_Color(255, 255, 255));
		utils::color_var not_visible_team = utils::color_var(C_Color(255, 255, 255));
		utils::color_var visible_enemy = utils::color_var(C_Color(255, 255, 255));
		utils::color_var not_visible_enemy = utils::color_var(C_Color(255, 255, 255));
		utils::color_var radar_bg = utils::color_var(C_Color(255, 255, 255));
		utils::color_var radar_boarder = utils::color_var(C_Color(255, 255, 255));
		utils::color_var crate_color = utils::color_var(C_Color(255, 255, 255));
		utils::color_var crate_color2 = utils::color_var(C_Color(255, 255, 255));
		utils::color_var crate_color3 = utils::color_var(C_Color(255, 255, 255));
		utils::color_var cash_color = utils::color_var(C_Color(255, 255, 255));
		utils::color_var weapon_color = utils::color_var(C_Color(255, 255, 255));
		utils::color_var mission_color = utils::color_var(C_Color(255, 255, 255));
		utils::color_var killstreak_color = utils::color_var(C_Color(255, 255, 255));
		utils::color_var ammo_color = utils::color_var(C_Color(255, 255, 255));
		/*ImColor White = { 1.f,1.f,1.f,1.f };
		ImColor Black = { 0.f,0.f,0.f,1.f };
		ImColor Red = { 1.f,0.f,0.f,1.f };
		ImColor Green = { 0.f,1.f,0.f,1.f };
		ImColor Blue = { 0.f,0.f,1.f,1.f };
		ImColor visible_team = { 0.f, 0.f, 1.f, 1.f };
		ImColor not_visible_team = { 0.f, 0.75f, 1.f, 1.f };
		ImColor visible_enemy = { 1.f, 1.f, 0.f, 1.f };
		ImColor not_visible_enemy = { 1.f,0.f,0.f,1.f };
		ImColor radar_bg = { 1.f,1.f,1.f,0.f };
		ImColor radar_boarder = { 1.f,1.f,1.f,0.f };
		ImColor crate_color = { 1.f,1.f,1.f,1.f };
		ImColor crate_color2 = { 1.f,1.f,1.f,1.f };
		ImColor crate_color3 = { 1.f,1.f,1.f,1.f };
		ImColor cash_color = { 1.f,1.f,1.f,1.f };
		ImColor weapon_color = { 1.f,1.f,1.f,1.f };
		ImColor mission_color = { 1.f,1.f,1.f,1.f };
		ImColor killstreak_color = { 1.f,1.f,1.f,1.f };
		ImColor ammo_color = { 1.f,1.f,1.f,1.f };*/
	}

	BOOL WritePrivateProfileInt(LPCSTR lpAppName, LPCSTR lpKeyName, int nInteger, LPCSTR lpFileName) {
		char lpString[1024];
		sprintf(lpString, "%d", nInteger);
		return WritePrivateProfileStringA(lpAppName, lpKeyName, lpString, lpFileName);
	}
	BOOL WritePrivateProfileFloat(LPCSTR lpAppName, LPCSTR lpKeyName, float nInteger, LPCSTR lpFileName) {
		char lpString[1024];
		sprintf(lpString, "%f", nInteger);
		return WritePrivateProfileStringA(lpAppName, lpKeyName, lpString, lpFileName);
	}
	float GetPrivateProfileFloat(LPCSTR lpAppName, LPCSTR lpKeyName, FLOAT flDefault, LPCSTR lpFileName)
	{
		char szData[32];

		GetPrivateProfileStringA(lpAppName, lpKeyName, std::to_string(flDefault).c_str(), szData, 32, lpFileName);

		return (float)atof(szData);
	}

	void Save_Settings(std::string fileName) {
		char file_path[MAX_PATH];
		sprintf_s(file_path, xorstr("C:\\Limitless\\configs\\%s%s"), fileName.c_str(), xorstr(".ini"));
		
		WritePrivateProfileInt("menu", "b_auto_show", menu::b_auto_show, file_path);
		WritePrivateProfileInt("esp", "b_aim_point", esp::b_aim_point, file_path);
		WritePrivateProfileInt("esp", "b_uav", esp::b_auav, file_path);
		WritePrivateProfileInt("esp", "b_box", esp::b_box, file_path);
		WritePrivateProfileInt("esp", "b_crosshair", esp::b_crosshair, file_path);
		WritePrivateProfileInt("esp", "b_distance", esp::b_distance, file_path);
		WritePrivateProfileInt("esp", "b_fov", esp::b_fov, file_path);
		WritePrivateProfileInt("esp", "b_friendly", esp::b_friendly, file_path);
		WritePrivateProfileInt("esp", "b_health", esp::b_health, file_path);
		WritePrivateProfileInt("esp", "b_line", esp::b_line, file_path);
		WritePrivateProfileInt("esp", "b_names", esp::b_names, file_path);
		WritePrivateProfileInt("esp", "b_radar", esp::b_radar, file_path);
		WritePrivateProfileInt("esp", "b_skeleton", esp::b_skeleton, file_path);
		WritePrivateProfileInt("esp", "b_visible", esp::b_visible, file_path);
		WritePrivateProfileInt("esp", "i_fov_size", esp::fov_size, file_path);
		WritePrivateProfileInt("esp", "i_max_distance", esp::max_distance, file_path);
		WritePrivateProfileInt("esp", "i_radar_zoom", esp::radar_zoom, file_path);
		WritePrivateProfileInt("lootEsp", "b_name", lootEsp::b_name, file_path);
		WritePrivateProfileInt("lootEsp", "b_distance", lootEsp::b_distance, file_path);
		WritePrivateProfileInt("lootEsp", "b_missions", lootEsp::b_name, file_path);
		WritePrivateProfileInt("lootEsp", "b_missions", lootEsp::b_distance, file_path);
		WritePrivateProfileInt("lootEsp", "b_cash", lootEsp::b_cash, file_path);
		WritePrivateProfileInt("lootEsp", "b_weapons", lootEsp::b_weapons, file_path);
		WritePrivateProfileInt("lootEsp", "b_ammo", lootEsp::b_ammo, file_path);
		WritePrivateProfileInt("lootEsp", "b_crates", lootEsp::b_crates, file_path);
		WritePrivateProfileInt("lootEsp", "b_crates2", lootEsp::b_crates2, file_path);
		WritePrivateProfileInt("lootEsp", "b_crates3", lootEsp::b_crates3, file_path);
		WritePrivateProfileInt("lootEsp", "b_killstreak", lootEsp::b_killstreak, file_path);
		WritePrivateProfileInt("lootEsp", "b_missions", lootEsp::b_missions, file_path);
		WritePrivateProfileInt("lootEsp", "i_max_distance", lootEsp::max_dist, file_path);
		WritePrivateProfileInt("aimbot", "b_lock", aimbot::b_lock, file_path);
		WritePrivateProfileInt("aimbot", "b_recoil", aimbot::b_recoil, file_path);
		WritePrivateProfileInt("aimbot", "b_spread", aimbot::b_spread, file_path);
		WritePrivateProfileInt("aimbot", "b_skip_knocked", aimbot::b_skip_knocked, file_path);
		WritePrivateProfileInt("aimbot", "b_target_bone", aimbot::b_target_bone, file_path);
		WritePrivateProfileFloat("aimbot", "f_bullet_speed", aimbot::f_bullet_speed, file_path);
		WritePrivateProfileFloat("aimbot", "f_min_closest", aimbot::f_min_closest, file_path);
		WritePrivateProfileFloat("aimbot", "f_smooth", aimbot::f_smooth, file_path);
		WritePrivateProfileFloat("aimbot", "f_speed", aimbot::f_speed, file_path);
		WritePrivateProfileInt("aimbot", "aim_key", aimbot::aim_key, file_path);
		WritePrivateProfileInt("aimbot", "i_bone", aimbot::i_bone, file_path);
		WritePrivateProfileInt("aimbot", "i_priority", aimbot::i_priority, file_path);
		WritePrivateProfileInt("misc", "b_fov_scale", misc::b_fov_scale, file_path);
		WritePrivateProfileFloat("misc", "f_fov_scale", misc::f_fov_scale, file_path);
		WritePrivateProfileInt("colors", "not_visible_enemy", colors::not_visible_enemy.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "not_visible_team", colors::not_visible_team.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "visible_enemy", colors::visible_enemy.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "visible_team", colors::visible_team.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "radar_bg", colors::radar_bg.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "radar_boarder", colors::radar_boarder.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "ammo_color", colors::ammo_color.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "weapon_color", colors::weapon_color.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "cash_color", colors::cash_color.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "crate_color", colors::crate_color.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "crate_color2", colors::crate_color2.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "crate_color3", colors::crate_color3.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "killstreak_color", colors::killstreak_color.color().GetU32(), file_path);
		WritePrivateProfileInt("colors", "mission_color", colors::mission_color.color().GetU32(), file_path);
		
	}

	void Load_Settings(std::string fileName)
	{
		char file_path[MAX_PATH];
		sprintf_s(file_path, xorstr("C:\\Limitless\\configs\\%s%s"), fileName.c_str(), xorstr(".ini"));
		
		esp::b_aim_point = GetPrivateProfileIntA("esp", "b_aim_point", esp::b_aim_point, file_path);
		esp::b_auav = GetPrivateProfileIntA("esp", "b_uav", esp::b_auav, file_path);
		esp::b_box = GetPrivateProfileIntA("esp", "b_box", esp::b_box, file_path);
		esp::b_crosshair = GetPrivateProfileIntA("esp", "b_crosshair", esp::b_crosshair, file_path);
		esp::b_distance = GetPrivateProfileIntA("esp", "b_distance", esp::b_distance, file_path);
		esp::b_fov = GetPrivateProfileIntA("esp", "b_fov", esp::b_fov, file_path);
		esp::b_friendly = GetPrivateProfileIntA("esp", "b_friendly", esp::b_friendly, file_path);
		esp::b_health = GetPrivateProfileIntA("esp", "b_health", esp::b_health, file_path);
		esp::b_line = GetPrivateProfileIntA("esp", "b_line", esp::b_line, file_path);
		esp::b_names = GetPrivateProfileIntA("esp", "b_names", esp::b_names, file_path);
		esp::b_radar = GetPrivateProfileIntA("esp", "b_radar", esp::b_radar, file_path);
		esp::b_skeleton = GetPrivateProfileIntA("esp", "b_skeleton", esp::b_skeleton, file_path);
		esp::b_visible = GetPrivateProfileIntA("esp", "b_visible", esp::b_visible, file_path);
		esp::fov_size = GetPrivateProfileIntA("esp", "i_fov_size", esp::fov_size, file_path);
		esp::max_distance = GetPrivateProfileIntA("esp", "i_max_distance", esp::max_distance, file_path);
		esp::radar_zoom = GetPrivateProfileIntA("esp", "i_radar_zoom", esp::radar_zoom, file_path);
		lootEsp::b_name = GetPrivateProfileIntA("lootEsp", "b_enable", lootEsp::b_name, file_path);
		lootEsp::b_distance = GetPrivateProfileIntA("lootEsp", "b_enable", lootEsp::b_distance, file_path);
		lootEsp::b_name = GetPrivateProfileIntA("lootEsp", "b_missions", lootEsp::b_name, file_path);
		lootEsp::b_distance = GetPrivateProfileIntA("lootEsp", "b_missions", lootEsp::b_distance, file_path);
		lootEsp::b_cash = GetPrivateProfileIntA("lootEsp", "b_cash", lootEsp::b_cash, file_path);
		lootEsp::b_weapons = GetPrivateProfileIntA("lootEsp", "b_weapons", lootEsp::b_weapons, file_path);
		lootEsp::b_ammo = GetPrivateProfileIntA("lootEsp", "b_ammo", lootEsp::b_ammo, file_path);
		lootEsp::b_crates = GetPrivateProfileIntA("lootEsp", "b_crates", lootEsp::b_crates, file_path);
		lootEsp::b_crates2 = GetPrivateProfileIntA("lootEsp", "b_crates2", lootEsp::b_crates2, file_path);
		lootEsp::b_crates3 = GetPrivateProfileIntA("lootEsp", "b_crates3", lootEsp::b_crates3, file_path);
		lootEsp::b_killstreak = GetPrivateProfileIntA("lootEsp", "b_killstreak", lootEsp::b_killstreak, file_path);
		lootEsp::b_missions = GetPrivateProfileIntA("lootEsp", "b_missions", lootEsp::b_missions, file_path);
		lootEsp::max_dist = GetPrivateProfileIntA("lootEsp", "i_max_distance", lootEsp::max_dist, file_path);
		aimbot::b_lock = GetPrivateProfileIntA("aimbot", "b_lock", aimbot::b_lock, file_path);
		aimbot::b_recoil = GetPrivateProfileIntA("aimbot", "b_recoil", aimbot::b_recoil, file_path);
		aimbot::b_spread = GetPrivateProfileIntA("aimbot", "b_spread", aimbot::b_spread, file_path);
		aimbot::b_skip_knocked = GetPrivateProfileIntA("aimbot", "b_skip_knocked", aimbot::b_skip_knocked, file_path);
		aimbot::b_target_bone = GetPrivateProfileIntA("aimbot", "b_target_bone", aimbot::b_target_bone, file_path);
		aimbot::f_bullet_speed = GetPrivateProfileFloat("aimbot", "f_bullet_speed", aimbot::f_bullet_speed, file_path);
		aimbot::f_min_closest = GetPrivateProfileFloat("aimbot", "f_min_closest", aimbot::f_min_closest, file_path);
		aimbot::f_smooth = GetPrivateProfileFloat("aimbot", "f_smooth", aimbot::f_smooth, file_path);
		aimbot::f_speed = GetPrivateProfileFloat("aimbot", "f_speed", aimbot::f_speed, file_path);
		aimbot::aim_key = GetPrivateProfileIntA("aimbot", "aim_key", aimbot::aim_key, file_path);
		aimbot::i_bone = GetPrivateProfileIntA("aimbot", "i_bone", aimbot::i_bone, file_path);
		aimbot::i_priority = GetPrivateProfileIntA("aimbot", "i_priority", aimbot::i_priority, file_path);
		misc::b_fov_scale = GetPrivateProfileIntA("misc", "b_fov_scale", misc::b_fov_scale, file_path);
		misc::f_fov_scale = GetPrivateProfileFloat("misc", "f_fov_scale", misc::f_fov_scale, file_path);
		colors::not_visible_enemy.col_color = GetPrivateProfileIntA("colors", "not_visible_enemy", colors::not_visible_enemy.color().GetU32(), file_path);
		colors::not_visible_team.col_color = GetPrivateProfileIntA("colors", "not_visible_team", colors::not_visible_team.color().GetU32(), file_path);
		colors::visible_enemy.col_color = GetPrivateProfileIntA("colors", "visible_enemy", colors::visible_enemy.color().GetU32(), file_path);
		colors::visible_team.col_color = GetPrivateProfileIntA("colors", "visible_team", colors::visible_team.color().GetU32(), file_path);
		colors::radar_bg.col_color = GetPrivateProfileIntA("colors", "radar_bg", colors::radar_bg.color().GetU32(), file_path);
		colors::radar_boarder.col_color = GetPrivateProfileIntA("colors", "radar_boarder", colors::radar_boarder.color().GetU32(), file_path);
		colors::ammo_color.col_color = GetPrivateProfileIntA("colors", "ammo_color", colors::ammo_color.color().GetU32(), file_path);
		colors::weapon_color.col_color = GetPrivateProfileIntA("colors", "weapon_color", colors::weapon_color.color().GetU32(), file_path);
		colors::cash_color.col_color = GetPrivateProfileIntA("colors", "cash_color", colors::cash_color.color().GetU32(), file_path);
		colors::crate_color.col_color = GetPrivateProfileIntA("colors", "crate_color", colors::crate_color.color().GetU32(), file_path);
		colors::crate_color2.col_color = GetPrivateProfileIntA("colors", "crate_color2", colors::crate_color2.color().GetU32(), file_path);
		colors::crate_color3.col_color = GetPrivateProfileIntA("colors", "crate_color3", colors::crate_color3.color().GetU32(), file_path);
		colors::killstreak_color.col_color = GetPrivateProfileIntA("colors", "killstreak_color", colors::killstreak_color.color().GetU32(), file_path);
		colors::mission_color.col_color = GetPrivateProfileIntA("colors", "mission_color", colors::mission_color.color().GetU32(), file_path);

		g_menu::str_config_name = fileName;
	};

	std::vector<std::string> GetList() {
		std::vector<std::string> configs;
		WIN32_FIND_DATA ffd;
		LPCSTR directory = xorstr("C:\\Limitless\\Configs\\*");
		auto hFind = FindFirstFile(directory, &ffd);
		while (FindNextFile(hFind, &ffd))
		{
			if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				std::string file_name = ffd.cFileName;
				if (file_name.size() < 4) // .cfg
					continue;
				std::string end = file_name;
				end.erase(end.begin(), end.end() - 4);
				if (end != xorstr(".ini"))
					continue;
				file_name.erase(file_name.end() - 4, file_name.end());
				configs.push_back(file_name);
			}
		}
		return configs;
	}

	void Auto_Load() {
		if (!GetList().empty()) {
			Load_Settings(GetList().at(0));
		} else {
			CreateDirectoryA(xorstr("C:\\Limitless"), NULL);
			CreateDirectoryA(xorstr("C:\\Limitless\\Configs"), NULL);
		}
	}

}