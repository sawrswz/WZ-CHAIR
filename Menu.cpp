#include "stdafx.h"
#include "Menu.h"
#include "sdk.h"
#include "xor.hpp"
#include "settings.h"
#include "image_compressed.h"

bool b_debug_open = false;
bool b_thirdPerson = false;

static auto vector_getter = [](void* vec, int idx, const char** out_text)
{
	auto& vector = *static_cast<std::vector<std::string>*>(vec);
	if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
	*out_text = vector.at(idx).c_str();
	return true;
};

namespace ImGuiEx
{
	void KeyBindButton(int& key)
	{
		static auto b_get = false;
		static std::string sz_text = xorstr("Click to bind.");

		if (ImGui::Button(sz_text.c_str(), ImVec2(160, 20)))
			b_get = true;

		if (b_get)
		{
			for (auto i = 1; i < 256; i++)
			{
				if (GetAsyncKeyState(i) & 0x8000)
				{
					if (i != 12)
					{
						key = i == VK_ESCAPE ? 0 : i;
						b_get = false;
					}
				}
			}
			sz_text = xorstr("Press a Key.");
		}
		else if (!b_get && key == 0)
			sz_text = xorstr("Click to bind.");
		else if (!b_get && key != 0)
		{
			sz_text = xorstr("Key ~ ") + std::to_string(key);
		}
	}

	void KeyBindButton(int& key, int width, int height)
	{
		static auto b_get = false;
		static std::string sz_text = xorstr("Click to bind.");

		if (ImGui::Button(sz_text.c_str(), ImVec2(static_cast<float>(width), static_cast<float>(height))))
			b_get = true;

		if (b_get)
		{
			for (auto i = 1; i < 256; i++)
			{
				if (GetAsyncKeyState(i) & 0x8000)
				{
					if (i != 12)
					{
						key = i == VK_ESCAPE ? -1 : i;
						b_get = false;
					}
				}
			}
			sz_text = xorstr("Press a Key.");
		}
		else if (!b_get && key == -1)
			sz_text = xorstr("Click to bind.");
		else if (!b_get && key != -1)
		{
			sz_text = xorstr("Key ~ ") + std::to_string(key);
		}
	}

