#include "stdafx.h"
#include "esp.h"
#include "sdk.h"
#include "aim.h"
#include "settings.h"
#include "Menu.h"

bool once = false;

bool enable = true;

bool enable_loot = false;
const sdk::loot_definition_t loot_defs[] =
{
	{ "accessory_money", "CASH", &Settings::lootEsp::b_cash },
	{ "loot_wm_armor_plate", "Amor", &enable_loot },
	{ "lm_armor_vest_01", "Satchel", &enable_loot },
	{ "gasmask_a", "Gasmask", &Settings::lootEsp::b_cash },
	{ "offhand_vm_supportbox_armor_br", "Armor Box",&Settings::lootEsp::b_cash},
	{ "offhand_vm_supportbox_br", "Ammo Box", &Settings::lootEsp::b_cash },
	{ "loot_crate_01_br_legendary_01", "CRATE LEGENDARY", &Settings::lootEsp::b_crates },
	{ "military_loot_crate", "CRATE MILITARY", &Settings::lootEsp::b_crates },

	// Tablets
	// Mission
	{ "mission_tablet_01_scavenger", "Mission Scavenger", &Settings::lootEsp::b_missions },
	{ "mission_tablet_01_assassination", "Mission Most_Wanted", &Settings::lootEsp::b_missions },
	/*{ "mission_tablet_01_timedrun", "mission_tablet_01_timedrun", &trr },*/

	// Ammo
	{ "loot_ammo_pistol_smg", "SMG AMMO", &Settings::lootEsp::b_ammo },
	{ "ammo_box_ar", "AR AMMO", &Settings::lootEsp::b_ammo },
	{ "ammo_shotgun", "Shotgun AMMO", &Settings::lootEsp::b_ammo },
	{ "ammo_marksman_sniper", "SNIPER AMMO", &Settings::lootEsp::b_ammo },
	/*{ "ammo_rocket", "ROCKET AMMO", &trr },*/

	//Weapons
	{ "wm_weapon_root_s4_ar", "Assault Rifle", &Settings::lootEsp::b_weapons },
	{ "wm_weapon_root_s4_pi", "Pistol", &Settings::lootEsp::b_weapons },
	{ "wm_weapon_root_s4_sm", "SMG", &Settings::lootEsp::b_weapons },
	{ "wm_weapon_root_s4_sn", "Sniper", &Settings::lootEsp::b_weapons },
	{ "wm_weapon_root_s4_lm", "LMG", &Settings::lootEsp::b_weapons },

	//Equipment
	{ "lm_offhand_wm_grenade_flash", "flash", &enable_loot },
	{ "offhand_vm_grenade_decoy", "decoy", &enable_loot },
	{ "lm_heal_stim_syringe_01", "Stim", &enable_loot },
	{ "offhand_wm_grenade_thermite_br", "Thermite", &enable_loot },
	{ "lm_offhand_wm_grenade_mike67", "Grenade", &enable_loot },
	{ "lm_offhand_wm_grenade_semtex", "Semtex", &enable_loot },
	{ "lm_offhand_wm_grenade_snapshot", "Snapshot", &enable_loot },
	{ "lm_offhand_wm_grenade_concussion", "Concussion", &enable_loot },
	{ "lm_offhand_wm_c4", "C4", &enable_loot },
	{ "weapon_wm_knife_offhand_thrown_electric", "Throwing Knife Electric", &enable_loot },
	{ "m_weapon_wm_knife_offhand_thrown_fire", "Throwing Knife Fire", &enable_loot  },
	{ "lm_offhand_wm_at_mine", "Mine", &enable_loot },
	{ "lm_weapon_wm_knife_offhand_thrown", "Throwing Knife", &enable_loot  },
	{ "lm_offhand_wm_claymore", "Claymore", &enable_loot },

	//Unknown
	/*{"lm_offhand_wm_tac_cover", "tac_cover" },
	{"offhand_wm_jerrycan", "jerrycan" },
	{"x2_buy_station_rig_skeleton", "buystation" },
	{"veh8_mil_air_malfa_small_noscript", "veh8_mil_air" },
	{"offhand_wm_trophy_system_br", "TrophySystem" },
	{"perk_manual_01_dead_silence", "deadsilence" },*/

	//other strike,Uav ect
	{ "wm_killstreak_handset_ch3_airstrike", "Airstrike" },
	{ "killstreak_tablet_01_clusterstrike", "Clusterstrike" },
	{ "killstreak_tablet_01_uav", " UAV" },

};

