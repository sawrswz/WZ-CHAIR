#include "aim.h"
#include "sdk.h"
#include "settings.h"
#include "offsets.h"
#include "weapons.h"

namespace aim_assist
{
	vec2_t lock_position{};
	vec2_t target_angles{};

	WeaponCompleteDefArr* weapons = nullptr;
	float valuesRecoilBackup[962][60];
	float valuesSpreadBackup[962][22];

	float init_center_dist = 10000.0f;
	float min_center_dist = init_center_dist;
	float init_distance = 10000.0f;
	float min_distance = init_distance;

	void reset_lock()
	{
		min_center_dist = init_center_dist;
		min_distance = init_distance;
	}

	bool is_key_down(int vk_key)
	{
		return ((GetAsyncKeyState(vk_key) & 0x8000) ? 1 : 0);
	}

	void no_recoil()
	{
	}

	void enable_no_spread() {
	}

	unsigned long get_bone_opt()
	{
		switch (Settings::aimbot::i_bone)
		{
		case 0:
			return sdk::BONE_POS_MID;
		case 1:
			return sdk::BONE_POS_HEAD;
		case 2:
			return sdk::BONE_POS_NECK;
		case 3:
			return sdk::BONE_POS_CHEST;
		};
		return sdk::BONE_POS_HEAD;
	}

	/*unsigned long get_aimbot_key()
	{
		switch (globals::lock_key)
		{
		case 0:
			return VK_LBUTTON;
		case 1:
			return VK_RBUTTON;
		case 2:
			return 70;
		}
		return VK_LBUTTON;
	}*/

	void get_closest_enemy(vec2_t target_pos, float distance)
	{
		if (distance < min_distance)
		{
			min_distance = distance;
			lock_position = target_pos;
		}
	}

	void get_enemy_in_fov(vec2_t target_pos)
	{
		const float x = target_pos.x - (g_data::refdef->Width / 2);
		const float y = target_pos.y - (g_data::refdef->Height / 2);
		float center_dis = sqrt(pow(y, 2) + pow(x, 2));

		if (center_dis < min_center_dist && center_dis < Settings::esp::fov_size)
		{
			min_center_dist = center_dis;
			lock_position = target_pos;
		}
	}

	void aim_at()
	{
		vec2_t target(0, 0);
		vec2_t center(g_data::refdef->Width / 2, g_data::refdef->Height / 2);
		
		if (min_center_dist != init_center_dist ||
			min_distance != init_distance)
		{
			if (lock_position.x != 0)
			{
				if (lock_position.x > center.x)
				{
					target.x = -(center.x - lock_position.x);
					target.x /= (Settings::aimbot::f_speed * 10.f);
					if (target.x + center.x > center.x * 2)
						target.x = 0;
				}
				if (lock_position.x < center.x)
				{
					target.x = lock_position.x - center.x;
					target.x /= (Settings::aimbot::f_speed * 10.f);
					if (target.x + center.x < 0)
						target.x = 0;
				}
			}
			if (lock_position.y != 0)
			{
				if (lock_position.y > center.y)
				{
					target.y = -(center.y - lock_position.y);
					target.y /= (Settings::aimbot::f_speed * 10.f);
					if (target.y + center.y > center.y * 2)
						target.y = 0;
				}
				if (lock_position.y < center.y)
				{
					target.y = lock_position.y - center.y;
					target.y /= (Settings::aimbot::f_speed * 10.f);
					if (target.y + center.y < 0)
						target.y = 0;
				}
			}
			target.x /= (Settings::aimbot::f_smooth * 10.f);
			target.y /= (Settings::aimbot::f_smooth * 10.f);
			
			if (!target.is_Zero()) {
				mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(target.x), static_cast<DWORD>(target.y), NULL, NULL);
			}
		}
	}

	void is_aiming()
	{
		if (Settings::aimbot::aim_key != -1) {
			if (is_key_down(Settings::aimbot::aim_key)) {
				globals::is_aiming = true;
				aim_at();
			}
			globals::is_aiming = false;
		}
		else if(is_key_down(VK_LBUTTON) ||
			is_key_down(VK_XBUTTON1) ||
			is_key_down(VK_RBUTTON) && 
			is_key_down(VK_LSHIFT))
		{
			globals::is_aiming = true;
			aim_at();
		}
		globals::is_aiming = false;
	}

}