	void ColorPicker(const char* text, utils::color_var* cv_col, int spaces)
	{
		const auto& style = ImGui::GetStyle();

		ImGui::PushID(text);
		if (spaces > -1)
			ImGui::SameLine(static_cast<float>(spaces), style.ItemInnerSpacing.x);

		const ImVec4 col_display(static_cast<float>(cv_col->color().r()) / 255.f, static_cast<float>(cv_col->color().g()) / 255.f, static_cast<float>(cv_col->color().b()) / 255.f, static_cast<float>(cv_col->color().a()) / 255.f);
		if (ImGui::ColorButton("Display Color", col_display))
			ImGui::OpenPopup(text);

		if (ImGui::BeginPopup(text))
		{
			auto i_option = 0;
			if (cv_col->b_rainbow)
				i_option = 1;

			ImGui::RadioButton(xorstr("Static Color"), &i_option, 0);
			ImGui::RadioButton(xorstr("Rainbow"), &i_option, 1);
			if (!cv_col->b_rainbow)
			{
				float col[4] = { cv_col->col_color.Value.x, cv_col->col_color.Value.y, cv_col->col_color.Value.z };
				ImGui::ColorPicker3("Color",col);
				cv_col->col_color.Value.x = col[0];
				cv_col->col_color.Value.y = col[1];
				cv_col->col_color.Value.z = col[2];
				ImGui::PushItemWidth(228);
				ImGui::SliderFloat(xorstr("##ALPHACFASFASOL"), &cv_col->col_color.Value.w, 0.f, 1.f, xorstr("ALPHA: %0.2f"));
				ImGui::PopItemWidth();
				cv_col->b_rainbow = false;
			}
			else
			{
				ImGui::PushItemWidth(228);
				ImGui::SliderFloat(xorstr("##RAINBAFSFASASFOWSPEED"), &cv_col->f_rainbow_speed, 0.f, 1.f, xorstr("SPEED: %0.2f"));
				ImGui::SliderFloat(xorstr("##ALPHFASFASAFSACOL"), &cv_col->col_color.Value.w, 0.f, 1.f, xorstr("ALPHA: %0.2f"));
				ImGui::PopItemWidth();
				cv_col->b_rainbow = true;
			}

			if (i_option == 1)
				cv_col->b_rainbow = true;
			else
				cv_col->b_rainbow = false;
			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	static bool ListBox(const char* label, int* current_item, std::function<const char* (int)> lambda, int items_count, int height_in_items)
	{
		return ImGui::ListBox(label, current_item, [](void* data, int idx, const char** out_text)
			{
				*out_text = (*reinterpret_cast<std::function<const char* (int)>*>(data))(idx);
				return true;
			}, &lambda, items_count, height_in_items);
	}

	void* CopyFont(const unsigned int* src, std::size_t size)
	{
		void* ret = (void*)(new unsigned int[size / 4]);
		memcpy_s(ret, size, src, size);
		return ret;
	}

	void RenderTabs(std::vector<const char*> tabs, int& activetab, float w, float h, bool sameline)
	{
		bool values[8] = { false };
		values[activetab] = true;

		for (auto i = 0; i < static_cast<int>(tabs.size()); ++i)
		{
			if (ImGui::Selectable(tabs[i], &values[i], 0, ImVec2{ w, h }))
				activetab = i;

			if (sameline && i < static_cast<int>(tabs.size()) - 1)
				ImGui::SameLine();
		}
	}

	void SetTooltip(const char* text)
	{
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(xorstr("%s"), text);
	}
}

namespace g_menu
{
	bool b_menu_open = false;
	std::string str_config_name = "";

	void VisualTab() {
		/*ImGui::Separator();
		ImGui::Checkbox(xorstr("Show menu on load"), &Settings::menu::b_auto_show);
		ImGui::Separator();

		ImGui::Dummy(ImVec2(-10, 10));
		ImGui::SameLine();*/

		ImGui::Columns(3, xorstr("##VISUALTAB1"), false);
		{
			ImGui::SetColumnOffset(1, 244.5);
			ImGui::BeginChild(xorstr("##1VISUALCOL1"), ImVec2(0,0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				ImGui::PushItemWidth(-1);
				ImGui::Text(xorstr("Player"));
				ImGui::Separator();

				ImGui::SliderInt(xorstr("##MAXDISTANCE"), &Settings::esp::max_distance, 5, 1000, xorstr("ESP Distance: %dm"));
				ImGui::Separator();
				ImGui::Checkbox(xorstr("Advanced UAV"), &Settings::esp::b_auav);
				ImGui::Checkbox(xorstr("Check Visible"), &Settings::esp::b_visible);
				ImGui::Checkbox(xorstr("Show Visible Only"), &Settings::esp::b_visible_only);
				ImGui::Checkbox(xorstr("Show Boxes"), &Settings::esp::b_box);
				ImGui::Checkbox(xorstr("Show HealthBar"), &Settings::esp::b_health);
				ImGui::Checkbox(xorstr("Show Line"), &Settings::esp::b_line);
				ImGui::Checkbox(xorstr("Show Bones"), &Settings::esp::b_skeleton);
				ImGui::Checkbox(xorstr("Show Names"), &Settings::esp::b_names);
				ImGui::Checkbox(xorstr("Show Distance"), &Settings::esp::b_distance);
				ImGui::Checkbox(xorstr("Show Friendlies"), &Settings::esp::b_friendly);
			}
			ImGui::EndChild();

		}
		ImGui::NextColumn();
		{
			ImGui::SetColumnOffset(2, 488.5);
			ImGui::BeginChild(xorstr("##2VISUALCOL1"), ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				ImGui::PushItemWidth(-1);
				ImGui::Text(xorstr("Items"));
				ImGui::Separator();

				ImGui::SliderInt(xorstr("##MAXDISTANCE"), &Settings::lootEsp::max_dist, 5, 1000, xorstr("ESP Distance: %dm"));
				ImGui::Separator();
				ImGui::Checkbox(xorstr("Name"), &Settings::lootEsp::b_name);
				ImGui::Checkbox(xorstr("Distance"), &Settings::lootEsp::b_distance);
				ImGui::Dummy(ImVec2(-5, 5));
				ImGui::Text(xorstr("Filter"));
				ImGui::Separator();
				ImGui::Checkbox(xorstr("Show Weapons"), &Settings::lootEsp::b_weapons);
				ImGui::Checkbox(xorstr("Show Legendary crates"), &Settings::lootEsp::b_crates);
				ImGui::Checkbox(xorstr("Show Military crates"), &Settings::lootEsp::b_crates3);
				ImGui::Checkbox(xorstr("Show Missions"), &Settings::lootEsp::b_missions);
				ImGui::Checkbox(xorstr("Show Ammo"), &Settings::lootEsp::b_ammo);
			}
			ImGui::EndChild();
		}
		ImGui::Columns(1);
	}
	void AimbotTab() {

		ImGui::Text(xorstr("Aimbot Key"));
		ImGui::SameLine();
		ImGuiEx::KeyBindButton(Settings::aimbot::aim_key, 100, 20);
		ImGui::Separator();
		ImGui::Columns(3, xorstr("##AIMTAB1"), false);
		{
			ImGui::SetColumnOffset(1, 244.5);
			ImGui::BeginChild(xorstr("##1AIMCOL1"), ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				ImGui::PushItemWidth(-1);
				ImGui::Checkbox(xorstr("Enable"), &Settings::aimbot::b_lock);
				ImGui::Checkbox(xorstr("Show Fov"), &Settings::esp::b_fov);
				ImGui::Checkbox(xorstr("No Recoil"), &Settings::aimbot::b_recoil);
				ImGui::Checkbox(xorstr("No Spread"), &Settings::aimbot::b_spread);
				ImGui::Checkbox(xorstr("Skip Knocked"), &Settings::aimbot::b_skip_knocked);
				ImGui::Checkbox(xorstr("Prediction"), &Settings::aimbot::b_skip_knocked);
				ImGui::Checkbox(xorstr("Use Bones"), &Settings::aimbot::b_target_bone);
				ImGui::Combo(xorstr("Target Priority"), &Settings::aimbot::i_priority, xorstr("Closest\0FOV\0Closest and FOV\0"));
			}
			ImGui::EndChild();
		}
		ImGui::NextColumn();
		{
			ImGui::SetColumnOffset(2, 488.5);
			ImGui::BeginChild(xorstr("##2AIMCOL1"), ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				ImGui::PushItemWidth(-1);
				ImGui::SliderInt(xorstr("##FOVSIZE"), &Settings::esp::fov_size, 5, 800, xorstr("FOV Size: %dm"));
				ImGui::SliderFloat(xorstr("##LOCKSPEED"), &Settings::aimbot::f_speed, 0.1f, 2.0f, xorstr("Speed: %0.2f sec."));
				ImGui::SliderFloat(xorstr("##LOCKSMOOTH"), &Settings::aimbot::f_smooth, 0.1f, 2.0f, xorstr("Smooth: %0.2f sec."));
				if (Settings::aimbot::i_priority == 0 || Settings::aimbot::i_priority == 2)
				{
					ImGui::SliderFloat(xorstr("##PRIORDIST"), &Settings::aimbot::f_min_closest, 5.0f, 50.0f, xorstr("Priority distance: %0.1fm"));
				}
				if (Settings::aimbot::b_target_bone)
				{
					ImGui::Combo(xorstr("Target Bone"), &Settings::aimbot::i_bone, xorstr("Spine\0Head\0Neck\0Chest\0"));
				}
			}
			ImGui::EndChild();
		}
		ImGui::Columns(1);
	}
	void MiscTab(){

	}
	void ColorsTab() {
		ImGui::Columns(3, xorstr("##COLORSTAB1"), false);
		{
			ImGui::SetColumnOffset(1, 244.5);
			ImGui::BeginChild(xorstr("##1COLORSCOL1"));
			{
				ImGui::PushItemWidth(-1);
				ImGui::Text(xorstr("Player"));
				ImGui::Separator();
				static int col_x = 185;

				ImGui::Spacing();
				ImGui::Text(xorstr("Visible Team Color"));
				ImGuiEx::ColorPicker(xorstr("team_vis_col"), &Settings::colors::visible_team, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Not Visible Team Color"));
				ImGuiEx::ColorPicker(xorstr("team_not_vis_col"), &Settings::colors::not_visible_team, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Visible Enemy Color"));
				ImGuiEx::ColorPicker(xorstr("enemy_vis_col"), &Settings::colors::visible_enemy, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Not Visible Enemy Color"));
				ImGuiEx::ColorPicker(xorstr("enemy_not_vis_col"), &Settings::colors::not_visible_enemy, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Radar BackGround"));
				ImGuiEx::ColorPicker(xorstr("radar_bg_col"), &Settings::colors::radar_bg, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Radar Border"));
				ImGuiEx::ColorPicker(xorstr("radar_border"), &Settings::colors::radar_boarder, col_x);
				
			}
			ImGui::EndChild();
		}
		ImGui::NextColumn();
		{
			ImGui::SetColumnOffset(2, 488.5);
			ImGui::BeginChild(xorstr("##2COLORSCOL1"), ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				ImGui::PushItemWidth(-1);
				ImGui::Text(xorstr("Items"));
				ImGui::Separator();

				static int col_x = 190;
				ImGui::Text(xorstr("Cash Color"));
				ImGuiEx::ColorPicker(xorstr("cash_col"), &Settings::colors::cash_color, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Weapon Color"));
				ImGuiEx::ColorPicker(xorstr("weapon_col"), &Settings::colors::weapon_color, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Legendary Crate Color"));
				ImGuiEx::ColorPicker(xorstr("create_1"), &Settings::colors::crate_color, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Military Crate Color"));
				ImGuiEx::ColorPicker(xorstr("create_3"), &Settings::colors::crate_color3, col_x);
				ImGui::Spacing();
				ImGui::Text(xorstr("Ammo Color"));
				ImGuiEx::ColorPicker(xorstr("ammo_col"), &Settings::colors::ammo_color, col_x);
			}
			ImGui::EndChild();
		}
		ImGui::Columns(1);
	}
	void ConfigTab() {
		ImGui::Columns(3, xorstr("##CONFIGSTAB1"), false);
		{
			ImGui::SetColumnOffset(1, 235);
			ImGui::BeginChild(xorstr("##1CONFIGSCOL1"));
			{
				ImGui::PushItemWidth(-1);
				static std::string str_warn = xorstr("Config Must be at least 3 char length!");
				ImGui::Text(xorstr("Config:"));
				static char buf[128] = "";
				const auto width = ImGui::GetWindowWidth();
				ImGui::PushItemWidth(width - 29);
				ImGui::InputText(xorstr("##CONFIGNAME"), buf, 32);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button(xorstr("+"), ImVec2(20, 20)))
				{
					if (std::string(buf).length() < 3)
					{
						ImGui::OpenPopup(xorstr("warn"));
						str_warn = xorstr("Config Must be at least 3 char length!");
					}
					else
					{
						Settings::Save_Settings(buf);
					}
				}
			}
			ImGui::EndChild();
		}
		ImGui::Columns(1);
	}

	void Render(ImFont* font)
	{
		if (GetAsyncKeyState(VK_DELETE) & 0x1) {
			b_menu_open = !b_menu_open;
		}

		if (b_menu_open) {

			ImGui::Begin(xorstr("Limitless Aimy-2.0"), &b_menu_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			ImGui::SetWindowSize(ImVec2(850, 540), ImGuiCond_Always);

			static auto i_active_tab = 0;
			ImGui::Columns(2, xorstr("##TAB"), false);
			{
				ImGui::SetColumnOffset(1, 160);
				ImGui::Dummy(ImVec2(15, 0));
				ImGui::PushFont(font);
				const std::vector<const char*> vec_tabs = { 
				};
				ImGuiEx::RenderTabs(vec_tabs, i_active_tab, 150.f, 49.f, false);
				ImGui::PopFont();
			}
			ImGui::NextColumn();
			{
				ImGui::SetColumnOffset(2, ImGui::GetWindowWidth());
				ImGui::BeginChild(xorstr("##PANEL"), ImVec2(0, 440), true, ImGuiWindowFlags_NoScrollWithMouse);
				{
					switch (i_active_tab)
					{
					default:
						VisualTab();
						break;
					case 1:
						AimbotTab();
						break;
					/*case 2:
						MiscTab();
						break;*/
					case 2:
						ColorsTab();
						break;
					case 3:
						ConfigTab();
						break;
					}
				}
				ImGui::EndChild();
			}
			ImGui::Columns(1);
			ImGui::Separator();
			ImGui::End();
		}
	}
}