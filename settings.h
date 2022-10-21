#pragma once
#include "stdafx.h"

namespace Settings {
	namespace menu {
		extern bool b_auto_show;
	}
	namespace esp {
		extern bool b_auav;
		extern bool b_visible;
		extern bool b_visible_only;
		extern bool b_box;
		extern bool b_line;
		extern bool b_skeleton;
		extern bool b_names;
		extern bool b_distance;
		extern bool b_fov;
		extern bool b_crosshair;
		extern bool b_friendly;
		extern bool b_radar;
		extern bool b_health;
		extern bool b_aim_point;
		extern int fov_size;
		extern int max_distance;
		extern int radar_zoom;
	}
	namespace lootEsp {
		extern bool b_cash;
		extern bool b_weapons;
		extern bool b_crates;
		extern bool b_crates2;
		extern bool b_crates3;
		extern bool b_killstreak;
		extern bool b_missions;
		extern bool b_ammo;
		extern bool b_name;
		extern bool b_distance;
		extern int max_dist;
	}
	namespace aimbot {
		extern bool b_lock;
		extern bool b_recoil;
		extern bool b_spread;
		extern bool b_skip_knocked;
		extern bool b_target_bone;
		extern float f_bullet_speed;
		extern float f_min_closest;
		extern float f_speed;
		extern float f_smooth;
		extern int aim_key;
		extern int i_bone;
		extern int i_priority;
	}
	namespace misc {
		extern bool b_fov_scale;
		extern float f_fov_scale;
	}
	namespace colors
	{
		extern utils::color_var White; 
		extern utils::color_var Black; 
		extern utils::color_var Red;
		extern utils::color_var Green;
		extern utils::color_var Blue;
		extern utils::color_var visible_team;
		extern utils::color_var not_visible_team;
		extern utils::color_var visible_enemy;
		extern utils::color_var not_visible_enemy;
		extern utils::color_var radar_bg;
		extern utils::color_var radar_boarder;
		extern utils::color_var crate_color;
		extern utils::color_var crate_color2;
		extern utils::color_var crate_color3;
		extern utils::color_var cash_color;
		extern utils::color_var weapon_color;
		extern utils::color_var mission_color;
		extern utils::color_var killstreak_color;
		extern utils::color_var ammo_color;
		/*extern ImColor White;
		extern ImColor Black;
		extern ImColor Red;
		extern ImColor Green;
		extern ImColor Blue;
		extern ImColor visible_team;
		extern ImColor not_visible_team;
		extern ImColor visible_enemy;
		extern ImColor not_visible_enemy;
		extern ImColor radar_bg;
		extern ImColor radar_boarder;
		extern ImColor crate_color;
		extern ImColor crate_color2;
		extern ImColor crate_color3;
		extern ImColor cash_color;
		extern ImColor weapon_color;
		extern ImColor mission_color;
		extern ImColor killstreak_color;
		extern ImColor ammo_color;*/
	}

	void Save_Settings(std::string path);
	void Load_Settings(std::string path);
	void Auto_Load();
	std::vector<std::string> GetList();

}