namespace main_game
{
	void ui_header()
	{
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::Begin("A", reinterpret_cast<bool*>(true), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
		ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always);
	}

	void ui_end()
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		window->DrawList->PushClipRectFullScreen();
		ImGui::End();
		ImGui::PopStyleColor();
	}

	void init(ImFont* font)
	{
		ui_header();
		main_loop(font);
		ui_end();
	}

	void player_esp(ImFont* font)
	{
		//uint32_t current_color;
		utils::color_var current_color;

		globals::b_in_game = sdk::in_game();
		globals::max_player_count = sdk::get_max_player_count();

		if (!globals::b_in_game)
		{
			sdk::clear_vel_map();
			
			if (!globals::b_spread) {
				globals::b_spread = false;
			}

			once = false;
			return;
		}

		sdk::start_tick();

		auto ref_def = sdk::get_refdef();
		if (is_bad_ptr(ref_def))
			return;

		g_data::refdef = ref_def;

		globals::refdef_ptr = (uintptr_t)g_data::refdef;

		/*auto cl_info_base = decryption::get_client_info_base();
		if (is_bad_ptr(cl_info_base))
			return;*/

		sdk::player_t local_player = sdk::get_local_player();

		globals::local_ptr = local_player.address;
		if (is_bad_ptr(globals::local_ptr))
			return;

		globals::local_is_alive = !local_player.died() && local_player.is_valid();

		if (!globals::local_is_alive)
			return;

		globals::local_pos = local_player.get_pos();

		if (Settings::esp::b_crosshair)
			g_draw::draw_crosshair();

		if (Settings::esp::b_fov)
			g_draw::draw_fov(Settings::esp::fov_size);

		if (Settings::esp::b_radar)
			g_radar::show_radar_background();

		if (Settings::esp::b_auav)
			sdk::enable_uav();

		if (!globals::is_aiming)
			aim_assist::reset_lock();

		if (Settings::esp::b_visible)
			g_data::visible_base = sdk::get_visible_base();
	
		if (!Settings::aimbot::b_recoil) {
			aim_assist::no_recoil();
		}

		if (Settings::aimbot::b_spread)
		{
		
		}

		for (int i = 0; i < globals::max_player_count; ++i)
		{
			if (local_player.get_index() == i)
				continue;

			auto cl_info_base = decryption::get_client_info_base();
			if (is_bad_ptr(cl_info_base))
				return;

			sdk::player_t player = sdk::get_player(i);
			if (is_bad_ptr(player.address))
				continue;

			bool is_friendly = player.team_id() == local_player.team_id();;

			if (is_friendly && !Settings::esp::b_friendly)
				continue;

			auto is_visible = player.is_visible();
			auto is_valid = player.is_valid();
			auto died = player.died();
			auto health = sdk::get_player_health(i);

			if (!is_valid || died)
				continue;

			float fdistance = 0;
			float display_string = 0;

			fdistance = sdk::units_to_m(globals::local_pos.distance_to(player.get_pos()));

			if (fdistance > Settings::esp::max_distance)
				continue;

			if (Settings::esp::b_visible && is_visible)
				current_color = is_friendly ? Settings::colors::visible_team : Settings::colors::visible_enemy;
			else
				current_color = is_friendly ? Settings::colors::not_visible_team : Settings::colors::not_visible_enemy;

			if (Settings::esp::b_radar)
				g_radar::draw_entity(local_player, player, is_friendly, died, current_color.color());

			if (Settings::esp::b_visible_only && !is_visible)
				continue;

			vec2_t ScreenPos, BoxSize;

			if (!sdk::w2s(player.get_pos(), &ScreenPos, &BoxSize))
				continue;

			if (!sdk::check_valid_bones(i, player.get_pos())) {
				continue;
			}

			if (Settings::esp::b_health)
			{
				g_draw::draw_health(health, player.get_pos());
			}
			
			if (Settings::esp::b_box)
			{
				g_draw::draw_corned_box(ScreenPos, BoxSize, current_color.color(), 1.f);
			}
			if (Settings::esp::b_line)
			{
				ImVec2 origin;
				origin.x = g_data::refdef->Width / 2;
				origin.y = g_data::refdef->Height / 2;

				g_draw::draw_line(origin, ImVec2(ScreenPos.x, ScreenPos.y), current_color.color(), 1.f);
			}
			if (Settings::esp::b_names)
			{
				std::string name = sdk::get_player_name(i);
				g_draw::draw_sketch_edge_text(font, name, ImVec2(
					ScreenPos.x + (BoxSize.x / 2),
					ScreenPos.y + BoxSize.y + 5 + display_string),
					18.0f,
					current_color.color(),
					true
				);

				display_string += 15;
			}
			if (Settings::esp::b_distance)
			{
				std::string distance_string("[ ");
				distance_string += std::to_string(fdistance);
				std::string::size_type end = distance_string.find('.');
				if (end != std::string::npos && (end + 2) <= (distance_string.length() - 1))
					distance_string.erase(distance_string.begin() + end + 2, distance_string.end());
				distance_string += "m ]";
				g_draw::draw_sketch_edge_text(font, distance_string, ImVec2(
					ScreenPos.x + (BoxSize.x / 2),
					ScreenPos.y + BoxSize.y + 5 + display_string),
					18.0f,
					current_color.color(),
					true
				);
			}
			if (Settings::esp::b_skeleton)
			{
				g_draw::draw_bones(i, player.get_pos(), current_color.color());
			}
			if (Settings::aimbot::b_lock)
			{
				vec3_t current_bone;
				vec2_t bone_screen_pos;
				
				sdk::update_vel_map(sdk::local_index(), local_player.get_pos());

				if (Settings::aimbot::b_target_bone)
				{
					if (!sdk::get_bone_by_player_index(i, aim_assist::get_bone_opt(), &current_bone))
						continue;

					if (!sdk::WorldToScreen(current_bone, &bone_screen_pos))
						continue;
					
					if (Settings::esp::b_aim_point)
						g_draw::draw_circle(ImVec2(bone_screen_pos.x, bone_screen_pos.y), 5, current_color.color(), 100.0f);

					if (player.get_stance() == sdk::KNOCKED && Settings::aimbot::b_skip_knocked)
						continue;

					if (Settings::esp::b_visible && !player.is_visible())
						continue;

					if (is_friendly)
						continue;

					switch (Settings::aimbot::i_priority)
					{
					case 0:
						aim_assist::get_closest_enemy(bone_screen_pos, fdistance);
						break;
					case 1:
						aim_assist::get_enemy_in_fov(bone_screen_pos);
						break;
					case 2:
						if (fdistance <= Settings::aimbot::f_min_closest)
						{
							if (player.get_stance() == sdk::KNOCKED)
								continue;
							aim_assist::get_closest_enemy(bone_screen_pos, fdistance);
						}
						else {
							aim_assist::get_enemy_in_fov(bone_screen_pos);
						}
						break;
					}
				}
				else
				{
					if (!once) {
						sdk::clear_vel_map();
						once = true;
					}

					sdk::update_vel_map(i, player.get_pos());
					
					if (!sdk::head_to_screen(current_bone, &bone_screen_pos, player.get_stance())) 
						continue;

					if (Settings::esp::b_aim_point)
						g_draw::draw_circle(ImVec2(bone_screen_pos.x, bone_screen_pos.y), 5, current_color.color(), 100.0f);

					if (player.get_stance() == sdk::KNOCKED && Settings::aimbot::b_skip_knocked)
						continue;

					if (Settings::esp::b_visible && !player.is_visible())
						continue;

					if (is_friendly)
						continue;

					switch (Settings::aimbot::i_priority)
					{
					case 0:
						aim_assist::get_closest_enemy(bone_screen_pos, fdistance);
						break;
					case 1:
						aim_assist::get_enemy_in_fov(bone_screen_pos);
						break;
					case 2:
						if (fdistance <= Settings::aimbot::f_min_closest)
						{
							if (player.get_stance() == sdk::KNOCKED)
								continue;
							aim_assist::get_closest_enemy(bone_screen_pos, fdistance);
						}
						else {
							aim_assist::get_enemy_in_fov(bone_screen_pos);
						}
						break;
					}
				}
			}
		}
		if(!g_menu::b_menu_open)
			aim_assist::is_aiming();
	}

	void loot_esp(ImFont* font)
	{
		utils::color_var item_color = Settings::colors::Green;
		float f_distance = 0;

		for (auto j = 0; j < 1000; j++)
		{
			float display_string = 0;
			auto loot = sdk::get_loot_by_id(j);

			if (!loot->is_valid())
				continue;

			auto name = loot->get_name();

			if (is_bad_ptr(name) || !strlen(name))
				continue;

			auto pos = loot->get_position();
			if (pos.is_Zero())
				continue;

			vec2_t screen_pos;
			if (!sdk::WorldToScreen(pos, &screen_pos))
				continue;

			f_distance = sdk::units_to_m(globals::local_pos.distance_to(pos));
			if (f_distance > Settings::lootEsp::max_dist)
				continue;

			for (auto k = 0; k < IM_ARRAYSIZE(loot_defs); k++)
			{
				auto* loot_def = &loot_defs[k];

				if (name && strstr(name, loot_def->name))
				{
					if (strstr(name, "accessory_money") ||
						strstr(name, "loot_wm_armor_plate") ||
						strstr(name, "lm_armor_vest_01") ||
						strstr(name, "gasmask_a")) {

						item_color = Settings::colors::cash_color;
					}
					if (strstr(name, "mission_tablet_01_scavenger") ||
						strstr(name, "mission_tablet_01_assassination")) {

						item_color = Settings::colors::mission_color;
					}
					if (strstr(name, "loot_ammo_pistol_smg") ||
						strstr(name, "ammo_box_ar") ||
						strstr(name, "ammo_shotgun") ||
						strstr(name, "ammo_marksman_sniper")) {

						item_color = Settings::colors::ammo_color;
					}
					if (strstr(name, "loot_crate_01_br_legendary_01")) {
						item_color = Settings::colors::crate_color;
					}
					if (strstr(name, "military_loot_crate"))
					{
						item_color = Settings::colors::crate_color3;
					}

					if (loot_def->show != 0 && !*loot_def->show)
						continue;

					if (Settings::lootEsp::b_name)
					{
						std::string buff = std::string(loot_def->text);
						g_draw::draw_sketch_edge_text(font, buff, ImVec2(
							screen_pos.x,
							screen_pos.y + 5 + display_string),
							18.0f,
							item_color.color(),
							true);
					}
					if (Settings::lootEsp::b_distance)
					{
						std::string distance_string("[ ");
						distance_string += std::to_string(f_distance);
						std::string::size_type end = distance_string.find('.');
						if (end != std::string::npos && (end + 2) <= (distance_string.length() - 1))
							distance_string.erase(distance_string.begin() + end + 2, distance_string.end());
						distance_string += "m ]";
						g_draw::draw_sketch_edge_text(font, distance_string, ImVec2(
							screen_pos.x,
							screen_pos.y + 5 + 20),
							18.0f,
							item_color.color(),
							true);
					}
				}
			}
		}
	}

	void main_loop(ImFont* font) {

		player_esp(font);
		loot_esp(font);
	}
}