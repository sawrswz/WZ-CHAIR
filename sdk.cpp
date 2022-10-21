#include "sdk.h"
#include "defs.h"
#include "offsets.h"
#include "xor.hpp"
#include "imgui/imgui_internal.h"
#include "lazyimporter.h"
#include "settings.h"

namespace process
{
	HWND hwnd;

	BOOL CALLBACK EnumWindowCallBack(HWND hWnd, LPARAM lParam)
	{
		DWORD dwPid = 0;
		GetWindowThreadProcessId(hWnd, &dwPid);
		if (dwPid == lParam)
		{
			hwnd = hWnd;
			return FALSE;
		}
		return TRUE;
	}

	HWND get_process_window()
	{
		if (hwnd)
			return hwnd;

		EnumWindows(EnumWindowCallBack, GetCurrentProcessId());

		if (hwnd == NULL)
			Exit();

		return hwnd;
	}
}

namespace sdk
{
	const DWORD nTickTime = 64;//64 ms
	bool bUpdateTick = false;
	std::map<DWORD, velocityInfo_t> velocityMap;
	
	vec3_t get_camera_location()
	{
		float x = *(float*)(*(uint64_t*)(g_data::base + offsets::camera_base) + 0xF8);
		float y = *(float*)(*(uint64_t*)(g_data::base + offsets::camera_base) + 0x108);
		float z = *(float*)(*(uint64_t*)(g_data::base + offsets::camera_base) + 0x118);

		return vec3_t{ x, y, z };
	}

	bool WorldToScreen(const vec3_t& WorldPos, vec2_t* ScreenPos)
	{
		vec3_t ViewOrig = get_camera_location();
		auto refdef = g_data::refdef;
		vec3_t vLocal, vTransform;
		vLocal = WorldPos - ViewOrig;
		// get our dot products from viewAxis
		vTransform.x = vLocal.dot(refdef->ViewAxis[1]);
		vTransform.y = vLocal.dot(refdef->ViewAxis[2]);
		vTransform.z = vLocal.dot(refdef->ViewAxis[0]);
		// make sure it is in front of us
		if (vTransform.z < 0.01f)
			return false;
		ScreenPos->x = ((refdef->Width/ 2) * (1 - (vTransform.x / refdef->FovX / vTransform.z)));
		ScreenPos->y = ((refdef->Height / 2) * (1 - (vTransform.y / refdef->FovY / vTransform.z)));

		if (ScreenPos->x < 1 || ScreenPos->y < 1 || (ScreenPos->x > refdef->Width) || (ScreenPos->y > refdef->Height)) {
			return false;
		}

		return true;
	}

	float units_to_m(float units) {
		return units * 0.0254;
	}

	float m_to_units(float meters) {
		return 0.0254 / meters;
	}
	
	float xangle(const vec3_t& LocalPos, const vec3_t& WorldPos)
	{
		float dl = sqrt((WorldPos.x - LocalPos.x) * (WorldPos.x - LocalPos.x) + (WorldPos.y - LocalPos.y) * (WorldPos.y - LocalPos.y));

		if (dl == 0.0f)
			dl = 1.0f;

		float dl2 = abs(WorldPos.x - LocalPos.x);
		float teta = ((180.0f / M_PI) * acos(dl2 / dl));

		if (WorldPos.x < LocalPos.x)
			teta = 180.0f - teta;

		if (WorldPos.y < LocalPos.y)
			teta = teta * -1.0f;

		if (teta > 180.0f)
			teta = (360.0f - teta) * (-1.0f);

		if (teta < -180.0f)
			teta = (360.0f + teta);

		return teta;
	}

	void rotation_point_alpha(float x, float y, float z, float alpha, vec3_t* outVec3)
	{
		static HMODULE hD3dx9_43 = NULL;
		if (hD3dx9_43 == NULL)
			hD3dx9_43 = LoadLibraryA(xorstr("d3dx9_43.dll"));

		typedef LinearTransform* (WINAPI* t_D3DXMatrixRotationY)(LinearTransform* pOut, FLOAT Angle);
		static t_D3DXMatrixRotationY D3DXMatrixRotationY = NULL;
		if (D3DXMatrixRotationY == NULL)
			D3DXMatrixRotationY = (t_D3DXMatrixRotationY)GetProcAddress(hD3dx9_43, xorstr("D3DXMatrixRotationY"));

		typedef vec4_t* (WINAPI* t_D3DXVec3Transform)(vec4_t* pOut, CONST vec4_t* pV, CONST LinearTransform* pM);
		static t_D3DXVec3Transform D3DXVec4Transform = NULL;
		if (D3DXVec4Transform == NULL)
			D3DXVec4Transform = (t_D3DXVec3Transform)GetProcAddress(hD3dx9_43, xorstr("D3DXVec4Transform"));

		Matrix4x4 rot1;
		vec4_t vec = { x, z, y, 1.0f };
		D3DXMatrixRotationY((LinearTransform*)&rot1, alpha * M_PI / 180.0f);
		D3DXVec4Transform((vec4_t*)&vec, (const vec4_t*)&vec, (const LinearTransform*)&rot1);

		outVec3->x = vec.x;
		outVec3->y = vec.z;
		outVec3->z = vec.y;
	};

	bool w2s(const vec3_t& WorldPos, vec2_t* ScreenPos, vec2_t* BoxSize)
	{
		auto ViewOrig = get_camera_location();

		float angleX = xangle(ViewOrig, WorldPos);

		vec3_t posl, posr;

		rotation_point_alpha(-16.0f, 0.0f, 65.0f, -angleX + 90.0f, &posl);
		rotation_point_alpha(16.0f, 0.0f, -5.0f, -angleX + 90.0f, &posr);

		vec3_t vposl, vposr;

		vposl.x = WorldPos.x + posl.x;
		vposl.y = WorldPos.y + posl.y;
		vposl.z = WorldPos.z + posl.z;

		vposr.x = WorldPos.x + posr.x;
		vposr.y = WorldPos.y + posr.y;
		vposr.z = WorldPos.z + posr.z;

		vec2_t screenPosl, screenPosr;

		if (!WorldToScreen(vposl, &screenPosl) || !WorldToScreen(vposr, &screenPosr))
		{
			return false;
		}

		BoxSize->x = abs(screenPosr.x - screenPosl.x);
		BoxSize->y = abs(screenPosl.y - screenPosr.y);

		ScreenPos->x = screenPosr.x - BoxSize->x;
		ScreenPos->y = screenPosl.y;

		return true;
	}

	bool head_to_screen(vec3_t pos, vec2_t* pos_out, int stance)
	{
		vec2_t w2s_head;

		pos.z += 55.f;
		if (!WorldToScreen(pos, &w2s_head))
			return false;

		else if (stance == sdk::CROUNCH)
		{
			pos.z -= 25.f;
			if (!WorldToScreen(pos, &w2s_head))
				return false;
		}
		else if (stance == sdk::KNOCKED)
		{
			pos.z -= 28.f;
			if (!WorldToScreen(pos, &w2s_head))
				return false;
		}
		else if (stance == sdk::PRONE)
		{
			pos.z -= 50.f;
			if (!WorldToScreen(pos, &w2s_head))
				return false;
		}

		pos_out->x = w2s_head.x;
		pos_out->y = w2s_head.y;

		return true;
	}

	bool bones_to_screen(vec3_t* BonePosArray, vec2_t* ScreenPosArray, const long Count)
	{
		for (long i = 0; i < Count; ++i)
		{
			if (!WorldToScreen(BonePosArray[i], &ScreenPosArray[i]))
				return false;
		}
		return true;
	}

	bool is_valid_bone(vec3_t origin, vec3_t* bone, const long Count)
	{
		for (long i = 0; i < Count; ++i)
		{
			if (bone[i].distance_to(bone[i + 1]) > 41 && origin.distance_to(bone[i]) > 200) //118 ~= 3metres, 39~= 1metre
				return false;
		}
		return true;
	}

	bool in_game()
	{
		return *(int*)(g_data::base + offsets::game_mode) > 1;
	}

	GAME_MODE get_game_mode() {

		auto game = *(int*)(g_data::base + offsets::game_mode);
		switch (game)
		{
		case sdk::PLUNDER:
			return sdk::PLUNDER;
		case sdk::REBIRTH:
			return sdk::REBIRTH;
		case sdk::BR:
			return sdk::BR;
		case sdk::BR_TRAINING:
			return sdk::BR_TRAINING;
		case sdk::TRAINING:
			return sdk::TRAINING;
		}
	}

	int get_max_player_count()
	{
		return *(int*)(g_data::base + offsets::game_mode);
	}

	player_t get_player(int i)
	{
		uint64_t decryptedPtr = decryption::get_client_info();

		if (is_valid_ptr(decryptedPtr))
		{
			uint64_t client_info = decryption::get_client_info_base();

			if (is_valid_ptr(client_info))
			{
				return player_t(client_info + (i * offsets::player::size));
			}
		}
		return player_t(NULL);
	}

	player_t get_local_player()
	{
		uint64_t decryptedPtr = decryption::get_client_info();

		if (is_bad_ptr(decryptedPtr))
			return player_t(NULL);

		auto local_index = *(uintptr_t*)(decryptedPtr + offsets::local_index);
		if (is_bad_ptr(local_index))
			return player_t(NULL);

		auto index = *(int*)(local_index + offsets::local_index_pos);
		return get_player(index);
	}

	int local_index()
	{
		uint64_t decryptedPtr = decryption::get_client_info();

		if (is_valid_ptr(decryptedPtr))
		{
			auto local_index = *(uintptr_t*)(decryptedPtr + offsets::local_index);
			return *(int*)(local_index + offsets::local_index_pos);
		}
		return 0;
	}

	refdef_t* get_refdef()
	{
		uint32_t crypt_0 = *(uint32_t*)(g_data::base + offsets::refdef);
		uint32_t crypt_1 = *(uint32_t*)(g_data::base + offsets::refdef + 0x4);
		uint32_t crypt_2 = *(uint32_t*)(g_data::base + offsets::refdef + 0x8);
		// lower 32 bits
		uint32_t entry_1 = (uint32_t)(g_data::base + offsets::refdef);
		uint32_t entry_2 = (uint32_t)(g_data::base + offsets::refdef + 0x4);
		// decryption
		uint32_t _low = entry_1 ^ crypt_2;
		uint32_t _high = entry_2 ^ crypt_2;
		uint32_t low_bit = crypt_0 ^ _low * (_low + 2);
		uint32_t high_bit = crypt_1 ^ _high * (_high + 2);
		return (refdef_t*)(((uint64_t)high_bit << 32) + low_bit);
	}

	vec3_t get_camera_pos()
	{
		vec3_t pos = vec3_t{};

		auto camera_ptr = *(uint64_t*)(g_data::base + offsets::camera_base);

		if (is_bad_ptr(camera_ptr))return pos;
		
		
		pos = *(vec3_t*)(camera_ptr + offsets::camera_pos);
		return pos;
	}

	std::string get_player_name(int i)
	{
		uint64_t bgs = *(uint64_t*)(g_data::base + offsets::name_array);

		if (bgs)
		{
			name_t* clientInfo_ptr = (name_t*)(bgs + offsets::name_array_pos + (i * 0xD0));
			int length = strlen(clientInfo_ptr->name);
			for (int j = 0; j < length; ++j)
			{
				char ch = clientInfo_ptr->name[j];
				bool is_english = ch >= 0 && ch <= 127;
				if (!is_english)
					return xorstr("Player");
			}
			return clientInfo_ptr->name;
		}
		return xorstr("Player");
	}

	bool get_bone_by_player_index(int i, int bone_id, vec3_t* Out_bone_pos)
	{
		uint64_t decrypted_ptr = decryption::get_bone_ptr();

		if (is_bad_ptr(decrypted_ptr))return false;
		
		unsigned short bone_index = decryption::get_bone_index(i);

		if (bone_index != 0)
		{
			uint64_t bone_ptr = *(uint64_t*)(decrypted_ptr + (bone_index * offsets::bone::index_struct_size) + 0xC0);

			if (is_bad_ptr(bone_ptr))return false;
			{
				uint64_t client_info = decryption::get_client_info();

				if (is_bad_ptr(client_info))return false;

				vec3_t bone_pos = *(vec3_t*)(bone_ptr + (bone_id * 0x20) + 0x10);

				vec3_t BasePos = *(vec3_t*)(client_info + offsets::bone::base_pos);

				bone_pos.x += BasePos.x;
				bone_pos.y += BasePos.y;
				bone_pos.z += BasePos.z;

				*Out_bone_pos = bone_pos;
				return true;
			}
		}
		
		return false;
	}

	void enable_uav()
	{
		auto cg_t = *(uint64_t*)(g_data::base + offsets::cg_t_ptr);
		//const auto offset1 = 0x304;
		//const auto enable = 33619969;
		const auto uav = 0x307;
		const auto auav = 0x306;

		if (sdk::in_game())
		{
			if (cg_t != 0)
			{
				*(bool*)(cg_t + auav) = Settings::esp::b_auav;
				*(int*)(cg_t + uav) = 0x2;
			}
		}
	}

	int get_player_health(int i)
	{
		uint64_t bgs = *(uint64_t*)(g_data::base + offsets::name_array);

		if (bgs)
		{
			sdk::name_t* info = (sdk::name_t*)(bgs + offsets::name_array_pos + (i * 0xd0));

			return info->health;


		}
		return 0;
	}

	void start_tick()
	{
		static DWORD lastTick = 0;
		DWORD t = GetTickCount();
		bUpdateTick = lastTick < t;

		if (bUpdateTick)
			lastTick = t + nTickTime;
	}

	void update_vel_map(int index, vec3_t vPos)
	{
		if (!bUpdateTick)
			return;

		velocityMap[index].delta = vPos - velocityMap[index].lastPos;
		velocityMap[index].lastPos = vPos;
	}

	void clear_vel_map()
	{
		if (!velocityMap.empty()) { velocityMap.clear(); }
	}

	vec3_t get_speed(int index)
	{
		return velocityMap[index].delta;
	}

	int get_client_count()
	{
		auto cl_info = decryption::get_client_info();
		if (is_valid_ptr(cl_info))
		{
			auto client_ptr = *(uint64_t*)(cl_info + offsets::local_index);
			if (is_valid_ptr(client_ptr))
			{
				return *(int*)(client_ptr + 0x1C);
			}
		}

		return 0;
	}

	Result MidnightSolver(float a, float b, float c)
	{
		Result res;

		double subsquare = b * b - 4 * a * c;

		if (subsquare < 0)
		{
			res.hasResult = false;
			return res;
		}
		else
		{
			res.hasResult = true,
			res.a = (float)((-b + sqrt(subsquare)) / (2 * a));
			res.b = (float)((-b - sqrt(subsquare)) / (2 * a));
		}
		return res;
	}

	uint64_t get_visible_base()
	{
		for (int32_t j = 4000; j >= 0; --j)
		{
			uint64_t n_index = (j + (j << 2)) << 0x6;
			uint64_t vis_base = *(uint64_t*)(g_data::base + offsets::distribute);

			if (is_bad_ptr(vis_base))
				continue;

			uint64_t vis_base_ptr = vis_base + n_index;
			uint64_t cmp_function = *(uint64_t*)(vis_base_ptr + 0x90);

			if (is_bad_ptr(cmp_function))
				continue;

			uint64_t about_visible = g_data::base + offsets::visible;
			if (cmp_function == about_visible)
			{
				return vis_base_ptr;
			}
		}
		return NULL;
	}

	// player class methods
	bool player_t::is_valid() {
		if (is_bad_ptr(address))return 0;

		return *(bool*)((uintptr_t)address + offsets::player::valid);
	}

	bool player_t::died() {
		if (is_bad_ptr(address))return 0;

		auto dead1 = *(bool*)((uintptr_t)address + offsets::player::dead_1);
		auto dead2 = *(bool*)((uintptr_t)address + offsets::player::dead_2);
		return dead1 || dead2;
	}

	int player_t::team_id() {

		if (is_bad_ptr(address))return 0;
		return *(int*)((uintptr_t)address + offsets::player::team);
	}

	int player_t::get_stance() {

		auto ret = *(int*)((uintptr_t)address + offsets::player::stance);
		return ret;
	}

	vec3_t player_t::get_pos() 
	{
		if (is_bad_ptr(address))return {};
		auto local_pos_ptr = *(uintptr_t*)((uintptr_t)address + offsets::player::pos);

		if (is_bad_ptr(local_pos_ptr))return{};
		else
			return *(vec3_t*)(local_pos_ptr + 0x40);
		return vec3_t{}; 
	}

	float player_t::get_rotation()
	{
		if (is_bad_ptr(address))return 0;
		auto local_pos_ptr = *(uintptr_t*)((uintptr_t)address + offsets::player::pos);

		if (is_bad_ptr(local_pos_ptr))return 0;

		auto rotation = *(float*)(local_pos_ptr + 0x58);

		if (rotation < 0)
			rotation = 360.0f - (rotation * -1);

		rotation += 90.0f;

		if (rotation >= 360.0f)
			rotation = rotation - 360.0f;

		return rotation;
	}

	uint32_t player_t::get_index()
	{
		auto cl_info = decryption::get_client_info();
		if (is_valid_ptr(cl_info))
		{
			auto cl_info_base = decryption::get_client_info_base();
			if (is_valid_ptr(cl_info_base))
			{
				return ((uintptr_t)address - cl_info_base) / offsets::player::size;
			}
			return 0;
		}
	}

	bool player_t::is_visible()
	{
		if (is_valid_ptr(g_data::visible_base))
		{
			uint64_t VisibleList = *(uint64_t*)(g_data::visible_base + 0x108);
			if (is_bad_ptr(VisibleList))
				return false;

			uint64_t rdx = VisibleList + (player_t::get_index() * 9 + 0x14E) * 8;
			if (is_bad_ptr(rdx))
				return false;

			DWORD VisibleFlags = (rdx + 0x10) ^ *(DWORD*)(rdx + 0x14);
			if (is_bad_ptr(VisibleFlags))
				return false;

			DWORD v511 = VisibleFlags * (VisibleFlags + 2);
			if (is_bad_ptr(v511))
				return false;

			BYTE VisibleFlags1 = *(DWORD*)(rdx + 0x10) ^ v511 ^ BYTE1(v511);
			if (VisibleFlags1 == 3) {
				return true;
			}
		}
		return false;
	}

	vec2_t get_camera_angles() {
		auto camera = *(uintptr_t*)(g_data::base + offsets::camera_base);
		if (!camera)
			return {};
		return *(vec2_t*)(camera + offsets::camera_pos + 0xC);
	}

	float get_fov() {
		auto radians_to_degrees = [](float radians) { return radians * 180 / static_cast<float>(M_PI); };
		vec2_t fov(g_data::refdef->FovX, g_data::refdef->FovY);
		return radians_to_degrees(atan(fov.length()) * 2.0);
	}

    auto loot::get_name() -> char*
    {
        auto ptr = *(uintptr_t*)(this + 0);
        if (is_bad_ptr(ptr))
            return NULL;

        return *(char**)(ptr);
    }

    auto loot::is_valid() -> bool
    {
        return *(char*)(this + 0xC8) == 1 && *(char*)(this + 0xC9) == 1;
    }

    auto loot::get_position() -> vec3_t
    {
        return *(vec3_t*)(this + 0x160);
    }

    auto get_loot_by_id(const uint32_t id) -> loot*
    {
        return (loot*)(g_data::base + offsets::loot_ptr) + (static_cast<uint64_t>(id) * 0x250);

    }


	bool check_valid_bones(unsigned long i, vec3_t origem)
	{
		vec3_t header_to_bladder[6], right_foot_to_bladder[5], left_foot_to_bladder[5], right_hand[5], left_hand[5];
		vec2_t screen_header_to_bladder[6], screen_right_foot_to_bladder[5], screen_left_foot_to_bladder[5], screen_right_hand[5], screen_left_hand[5]; screen_left_hand[5];

		if (sdk::get_bone_by_player_index(i, sdk::BONE_POS_HEAD, &header_to_bladder[0]))
		{
			return true;
		}
		else
		{
			return false;
		}

		return true;
	}

}

namespace decryption
{
    uintptr_t get_client_info()
    {
        auto imageBase = g_data::base;
        auto Peb = __readgsqword(0x60);
        uint64_t rax = imageBase, rbx = imageBase, rcx = imageBase, rdx = imageBase, rdi = imageBase, rsi = imageBase, r8 = imageBase, r9 = imageBase, r10 = imageBase, r11 = imageBase, r12 = imageBase, r13 = imageBase, r14 = imageBase, r15 = imageBase;
        rbx = *(uint64_t*)(imageBase + 0x1ED52A08);
        if (!rbx)
            return rbx;
        rdx = Peb;              //mov rdx, gs:[rax]
        rax = rbx;              //mov rax, rbx
        rax >>= 0x14;           //shr rax, 0x14
        r8 = 0x5E6F7BF32D916FFD;                //mov r8, 0x5E6F7BF32D916FFD
        rbx ^= rax;             //xor rbx, rax
        rax = rbx;              //mov rax, rbx
        rax >>= 0x28;           //shr rax, 0x28
        rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
        rbx ^= rax;             //xor rbx, rax
        rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
        rcx ^= *(uint64_t*)(imageBase + 0x741110F);          //xor rcx, [0x0000000004FF2C89]
        rax = imageBase + 0xEF58;               //lea rax, [0xFFFFFFFFFDBF0ACB]
        rbx -= r8;              //sub rbx, r8
        rcx = ~rcx;             //not rcx
        rbx ^= rdx;             //xor rbx, rdx
        rbx ^= rax;             //xor rbx, rax
        rax = 0x3E2F8F93EA2DD463;               //mov rax, 0x3E2F8F93EA2DD463
        rbx *= *(uint64_t*)(rcx + 0x13);             //imul rbx, [rcx+0x13]
        rbx *= rax;             //imul rbx, rax
        rax = rbx;              //mov rax, rbx
        rax >>= 0x28;           //shr rax, 0x28
        rbx ^= rax;             //xor rbx, rax
        return rbx;
    }
    uintptr_t get_client_info_base() {
        auto imageBase = g_data::base;
        auto Peb = __readgsqword(0x60);
        uint64_t rax = imageBase, rbx = imageBase, rcx = imageBase, rdx = imageBase, rdi = imageBase, rsi = imageBase, r8 = imageBase, r9 = imageBase, r10 = imageBase, r11 = imageBase, r12 = imageBase, r13 = imageBase, r14 = imageBase, r15 = imageBase;
        rax = *(uint64_t*)(get_client_info() + 0xae648);
        if (!rax)
            return rax;
        rbx = Peb;              //mov rbx, gs:[rcx]
        rcx = rbx;              //mov rcx, rbx
        rcx = _rotr64(rcx, 0x13);               //ror rcx, 0x13
        rcx &= 0xF;
        switch (rcx) {
        case 0:
        {
            r15 = imageBase + 0x78B8;               //lea r15, [0xFFFFFFFFFDBE937F]
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FF2BC3]
            r14 = 0x7F085A93590E018A;               //mov r14, 0x7F085A93590E018A
            rax += r14;             //add rax, r14
            r14 = 0x2273C0B434EBAD47;               //mov r14, 0x2273C0B434EBAD47
            rax ^= r14;             //xor rax, r14
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rcx = rbx;              //mov rcx, rbx
            rcx = ~rcx;             //not rcx
            rcx *= r15;             //imul rcx, r15
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x1D;           //shr rcx, 0x1D
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x3A;           //shr rcx, 0x3A
            rax ^= rcx;             //xor rax, rcx
            rcx = 0x18B705E542D132B3;               //mov rcx, 0x18B705E542D132B3
            rax *= rcx;             //imul rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x1E;           //shr rcx, 0x1E
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x3C;           //shr rcx, 0x3C
            rax ^= rcx;             //xor rax, rcx
            rcx = rbx;              //mov rcx, rbx
            rcx = ~rcx;             //not rcx
            uintptr_t RSP_0xFFFFFFFFFFFFFFA0;
            RSP_0xFFFFFFFFFFFFFFA0 = imageBase + 0xE868;            //lea rcx, [0xFFFFFFFFFDBF0344] : RBP+0xFFFFFFFFFFFFFFA0
            rcx += RSP_0xFFFFFFFFFFFFFFA0;          //add rcx, [rbp-0x60]
            rax ^= rcx;             //xor rax, rcx
            return rax;
        }
        case 1:
        {
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FF2810]
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBE16B2]
            rdx = imageBase + 0x56E5F09F;           //lea rdx, [0x0000000054A406F7]
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x13;           //shr rcx, 0x13
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x26;           //shr rcx, 0x26
            rax ^= rcx;             //xor rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rcx = 0x6A320FECB6D195C2;               //mov rcx, 0x6A320FECB6D195C2
            rax ^= rcx;             //xor rax, rcx
            rcx = rdx;              //mov rcx, rdx
            rcx = ~rcx;             //not rcx
            rcx += rbx;             //add rcx, rbx
            rax += rcx;             //add rax, rcx
            rax ^= r11;             //xor rax, r11
            rcx = imageBase + 0x7C185F9B;           //lea rcx, [0x0000000079D673DE]
            rcx = ~rcx;             //not rcx
            rcx -= rbx;             //sub rcx, rbx
            rax += rcx;             //add rax, rcx
            rcx = 0x12E62C9D44E6270B;               //mov rcx, 0x12E62C9D44E6270B
            rax *= rcx;             //imul rax, rcx
            rcx = 0x4F345B2E15933B55;               //mov rcx, 0x4F345B2E15933B55
            rax -= rcx;             //sub rax, rcx
            return rax;
        }
        case 2:
        {
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBE11FE]
            r9 = *(uint64_t*)(imageBase + 0x741114B);            //mov r9, [0x0000000004FF22B7]
            rcx = rbx;              //mov rcx, rbx
            rcx = ~rcx;             //not rcx
            uintptr_t RSP_0x48;
            RSP_0x48 = imageBase + 0xBADE;          //lea rcx, [0xFFFFFFFFFDBECC56] : RSP+0x48
            rcx += RSP_0x48;                //add rcx, [rsp+0x48]
            rax ^= rcx;             //xor rax, rcx
            rcx = 0xC929D29314A2E41B;               //mov rcx, 0xC929D29314A2E41B
            rax *= rcx;             //imul rax, rcx
            rax += rbx;             //add rax, rbx
            rcx = 0x431100F64BCCECE7;               //mov rcx, 0x431100F64BCCECE7
            rax += rcx;             //add rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r9;              //xor rcx, r9
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x18;           //shr rcx, 0x18
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x30;           //shr rcx, 0x30
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0xC;            //shr rcx, 0x0C
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x18;           //shr rcx, 0x18
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x30;           //shr rcx, 0x30
            rax ^= rcx;             //xor rax, rcx
            rax += r11;             //add rax, r11
            return rax;
        }
        case 3:
        {
            r14 = imageBase + 0x46600CD9;           //lea r14, [0x00000000441E198A]
            r9 = *(uint64_t*)(imageBase + 0x741114B);            //mov r9, [0x0000000004FF1D87]
            rcx = 0xD549FB4A91EB98A5;               //mov rcx, 0xD549FB4A91EB98A5
            rax *= rcx;             //imul rax, rcx
            rcx = 0x19ADDC53F1BABC1D;               //mov rcx, 0x19ADDC53F1BABC1D
            rax += rcx;             //add rax, rcx
            rcx = r14;              //mov rcx, r14
            rcx -= rbx;             //sub rcx, rbx
            rax ^= rcx;             //xor rax, rcx
            rax ^= rbx;             //xor rax, rbx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r9;              //xor rcx, r9
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x26;           //shr rcx, 0x26
            rax ^= rcx;             //xor rax, rcx
            rax += rbx;             //add rax, rbx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x1A;           //shr rcx, 0x1A
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x34;           //shr rcx, 0x34
            rax ^= rcx;             //xor rax, rcx
            return rax;
        }
        case 4:
        {
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBE07AD]
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FF1885]
            rcx = 0x527CA2DCBC3CFB52;               //mov rcx, 0x527CA2DCBC3CFB52
            rax -= rcx;             //sub rax, rcx
            rax += r11;             //add rax, r11
            rax += rbx;             //add rax, rbx
            rcx = rbx;              //mov rcx, rbx
            uintptr_t RSP_0x48;
            RSP_0x48 = imageBase + 0x3BDC6E94;              //lea rcx, [0x00000000399A7668] : RSP+0x48
            rcx *= RSP_0x48;                //imul rcx, [rsp+0x48]
            rax += rcx;             //add rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x3;            //shr rcx, 0x03
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x6;            //shr rcx, 0x06
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0xC;            //shr rcx, 0x0C
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x18;           //shr rcx, 0x18
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x30;           //shr rcx, 0x30
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0xD;            //shr rcx, 0x0D
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x1A;           //shr rcx, 0x1A
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x34;           //shr rcx, 0x34
            rax ^= rcx;             //xor rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rcx = *(uint64_t*)(rcx + 0x15);              //mov rcx, [rcx+0x15]
            uintptr_t RSP_0xFFFFFFFFFFFFFF80;
            RSP_0xFFFFFFFFFFFFFF80 = 0x7880F51A8D2167E5;            //mov rcx, 0x7880F51A8D2167E5 : RBP+0xFFFFFFFFFFFFFF80
            rcx *= RSP_0xFFFFFFFFFFFFFF80;          //imul rcx, [rbp-0x80]
            rax *= rcx;             //imul rax, rcx
            return rax;
        }
        case 5:
        {
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FF1402]
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBE02A4]
            rdx = imageBase + 0xB54F;               //lea rdx, [0xFFFFFFFFFDBEB78D]
            rax += r11;             //add rax, r11
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rax ^= rbx;             //xor rax, rbx
            rax ^= rdx;             //xor rax, rdx
            rcx = 0x341C623060C268FB;               //mov rcx, 0x341C623060C268FB
            rax *= rcx;             //imul rax, rcx
            rcx = 0x8E6772456E650B49;               //mov rcx, 0x8E6772456E650B49
            rax ^= rcx;             //xor rax, rcx
            rax -= r11;             //sub rax, r11
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x1E;           //shr rcx, 0x1E
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x3C;           //shr rcx, 0x3C
            rax ^= rcx;             //xor rax, rcx
            rcx = 0xAF47B004AECAC3D4;               //mov rcx, 0xAF47B004AECAC3D4
            rax ^= rcx;             //xor rax, rcx
            return rax;
        }
        case 6:
        {
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FF0FCF]
            rax -= rbx;             //sub rax, rbx
            rcx = imageBase + 0xAE26;               //lea rcx, [0xFFFFFFFFFDBEA93D]
            rcx = ~rcx;             //not rcx
            rcx -= rbx;             //sub rcx, rbx
            rax += rcx;             //add rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x18;           //shr rcx, 0x18
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x30;           //shr rcx, 0x30
            rax ^= rcx;             //xor rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rdx = rbx;              //mov rdx, rbx
            rdx = ~rdx;             //not rdx
            rcx = imageBase + 0x2174FA7A;           //lea rcx, [0x000000001F32F413]
            rcx = ~rcx;             //not rcx
            rdx *= rcx;             //imul rdx, rcx
            rax += rdx;             //add rax, rdx
            rcx = 0x6D36DFFFD4B328B;                //mov rcx, 0x6D36DFFFD4B328B
            rax *= rcx;             //imul rax, rcx
            rcx = 0x8B9B0857127A3C2C;               //mov rcx, 0x8B9B0857127A3C2C
            rax ^= rcx;             //xor rax, rcx
            rcx = 0xC161BF0D6F53E93D;               //mov rcx, 0xC161BF0D6F53E93D
            rax *= rcx;             //imul rax, rcx
            return rax;
        }
        case 7:
        {
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FF0A47]
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBDF8E9]
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rax ^= rbx;             //xor rax, rbx
            rcx = rbx;              //mov rcx, rbx
            rcx -= r11;             //sub rcx, r11
            rcx += 0xFFFFFFFFBF29D9F9;              //add rcx, 0xFFFFFFFFBF29D9F9
            rax += rcx;             //add rax, rcx
            rcx = 0xEB771554EC2B363F;               //mov rcx, 0xEB771554EC2B363F
            rax *= rcx;             //imul rax, rcx
            rcx = 0x3EEF0F285DCA6A7C;               //mov rcx, 0x3EEF0F285DCA6A7C
            rax ^= rcx;             //xor rax, rcx
            rcx = 0x52268462CCFC82F2;               //mov rcx, 0x52268462CCFC82F2
            rax -= rcx;             //sub rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x22;           //shr rcx, 0x22
            rax ^= rcx;             //xor rax, rcx
            rcx = imageBase + 0x2DA5;               //lea rcx, [0xFFFFFFFFFDBE21F2]
            rax -= rbx;             //sub rax, rbx
            rax += rcx;             //add rax, rcx
            return rax;
        }
        case 8:
        {
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FF053C]
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBDF3DE]
            r15 = 0xD0DFF84B49A0E735;               //mov r15, 0xD0DFF84B49A0E735
            rcx = rax + r11 * 1;            //lea rcx, [rax+r11*1]
            rax = 0x34B811E17BD00AC9;               //mov rax, 0x34B811E17BD00AC9
            rcx *= rax;             //imul rcx, rax
            rax = imageBase + 0x9E5E;               //lea rax, [0xFFFFFFFFFDBE8DEF]
            rax = ~rax;             //not rax
            rcx += rbx;             //add rcx, rbx
            rax += r15;             //add rax, r15
            rax += rcx;             //add rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x4;            //shr rcx, 0x04
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x8;            //shr rcx, 0x08
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x10;           //shr rcx, 0x10
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x20;           //shr rcx, 0x20
            rax ^= rcx;             //xor rax, rcx
            rcx = 0x62C9A67C9A700B03;               //mov rcx, 0x62C9A67C9A700B03
            rax *= rcx;             //imul rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rax += rbx;             //add rax, rbx
            return rax;
        }
        case 9:
        {
            r9 = *(uint64_t*)(imageBase + 0x741114B);            //mov r9, [0x0000000004FF0083]
            rcx = 0xD2BB91E76F53D70;                //mov rcx, 0xD2BB91E76F53D70
            rax -= rcx;             //sub rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x9;            //shr rcx, 0x09
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x12;           //shr rcx, 0x12
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x24;           //shr rcx, 0x24
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0xB;            //shr rcx, 0x0B
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x16;           //shr rcx, 0x16
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x2C;           //shr rcx, 0x2C
            rax ^= rcx;             //xor rax, rcx
            rcx = 0x41DD76B0C354E076;               //mov rcx, 0x41DD76B0C354E076
            rax -= rcx;             //sub rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x16;           //shr rcx, 0x16
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x2C;           //shr rcx, 0x2C
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0xF;            //shr rcx, 0x0F
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x1E;           //shr rcx, 0x1E
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x3C;           //shr rcx, 0x3C
            rax ^= rcx;             //xor rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r9;              //xor rcx, r9
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rcx = 0x5E76E3EFACC891DB;               //mov rcx, 0x5E76E3EFACC891DB
            rax *= rcx;             //imul rax, rcx
            return rax;
        }
        case 10:
        {
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FEFA9C]
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBDE93E]
            rcx = 0xC4CA6C12DF008F9B;               //mov rcx, 0xC4CA6C12DF008F9B
            rax *= rcx;             //imul rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x19;           //shr rcx, 0x19
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x32;           //shr rcx, 0x32
            rax ^= rcx;             //xor rax, rcx
            rcx = 0x43489384F8792BA3;               //mov rcx, 0x43489384F8792BA3
            rax -= rcx;             //sub rax, rcx
            rdx = 0;                //and rdx, 0xFFFFFFFFC0000000
            rdx = _rotl64(rdx, 0x10);               //rol rdx, 0x10
            rdx ^= r10;             //xor rdx, r10
            rcx = r11 + 0x1755108b;                 //lea rcx, [r11+0x1755108B]
            rcx += rbx;             //add rcx, rbx
            rdx = ~rdx;             //not rdx
            rax ^= rcx;             //xor rax, rcx
            rcx = 0xA2EE3D7BF725ACD;                //mov rcx, 0xA2EE3D7BF725ACD
            rax *= *(uint64_t*)(rdx + 0x15);             //imul rax, [rdx+0x15]
            rax *= rcx;             //imul rax, rcx
            return rax;
        }
        case 11:
        {
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBDE484]
            r15 = imageBase + 0x39DA1ABF;           //lea r15, [0x000000003797FF37]
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FEF576]
            rdx = rbx;              //mov rdx, rbx
            rdx = ~rdx;             //not rdx
            rax += r15;             //add rax, r15
            rax += rdx;             //add rax, rdx
            rcx = 0x7C8D2EF677E8684B;               //mov rcx, 0x7C8D2EF677E8684B
            rax -= rcx;             //sub rax, rcx
            rax ^= r11;             //xor rax, r11
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rax -= r11;             //sub rax, r11
            rcx = 0x46F95E7D57EC4321;               //mov rcx, 0x46F95E7D57EC4321
            rax *= rcx;             //imul rax, rcx
            rcx = 0x2FA98246C6AE4C5;                //mov rcx, 0x2FA98246C6AE4C5
            rax += rcx;             //add rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x21;           //shr rcx, 0x21
            rax ^= rcx;             //xor rax, rcx
            return rax;
        }
        case 12:
        {
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBDE054]
            r15 = imageBase + 0x40BC;               //lea r15, [0xFFFFFFFFFDBE2104]
            r9 = *(uint64_t*)(imageBase + 0x741114B);            //mov r9, [0x0000000004FEF14A]
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r9;              //xor rcx, r9
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rax += r11;             //add rax, r11
            rax ^= rbx;             //xor rax, rbx
            rax ^= r15;             //xor rax, r15
            rcx = 0x26E13035D1B7A93A;               //mov rcx, 0x26E13035D1B7A93A
            rax ^= rcx;             //xor rax, rcx
            rcx = 0x92E616F6D6E7D709;               //mov rcx, 0x92E616F6D6E7D709
            rax *= rcx;             //imul rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x1D;           //shr rcx, 0x1D
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x3A;           //shr rcx, 0x3A
            rax ^= rcx;             //xor rax, rcx
            rax += r11;             //add rax, r11
            return rax;
        }
        case 13:
        {
            r11 = imageBase;                //lea r11, [0xFFFFFFFFFDBDDC69]
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FEED55]
            rax -= r11;             //sub rax, r11
            rcx = 0x50D41ACA0A9DBE9F;               //mov rcx, 0x50D41ACA0A9DBE9F
            rax *= rcx;             //imul rax, rcx
            rdx = rbx;              //mov rdx, rbx
            rdx = ~rdx;             //not rdx
            rcx = imageBase + 0x4B7B233F;           //lea rcx, [0x000000004938FC3E]
            rax += rcx;             //add rax, rcx
            rax += rdx;             //add rax, rdx
            rcx = 0xFBAEACB69D3E9055;               //mov rcx, 0xFBAEACB69D3E9055
            rax *= rcx;             //imul rax, rcx
            rax ^= r11;             //xor rax, r11
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x18;           //shr rcx, 0x18
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x30;           //shr rcx, 0x30
            rax ^= rcx;             //xor rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rcx = 0xF94DE3AC948EA587;               //mov rcx, 0xF94DE3AC948EA587
            rax *= rcx;             //imul rax, rcx
            return rax;
        }
        case 14:
        {
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FEE943]
            r15 = imageBase + 0xEE7;                //lea r15, [0xFFFFFFFFFDBDE6C1]
            rdx = imageBase + 0x77E930C8;           //lea rdx, [0x0000000075A7083C]
            rcx = rbx;              //mov rcx, rbx
            rcx = ~rcx;             //not rcx
            uintptr_t RSP_0xFFFFFFFFFFFFFFA0;
            RSP_0xFFFFFFFFFFFFFFA0 = imageBase + 0x7E52;            //lea rcx, [0xFFFFFFFFFDBE5643] : RBP+0xFFFFFFFFFFFFFFA0
            rcx ^= RSP_0xFFFFFFFFFFFFFFA0;          //xor rcx, [rbp-0x60]
            rax += rcx;             //add rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x8;            //shr rcx, 0x08
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x10;           //shr rcx, 0x10
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rdx -= rbx;             //sub rdx, rbx
            rcx >>= 0x20;           //shr rcx, 0x20
            rdx ^= rcx;             //xor rdx, rcx
            rax ^= rdx;             //xor rax, rdx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x27;           //shr rcx, 0x27
            rax ^= rcx;             //xor rax, rcx
            rcx = 0xFF5B8549DF1C290D;               //mov rcx, 0xFF5B8549DF1C290D
            rax *= rcx;             //imul rax, rcx
            rcx = r15;              //mov rcx, r15
            rcx -= rbx;             //sub rcx, rbx
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x19;           //shr rcx, 0x19
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x32;           //shr rcx, 0x32
            rax ^= rcx;             //xor rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            return rax;
        }
        case 15:
        {
            r14 = imageBase + 0x26A2E4E2;           //lea r14, [0x000000002460B7D4]
            r15 = imageBase + 0x1E4DD2E1;           //lea r15, [0x000000001C0BA5C7]
            r10 = *(uint64_t*)(imageBase + 0x741114B);           //mov r10, [0x0000000004FEE3B3]
            rcx = 0x1;              //mov ecx, 0x01
            rcx -= r14;             //sub rcx, r14
            rcx *= rbx;             //imul rcx, rbx
            rax += rcx;             //add rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x21;           //shr rcx, 0x21
            rax ^= rcx;             //xor rax, rcx
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rcx ^= r10;             //xor rcx, r10
            rcx = ~rcx;             //not rcx
            rax *= *(uint64_t*)(rcx + 0x15);             //imul rax, [rcx+0x15]
            rcx = 0x138518BE6EE64EFF;               //mov rcx, 0x138518BE6EE64EFF
            rax *= rcx;             //imul rax, rcx
            rcx = rbx;              //mov rcx, rbx
            rcx *= r15;             //imul rcx, r15
            rax -= rcx;             //sub rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0xC;            //shr rcx, 0x0C
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x18;           //shr rcx, 0x18
            rax ^= rcx;             //xor rax, rcx
            rcx = rax;              //mov rcx, rax
            rcx >>= 0x30;           //shr rcx, 0x30
            rax ^= rcx;             //xor rax, rcx
            rcx = imageBase + 0x7789BB5B;           //lea rcx, [0x0000000075478B78]
            rax -= rbx;             //sub rax, rbx
            rax += rcx;             //add rax, rcx
            return rax;
        }
        }
    }
    uintptr_t get_bone_ptr()
    {
        auto imageBase = g_data::base;
        auto Peb = __readgsqword(0x60);
        uint64_t rax = imageBase, rbx = imageBase, rcx = imageBase, rdx = imageBase, rdi = imageBase, rsi = imageBase, r8 = imageBase, r9 = imageBase, r10 = imageBase, r11 = imageBase, r12 = imageBase, r13 = imageBase, r14 = imageBase, r15 = imageBase;
        r8 = *(uint64_t*)(imageBase + 0x1BE68598);
        if (!r8)
            return r8;
        rbx = Peb;              //mov rbx, gs:[rax]
        rax = rbx;              //mov rax, rbx
        rax = _rotl64(rax, 0x2F);               //rol rax, 0x2F
        rax &= 0xF;
        globals::clientSwitch = rax;
        switch (rax) {
        case 0:
        {
            r11 = imageBase + 0x2CA1;          //lea r11, [0xFFFFFFFFFDBF7455]
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x00000000050059DD]
            rax = 0x8D0CFB3EDF09FA75;               //mov rax, 0x8D0CFB3EDF09FA75
            r8 ^= rax;              //xor r8, rax
            rax = 0x518F5CA9FEB339C5;               //mov rax, 0x518F5CA9FEB339C5
            r8 *= rax;              //imul r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x8;            //shr rax, 0x08
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x10;           //shr rax, 0x10
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x20;           //shr rax, 0x20
            r8 ^= rax;              //xor r8, rax
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rax = r11;              //mov rax, r11
            rcx ^= r10;             //xor rcx, r10
            rax = ~rax;             //not rax
            rcx = _byteswap_uint64(rcx);            //bswap rcx
            r8 *= *(uint64_t*)(rcx + 0x17);                 //imul r8, [rcx+0x17]
            r8 -= rbx;              //sub r8, rbx
            r8 ^= rax;              //xor r8, rax
            r8 ^= rbx;              //xor r8, rbx
            r8 += rbx;              //add r8, rbx
            rax = 0x42BBAAF60EA8A492;               //mov rax, 0x42BBAAF60EA8A492
            r8 ^= rax;              //xor r8, rax
            return r8;
        }
        case 1:
        {
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x00000000050055B1]
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF433D]
            rax = r8;               //mov rax, r8
            rax >>= 0x18;           //shr rax, 0x18
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x30;           //shr rax, 0x30
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x19;           //shr rax, 0x19
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x32;           //shr rax, 0x32
            r8 ^= rax;              //xor r8, rax
            r8 ^= rdi;              //xor r8, rdi
            rax = rbx;              //mov rax, rbx
            rax = ~rax;             //not rax
            r8 += rax;              //add r8, rax
            rax = imageBase + 0x6217791A;              //lea rax, [0x000000005FD6BA59]
            r8 += rax;              //add r8, rax
            rax = 0x6B93351967D1907F;               //mov rax, 0x6B93351967D1907F
            r8 *= rax;              //imul r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r10;             //xor rax, r10
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = 0xA07BEA13371AD43;                //mov rax, 0xA07BEA13371AD43
            r8 += rax;              //add r8, rax
            rax = 0xD079D59BE8486DCD;               //mov rax, 0xD079D59BE8486DCD
            r8 *= rax;              //imul r8, rax
            return r8;
        }
        case 2:
        {
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF3E04]
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x0000000005005000]
            rax = r8;               //mov rax, r8
            rax >>= 0x1F;           //shr rax, 0x1F
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x3E;           //shr rax, 0x3E
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x20;           //shr rax, 0x20
            r8 ^= rax;              //xor r8, rax
            rax = 0xC0CE1A130F381529;               //mov rax, 0xC0CE1A130F381529
            r8 *= rax;              //imul r8, rax
            rax = 0x408695D43D99E96D;               //mov rax, 0x408695D43D99E96D
            r8 += rax;              //add r8, rax
            rax = 0x6196B321368E3A05;               //mov rax, 0x6196B321368E3A05
            r8 *= rax;              //imul r8, rax
            r8 -= rdi;              //sub r8, rdi
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r10;             //xor rax, r10
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            r8 ^= rdi;              //xor r8, rdi
            return r8;
        }
        case 3:
        {
            rcx = imageBase + 0x44C017B3;              //lea rcx, [0x00000000427F4FC9]
            r13 = imageBase + 0x39CCCB31;              //lea r13, [0x00000000378C0328]
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x0000000005004A27]
            rax = r8;               //mov rax, r8
            rax >>= 0x16;           //shr rax, 0x16
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x2C;           //shr rax, 0x2C
            r8 ^= rax;              //xor r8, rax
            rax = 0x29F89A20CB1DD9AD;               //mov rax, 0x29F89A20CB1DD9AD
            r8 += rax;              //add r8, rax
            r8 -= rbx;              //sub r8, rbx
            rax = 0xA8257C2554044DF;                //mov rax, 0xA8257C2554044DF
            r8 *= rax;              //imul r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r10;             //xor rax, r10
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = 0xB1345058AADAF45B;               //mov rax, 0xB1345058AADAF45B
            r8 ^= rbx;              //xor r8, rbx
            r8 ^= r13;              //xor r8, r13
            r8 ^= rax;              //xor r8, rax
            rax = rbx;              //mov rax, rbx
            rax *= rcx;             //imul rax, rcx
            r8 ^= rax;              //xor r8, rax
            return r8;
        }
        case 4:
        {
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x00000000050045A9]
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF3335]
            r13 = imageBase + 0x26998716;              //lea r13, [0x000000002458BA40]
            rcx = imageBase + 0x5A49;          //lea rcx, [0xFFFFFFFFFDBF8D27]
            rax = 0x71FBCBD5B8BFB518;               //mov rax, 0x71FBCBD5B8BFB518
            r8 -= rax;              //sub r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x5;            //shr rax, 0x05
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0xA;            //shr rax, 0x0A
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x14;           //shr rax, 0x14
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x28;           //shr rax, 0x28
            r8 ^= rax;              //xor r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r10;             //xor rax, r10
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            r8 ^= rbx;              //xor r8, rbx
            rax = rbx;              //mov rax, rbx
            rax = ~rax;             //not rax
            rax += rcx;             //add rax, rcx
            r8 ^= rax;              //xor r8, rax
            rax = 0xF0E437B74CF06F55;               //mov rax, 0xF0E437B74CF06F55
            r8 *= rax;              //imul r8, rax
            r8 ^= rdi;              //xor r8, rdi
            rax = r13;              //mov rax, r13
            rax = ~rax;             //not rax
            rax += rbx;             //add rax, rbx
            r8 += rax;              //add r8, rax
            return r8;
        }
        case 5:
        {
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF2DB1]
            r9 = *(uint64_t*)(imageBase + 0x7411261);          //mov r9, [0x0000000005003FDE]
            r8 += rbx;              //add r8, rbx
            rax = r8;               //mov rax, r8
            rax >>= 0x22;           //shr rax, 0x22
            r8 ^= rax;              //xor r8, rax
            rax = 0xAFF7C9FA2085EAF9;               //mov rax, 0xAFF7C9FA2085EAF9
            r8 ^= rax;              //xor r8, rax
            r8 += rdi;              //add r8, rdi
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r9;              //xor rax, r9
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = r8;               //mov rax, r8
            rax >>= 0x26;           //shr rax, 0x26
            r8 ^= rax;              //xor r8, rax
            r8 ^= rdi;              //xor r8, rdi
            rax = 0xF7404BB4AD97295F;               //mov rax, 0xF7404BB4AD97295F
            r8 *= rax;              //imul r8, rax
            return r8;
        }
        case 6:
        {
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF29CC]
            r11 = imageBase + 0xEDDD;          //lea r11, [0xFFFFFFFFFDC0179D]
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x0000000005003C0C]
            r8 += rdi;              //add r8, rdi
            rcx = r11;              //mov rcx, r11
            rcx = ~rcx;             //not rcx
            rcx ^= rbx;             //xor rcx, rbx
            r8 += rcx;              //add r8, rcx
            rax = r8;               //mov rax, r8
            rax >>= 0x16;           //shr rax, 0x16
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x2C;           //shr rax, 0x2C
            r8 ^= rax;              //xor r8, rax
            rax = 0x5B9A94B12A7A9CAF;               //mov rax, 0x5B9A94B12A7A9CAF
            r8 += rax;              //add r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r10;             //xor rax, r10
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = 0x9733D87D532BF667;               //mov rax, 0x9733D87D532BF667
            r8 *= rax;              //imul r8, rax
            rax = 0x7D770CCDF5B3AEEA;               //mov rax, 0x7D770CCDF5B3AEEA
            r8 += rax;              //add r8, rax
            rax = rdi + 0x81b8;             //lea rax, [rdi+0x81B8]
            rax += rbx;             //add rax, rbx
            r8 += rax;              //add r8, rax
            return r8;
        }
        case 7:
        {
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x0000000005003845]
            r13 = imageBase + 0x4A80C763;              //lea r13, [0x00000000483FED34]
            rax = r8;               //mov rax, r8
            rax >>= 0x15;           //shr rax, 0x15
            r8 ^= rax;              //xor r8, rax
            rcx = 0;                //and rcx, 0xFFFFFFFFC0000000
            rax = r8;               //mov rax, r8
            rcx = _rotl64(rcx, 0x10);               //rol rcx, 0x10
            rax >>= 0x2A;           //shr rax, 0x2A
            rcx ^= r10;             //xor rcx, r10
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x1A;           //shr rax, 0x1A
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x34;           //shr rax, 0x34
            r8 ^= rax;              //xor r8, rax
            rax = 0xA728509DBD2B2B1F;               //mov rax, 0xA728509DBD2B2B1F
            rcx = _byteswap_uint64(rcx);            //bswap rcx
            r8 *= *(uint64_t*)(rcx + 0x17);                 //imul r8, [rcx+0x17]
            r8 *= rax;              //imul r8, rax
            rax = 0xDE5C80CD15767E9E;               //mov rax, 0xDE5C80CD15767E9E
            r8 ^= rax;              //xor r8, rax
            r8 ^= rbx;              //xor r8, rbx
            r8 ^= r13;              //xor r8, r13
            rax = r8;               //mov rax, r8
            rax >>= 0x27;           //shr rax, 0x27
            r8 ^= rax;              //xor r8, rax
            rax = 0x503384185FB4D599;               //mov rax, 0x503384185FB4D599
            r8 -= rax;              //sub r8, rax
            return r8;
        }
        case 8:
        {
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF20CD]
            r9 = *(uint64_t*)(imageBase + 0x7411261);          //mov r9, [0x00000000050032EF]
            rax = r8;               //mov rax, r8
            rax >>= 0x23;           //shr rax, 0x23
            r8 ^= rax;              //xor r8, rax
            rax = imageBase + 0x8AAC;          //lea rax, [0xFFFFFFFFFDBFA8AC]
            r8 += rbx;              //add r8, rbx
            r8 += rax;              //add r8, rax
            rax = 0x6EA9243FACC73C99;               //mov rax, 0x6EA9243FACC73C99
            r8 *= rax;              //imul r8, rax
            rax = 0xB7FAD6466681C8DD;               //mov rax, 0xB7FAD6466681C8DD
            r8 ^= rdi;              //xor r8, rdi
            r8 ^= rax;              //xor r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r9;              //xor rax, r9
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = 0xA076BAD18B72BC1B;               //mov rax, 0xA076BAD18B72BC1B
            r8 ^= rax;              //xor r8, rax
            rax = rbx;              //mov rax, rbx
            rax -= rdi;             //sub rax, rdi
            rax += 0xFFFFFFFFC8D343AF;              //add rax, 0xFFFFFFFFC8D343AF
            r8 += rax;              //add r8, rax
            return r8;
        }
        case 9:
        {
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF1C50]
            r9 = *(uint64_t*)(imageBase + 0x7411261);          //mov r9, [0x0000000005002E43]
            rax = r8;               //mov rax, r8
            rax >>= 0x5;            //shr rax, 0x05
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0xA;            //shr rax, 0x0A
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x14;           //shr rax, 0x14
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x28;           //shr rax, 0x28
            r8 ^= rax;              //xor r8, rax
            r8 -= rdi;              //sub r8, rdi
            rax = r8;               //mov rax, r8
            rax >>= 0x13;           //shr rax, 0x13
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x26;           //shr rax, 0x26
            r8 ^= rax;              //xor r8, rax
            rax = 0x940BE87415339ABD;               //mov rax, 0x940BE87415339ABD
            r8 *= rax;              //imul r8, rax
            rax = 0x87E2C8F57A90D89D;               //mov rax, 0x87E2C8F57A90D89D
            r8 *= rax;              //imul r8, rax
            rax = imageBase + 0x41B03473;              //lea rax, [0x000000003F6F4D1F]
            rax = ~rax;             //not rax
            rax += rbx;             //add rax, rbx
            r8 ^= rax;              //xor r8, rax
            uintptr_t RSP_0xFFFFFFFFFFFFFFB0;
            RSP_0xFFFFFFFFFFFFFFB0 = 0xCAAF31B3A5ECD3DC;            //mov rax, 0xCAAF31B3A5ECD3DC : RBP+0xFFFFFFFFFFFFFFB0
            r8 ^= RSP_0xFFFFFFFFFFFFFFB0;           //xor r8, [rbp-0x50]
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r9;              //xor rax, r9
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            return r8;
        }
        case 10:
        {
            //failed to translate: pop rbx
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF16D7]
            r9 = *(uint64_t*)(imageBase + 0x7411261);          //mov r9, [0x00000000050028EA]
            r8 -= rbx;              //sub r8, rbx
            rax = 0x5ECC330FF5FB8867;               //mov rax, 0x5ECC330FF5FB8867
            r8 *= rax;              //imul r8, rax
            rax = 0x4E53853DB7F12575;               //mov rax, 0x4E53853DB7F12575
            r8 += rdi;              //add r8, rdi
            r8 *= rax;              //imul r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r9;              //xor rax, r9
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = r8;               //mov rax, r8
            rax >>= 0x7;            //shr rax, 0x07
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0xE;            //shr rax, 0x0E
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x1C;           //shr rax, 0x1C
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x38;           //shr rax, 0x38
            r8 ^= rax;              //xor r8, rax
            r8 -= rdi;              //sub r8, rdi
            rax = r8;               //mov rax, r8
            rax >>= 0xF;            //shr rax, 0x0F
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x1E;           //shr rax, 0x1E
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x3C;           //shr rax, 0x3C
            r8 ^= rax;              //xor r8, rax
            return r8;
        }
        case 11:
        {
            r11 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r11, [0x0000000005002437]
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF11C3]
            rax = rbx;              //mov rax, rbx
            uintptr_t RSP_0xFFFFFFFFFFFFFFB8;
            RSP_0xFFFFFFFFFFFFFFB8 = imageBase + 0x3863048C;           //lea rax, [0x0000000036221618] : RBP+0xFFFFFFFFFFFFFFB8
            rax *= RSP_0xFFFFFFFFFFFFFFB8;          //imul rax, [rbp-0x48]
            rax -= rdi;             //sub rax, rdi
            rax += 0xFFFFFFFFFFFFC0D5;              //add rax, 0xFFFFFFFFFFFFC0D5
            r8 += rax;              //add r8, rax
            rax = 0xDA06655CDE4259FD;               //mov rax, 0xDA06655CDE4259FD
            r8 *= rax;              //imul r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x18;           //shr rax, 0x18
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x30;           //shr rax, 0x30
            r8 ^= rax;              //xor r8, rax
            rax = 0x439587B46C85DEC;                //mov rax, 0x439587B46C85DEC
            r8 -= rax;              //sub r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x18;           //shr rax, 0x18
            r8 ^= rax;              //xor r8, rax
            rdx = 0;                //and rdx, 0xFFFFFFFFC0000000
            rcx = r8;               //mov rcx, r8
            rdx = _rotl64(rdx, 0x10);               //rol rdx, 0x10
            rcx >>= 0x30;           //shr rcx, 0x30
            rcx ^= r8;              //xor rcx, r8
            r8 = imageBase + 0x4A041086;               //lea r8, [0x0000000047C3201B]
            rdx ^= r11;             //xor rdx, r11
            rax = rbx + 0x1;                //lea rax, [rbx+0x01]
            r8 *= rax;              //imul r8, rax
            rdx = _byteswap_uint64(rdx);            //bswap rdx
            r8 += rcx;              //add r8, rcx
            r8 *= *(uint64_t*)(rdx + 0x17);                 //imul r8, [rdx+0x17]
            return r8;
        }
        case 12:
        {
            r11 = imageBase + 0x5E3DBA79;              //lea r11, [0x000000005BFCC6A0]
            r9 = *(uint64_t*)(imageBase + 0x7411261);          //mov r9, [0x0000000005001E52]
            rax = 0x266B888BE85851B9;               //mov rax, 0x266B888BE85851B9
            r8 *= rax;              //imul r8, rax
            r8 -= rbx;              //sub r8, rbx
            rax = r11;              //mov rax, r11
            rax = ~rax;             //not rax
            rax *= rbx;             //imul rax, rbx
            r8 += rax;              //add r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x11;           //shr rax, 0x11
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x22;           //shr rax, 0x22
            r8 ^= rax;              //xor r8, rax
            rax = imageBase + 0xF104;          //lea rax, [0xFFFFFFFFFDBFF976]
            rax -= rbx;             //sub rax, rbx
            r8 += rax;              //add r8, rax
            rax = imageBase + 0x4B3FFAE8;              //lea rax, [0x0000000048FF05FC]
            rax -= rbx;             //sub rax, rbx
            r8 ^= rax;              //xor r8, rax
            rax = 0x3C79FD5CC9304EA6;               //mov rax, 0x3C79FD5CC9304EA6
            r8 ^= rax;              //xor r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r9;              //xor rax, r9
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            return r8;
        }
        case 13:
        {
            r11 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r11, [0x00000000050019D2]
            rax = r8;               //mov rax, r8
            rax >>= 0x21;           //shr rax, 0x21
            rax ^= rbx;             //xor rax, rbx
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x1;            //shr rax, 0x01
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x2;            //shr rax, 0x02
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x4;            //shr rax, 0x04
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x8;            //shr rax, 0x08
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x10;           //shr rax, 0x10
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x20;           //shr rax, 0x20
            r8 ^= rax;              //xor r8, rax
            rcx = rbx;              //mov rcx, rbx
            rax = 0x1BE0CB10CCA65C4A;               //mov rax, 0x1BE0CB10CCA65C4A
            rcx *= rax;             //imul rcx, rax
            rax = r8;               //mov rax, r8
            rdx = 0;                //and rdx, 0xFFFFFFFFC0000000
            r8 = 0xCA55A839FE99AB05;                //mov r8, 0xCA55A839FE99AB05
            r8 *= rax;              //imul r8, rax
            rdx = _rotl64(rdx, 0x10);               //rol rdx, 0x10
            rax = imageBase + 0x2EF4FD4F;              //lea rax, [0x000000002CB4024B]
            r8 += rcx;              //add r8, rcx
            rdx ^= r11;             //xor rdx, r11
            r8 += rax;              //add r8, rax
            rdx = _byteswap_uint64(rdx);            //bswap rdx
            r8 *= *(uint64_t*)(rdx + 0x17);                 //imul r8, [rdx+0x17]
            return r8;
        }
        case 14:
        {
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBF02E9]
            r13 = imageBase + 0x81F9;          //lea r13, [0xFFFFFFFFFDBF84D6]
            r9 = *(uint64_t*)(imageBase + 0x7411261);          //mov r9, [0x000000000500150C]
            rax = r8;               //mov rax, r8
            rax >>= 0x10;           //shr rax, 0x10
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x20;           //shr rax, 0x20
            r8 ^= rax;              //xor r8, rax
            r8 += rbx;              //add r8, rbx
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r9;              //xor rax, r9
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = 0xE1B04D75EC46E8FD;               //mov rax, 0xE1B04D75EC46E8FD
            r8 ^= rax;              //xor r8, rax
            rax = 0xF387745DC26BE86D;               //mov rax, 0xF387745DC26BE86D
            r8 -= rdi;              //sub r8, rdi
            r8 += rbx;              //add r8, rbx
            r8 *= rax;              //imul r8, rax
            rax = 0x695800FC7CA60B4E;               //mov rax, 0x695800FC7CA60B4E
            r8 += rax;              //add r8, rax
            rax = rbx;              //mov rax, rbx
            rax *= r13;             //imul rax, r13
            r8 ^= rax;              //xor r8, rax
            rax = 0x63E51EE439448758;               //mov rax, 0x63E51EE439448758
            r8 ^= rax;              //xor r8, rax
            return r8;
        }
        case 15:
        {
            //failed to translate: pop rbx
            r10 = *(uint64_t*)(imageBase + 0x7411261);                 //mov r10, [0x000000000500110F]
            rdi = imageBase;           //lea rdi, [0xFFFFFFFFFDBEFE96]
            r12 = imageBase + 0x2D7428E7;              //lea r12, [0x000000002B332772]
            rcx = imageBase + 0x2C59FF05;              //lea rcx, [0x000000002A18FD25]
            rax = r8;               //mov rax, r8
            rax >>= 0xF;            //shr rax, 0x0F
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x1E;           //shr rax, 0x1E
            r8 ^= rax;              //xor r8, rax
            rax = r8;               //mov rax, r8
            rax >>= 0x3C;           //shr rax, 0x3C
            r8 ^= rax;              //xor r8, rax
            rax = 0x77AFE5AF523C94A3;               //mov rax, 0x77AFE5AF523C94A3
            r8 *= rax;              //imul r8, rax
            rax = rbx;              //mov rax, rbx
            rax *= r12;             //imul rax, r12
            r8 ^= rax;              //xor r8, rax
            rax = rbx;              //mov rax, rbx
            rax *= rcx;             //imul rax, rcx
            r8 -= rdi;              //sub r8, rdi
            r8 ^= rax;              //xor r8, rax
            rax = 0;                //and rax, 0xFFFFFFFFC0000000
            rax = _rotl64(rax, 0x10);               //rol rax, 0x10
            rax ^= r10;             //xor rax, r10
            rax = _byteswap_uint64(rax);            //bswap rax
            r8 *= *(uint64_t*)(rax + 0x17);                 //imul r8, [rax+0x17]
            rax = 0x770DE87D60F9538A;               //mov rax, 0x770DE87D60F9538A
            r8 += rax;              //add r8, rax
            rax = 0x6AFBA77C08ADD287;               //mov rax, 0x6AFBA77C08ADD287
            r8 *= rax;              //imul r8, rax
            return r8;
        }
        }
    }
    uint16_t get_bone_index(uint32_t bone_index)
    {
        auto imageBase = g_data::base;
        auto Peb = __readgsqword(0x60);
        uint64_t rax = imageBase, rbx = imageBase, rcx = imageBase, rdx = imageBase, rdi = imageBase, rsi = imageBase, r8 = imageBase, r9 = imageBase, r10 = imageBase, r11 = imageBase, r12 = imageBase, r13 = imageBase, r14 = imageBase, r15 = imageBase;
        rbx = bone_index;
        rcx = rbx * 0x13C8;
        rax = 0xC9D7E338B81A69C1;               //mov rax, 0xC9D7E338B81A69C1
        r11 = imageBase;           //lea r11, [0xFFFFFFFFFDBF497D]
        rax = _umul128(rax, rcx, (uintptr_t*)&rdx);             //mul rcx
        r10 = 0x78B61A0714EC5D43;               //mov r10, 0x78B61A0714EC5D43
        rdx >>= 0xC;            //shr rdx, 0x0C
        rax = rdx * 0x144B;             //imul rax, rdx, 0x144B
        rcx -= rax;             //sub rcx, rax
        rax = 0x2AD020C75918A037;               //mov rax, 0x2AD020C75918A037
        r8 = rcx * 0x144B;              //imul r8, rcx, 0x144B
        rax = _umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
        rdx >>= 0xA;            //shr rdx, 0x0A
        rax = rdx * 0x17EB;             //imul rax, rdx, 0x17EB
        r8 -= rax;              //sub r8, rax
        rax = 0x2E8BA2E8BA2E8BA3;               //mov rax, 0x2E8BA2E8BA2E8BA3
        rax = _umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
        rax = 0x624DD2F1A9FBE77;                //mov rax, 0x624DD2F1A9FBE77
        rdx >>= 0x4;            //shr rdx, 0x04
        rcx = rdx * 0x58;               //imul rcx, rdx, 0x58
        rax = _umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
        rax = r8;               //mov rax, r8
        rax -= rdx;             //sub rax, rdx
        rax >>= 0x1;            //shr rax, 0x01
        rax += rdx;             //add rax, rdx
        rax >>= 0x6;            //shr rax, 0x06
        rcx += rax;             //add rcx, rax
        rax = rcx * 0xFA;               //imul rax, rcx, 0xFA
        rcx = r8 * 0xFC;                //imul rcx, r8, 0xFC
        rcx -= rax;             //sub rcx, rax
        rax = *(uint16_t*)(rcx + r11 * 1 + 0x7426310);          //movzx eax, word ptr [rcx+r11*1+0x7426310]
        r8 = rax * 0x13C8;              //imul r8, rax, 0x13C8
        rax = r10;              //mov rax, r10
        rax = _umul128(rax, r8, (uintptr_t*)&rdx);              //mul r8
        rcx = r8;               //mov rcx, r8
        rax = r10;              //mov rax, r10
        rcx -= rdx;             //sub rcx, rdx
        rcx >>= 0x1;            //shr rcx, 0x01
        rcx += rdx;             //add rcx, rdx
        rcx >>= 0xC;            //shr rcx, 0x0C
        rcx = rcx * 0x15BF;             //imul rcx, rcx, 0x15BF
        r8 -= rcx;              //sub r8, rcx
        r9 = r8 * 0x2478;               //imul r9, r8, 0x2478
        rax = _umul128(rax, r9, (uintptr_t*)&rdx);              //mul r9
        rax = r9;               //mov rax, r9
        rax -= rdx;             //sub rax, rdx
        rax >>= 0x1;            //shr rax, 0x01
        rax += rdx;             //add rax, rdx
        rax >>= 0xC;            //shr rax, 0x0C
        rax = rax * 0x15BF;             //imul rax, rax, 0x15BF
        r9 -= rax;              //sub r9, rax
        rax = 0x47AE147AE147AE15;               //mov rax, 0x47AE147AE147AE15
        rax = _umul128(rax, r9, (uintptr_t*)&rdx);              //mul r9
        rax = r9;               //mov rax, r9
        rax -= rdx;             //sub rax, rdx
        rax >>= 0x1;            //shr rax, 0x01
        rax += rdx;             //add rax, rdx
        rax >>= 0x5;            //shr rax, 0x05
        rcx = rax * 0x32;               //imul rcx, rax, 0x32
        rax = 0x3AEF6CA970586723;               //mov rax, 0x3AEF6CA970586723
        rax = _umul128(rax, r9, (uintptr_t*)&rdx);              //mul r9
        rdx >>= 0x5;            //shr rdx, 0x05
        rcx += rdx;             //add rcx, rdx
        rax = rcx * 0x116;              //imul rax, rcx, 0x116
        rcx = r9 * 0x118;               //imul rcx, r9, 0x118
        rcx -= rax;             //sub rcx, rax
        r15 = *(uint16_t*)(rcx + r11 * 1 + 0x742B900);          //movsx r15d, word ptr [rcx+r11*1+0x742B900]
        return r15;
    }
}

namespace g_data
{
	uintptr_t base;
	uintptr_t peb;
	HWND hWind;
	uintptr_t visible_base;
	sdk::refdef_t* refdef;
	FARPROC Targetbitblt;
	FARPROC TargetStretchbitblt;

	void init()
	{
		base = (uintptr_t)(iat(GetModuleHandleA).get()("ModernWarfare.exe"));

		Targetbitblt = iat(GetProcAddress).get()(iat(GetModuleHandleA).get()("Gdi32.dll"), "BitBlt");

		TargetStretchbitblt = iat(GetProcAddress).get()(iat(GetModuleHandleA).get()("win32u.dll"), "NtGdiStretchBlt");

		hWind = process::get_process_window();

	}
}

namespace globals
{
	bool b_in_game = false;
	bool local_is_alive = false;
	bool is_aiming = false;
	bool b_spread = false;
	int max_player_count = -1;
	int clientSwitch = 0;
	int boneSwitch = 0;
	int keyBind = -1;
	uintptr_t local_ptr;
	uintptr_t client_info;
	uintptr_t client_base;
	uintptr_t refdef_ptr;
	vec3_t local_pos;

	void popUpAddress(std::string name, uintptr_t address)
	{
		std::stringstream ss;
		ss << name << " :0x" << std::hex << address;
		std::string buffer(ss.str());
		MessageBox(NULL, buffer.c_str(), name.c_str(), MB_OK);
	}

	void popUpMessage(std::string title, std::string message)
	{
		MessageBox(NULL, message.c_str(), title.c_str(), MB_OK);
	}
}

namespace screenshot
{
	bool		visuals = true;
	bool* pDrawEnabled = nullptr;
	uint32_t	screenshot_counter = 0;
	uint32_t	bit_blt_log = 0;
	const char* bit_blt_fail;
	uintptr_t	bit_blt_anotherlog;
	uint32_t	GdiStretchBlt_log = 0;
	const char* GdiStretchBlt_fail;
	uintptr_t	GdiStretchBlt_anotherlog;
	uintptr_t	texture_copy_log = 0;
	uintptr_t	virtualqueryaddr = 0;
}

namespace g_draw
{
	void draw_line(const ImVec2& from, const ImVec2& to, C_Color color, float thickness)
	{
		auto window = ImGui::GetBackgroundDrawList();;
		window->AddLine(from, to, color.GetU32(), thickness);
	}

	void draw_box(const float x, const float y, const float width, const float height, const C_Color color, float thickness)
	{
		draw_line(ImVec2(x, y), ImVec2(x + width, y), color, thickness);
		draw_line(ImVec2(x, y), ImVec2(x, y + height), color, thickness);
		draw_line(ImVec2(x, y + height), ImVec2(x + width, y + height), color, thickness);
		draw_line(ImVec2(x + width, y), ImVec2(x + width, y + height), color, thickness);
	}

	void draw_corned_box(const vec2_t& rect, const vec2_t& size, C_Color color, float thickness)
	{
		size.x - 5;
		const float lineW = (size.x / 5);
		const float lineH = (size.y / 6);
		const float lineT = 1;

		//outline
		draw_line(ImVec2(rect.x - lineT, rect.y - lineT), ImVec2(rect.x + lineW, rect.y - lineT), color, thickness); //top left
		draw_line(ImVec2(rect.x - lineT, rect.y - lineT), ImVec2(rect.x - lineT, rect.y + lineH), color, thickness);
		draw_line(ImVec2(rect.x - lineT, rect.y + size.y - lineH), ImVec2(rect.x - lineT, rect.y + size.y + lineT), color, thickness); //bot left
		draw_line(ImVec2(rect.x - lineT, rect.y + size.y + lineT), ImVec2(rect.x + lineW, rect.y + size.y + lineT), color, thickness);
		draw_line(ImVec2(rect.x + size.x - lineW, rect.y - lineT), ImVec2(rect.x + size.x + lineT, rect.y - lineT), color, thickness); // top right
		draw_line(ImVec2(rect.x + size.x + lineT, rect.y - lineT), ImVec2(rect.x + size.x + lineT, rect.y + lineH), color, thickness);
		draw_line(ImVec2(rect.x + size.x + lineT, rect.y + size.y - lineH), ImVec2(rect.x + size.x + lineT, rect.y + size.y + lineT), color, thickness); // bot right
		draw_line(ImVec2(rect.x + size.x - lineW, rect.y + size.y + lineT), ImVec2(rect.x + size.x + lineT, rect.y + size.y + lineT), color, thickness);

		//inline
		draw_line(ImVec2(rect.x, rect.y), ImVec2(rect.x, rect.y + lineH), color, thickness);//top left
		draw_line(ImVec2(rect.x, rect.y), ImVec2(rect.x + lineW, rect.y), color, thickness);
		draw_line(ImVec2(rect.x + size.x - lineW, rect.y), ImVec2(rect.x + size.x, rect.y), color, thickness); //top right
		draw_line(ImVec2(rect.x + size.x, rect.y), ImVec2(rect.x + size.x, rect.y + lineH), color, thickness);
		draw_line(ImVec2(rect.x, rect.y + size.y - lineH), ImVec2(rect.x, rect.y + size.y), color, thickness); //bot left
		draw_line(ImVec2(rect.x, rect.y + size.y), ImVec2(rect.x + lineW, rect.y + size.y), color, thickness);
		draw_line(ImVec2(rect.x + size.x - lineW, rect.y + size.y), ImVec2(rect.x + size.x, rect.y + size.y), color, thickness);//bot right
		draw_line(ImVec2(rect.x + size.x, rect.y + size.y - lineH), ImVec2(rect.x + size.x, rect.y + size.y), color, thickness);

	}

	void fill_rectangle(const float x, const float y, const float width, const float hight, const C_Color color)
	{
		const float end_y = y + hight;
		for (float curr_y = y; curr_y < end_y; ++curr_y)
		{
			draw_line(ImVec2(x, curr_y), ImVec2(x + width, curr_y), color, 1.5f);
		}
	}

	void draw_circle(const ImVec2& position, float radius, C_Color color, float thickness)
	{
		float step = (float)M_PI * 2.0f / thickness;
		for (float a = 0; a < (M_PI * 2.0f); a += step)
		{
			draw_line(
				ImVec2(radius * cosf(a) + position.x, radius * sinf(a) + position.y),
				ImVec2(radius * cosf(a + step) + position.x, radius * sinf(a + step) + position.y),
				color,
				1.5f
			);
		}
	}

	void draw_sketch_edge_text(ImFont* pFont, const std::string& text, const ImVec2& pos, float size, C_Color color, bool center, uint32_t EdgeColor)
	{
		constexpr float fStrokeVal1 = 1.0f;
		ImGuiWindow* window = ImGui::GetCurrentWindow();

		float Edge_a = (EdgeColor >> 24) & 0xff;
		float Edge_r = (EdgeColor >> 16) & 0xff;
		float Edge_g = (EdgeColor >> 8) & 0xff;
		float Edge_b = (EdgeColor) & 0xff;
		std::stringstream steam(text);
		std::string line;
		float y = 0.0f;
		int i = 0;
		while (std::getline(steam, line))
		{
			ImVec2 textSize = pFont->CalcTextSizeA(size, FLT_MAX, 0.0f, line.c_str());
			if (center)
			{
				window->DrawList->AddText(pFont, size, ImVec2((pos.x - textSize.x / 2.0f) - fStrokeVal1, pos.y + textSize.y * i), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2((pos.x - textSize.x / 2.0f) + fStrokeVal1, pos.y + textSize.y * i), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2((pos.x - textSize.x / 2.0f), (pos.y + textSize.y * i) - fStrokeVal1), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2((pos.x - textSize.x / 2.0f), (pos.y + textSize.y * i) + fStrokeVal1), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2(pos.x - textSize.x / 2.0f, pos.y + textSize.y * i), color.GetU32(), line.c_str());
			}
			else
			{
				window->DrawList->AddText(pFont, size, ImVec2((pos.x) - fStrokeVal1, (pos.y + textSize.y * i)), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2((pos.x) + fStrokeVal1, (pos.y + textSize.y * i)), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2((pos.x), (pos.y + textSize.y * i) - fStrokeVal1), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2((pos.x), (pos.y + textSize.y * i) + fStrokeVal1), ImGui::GetColorU32(ImVec4(Edge_r / 255, Edge_g / 255, Edge_b / 255, Edge_a / 255)), line.c_str());
				window->DrawList->AddText(pFont, size, ImVec2(pos.x, pos.y + textSize.y * i), color.GetU32(), line.c_str());
			}
			y = pos.y + textSize.y * (i + 1);
			i++;
		}
	}

	void draw_crosshair()
	{
		constexpr long crosshair_size = 15.0f;

		ImVec2 center = ImVec2(g_data::refdef->Width / 2, g_data::refdef->Height / 2);

		g_draw::draw_line(ImVec2((center.x), (center.y) - crosshair_size), ImVec2((center.x), (center.y) + crosshair_size), Settings::colors::White.color(), 1.5f);
		g_draw::draw_line(ImVec2((center.x) - crosshair_size, (center.y)), ImVec2((center.x) + crosshair_size, (center.y)), Settings::colors::White.color(), 1.5f);
	}

	void draw_fov(const float aimbot_fov)
	{
		ImVec2 center = ImVec2(g_data::refdef->Width / 2, g_data::refdef->Height / 2);

		g_draw::draw_circle(center, aimbot_fov, Settings::colors::White.color(), 100.0f);
	}

	void draw_bones(vec2_t* bones_screenPos, long count, C_Color color)
	{
		long last_count = count - 1;
		for (long i = 0; i < last_count; ++i)
			g_draw::draw_line(ImVec2(bones_screenPos[i].x, bones_screenPos[i].y), ImVec2(bones_screenPos[i + 1].x, bones_screenPos[i + 1].y), color, 1.8f);
	}

	void draw_bones(unsigned long i, vec3_t origem, C_Color color)
	{
		vec3_t header_to_bladder[6], right_foot_to_bladder[5], left_foot_to_bladder[5], right_hand[5], left_hand[5];
		vec2_t screen_header_to_bladder[6], screen_right_foot_to_bladder[5], screen_left_foot_to_bladder[5], screen_right_hand[5], screen_left_hand[5];

		if (sdk::get_bone_by_player_index(i, sdk::BONE_POS_HEAD, &header_to_bladder[0]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_NECK, &header_to_bladder[1]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_CHEST, &header_to_bladder[2]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_MID, &header_to_bladder[3]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_TUMMY, &header_to_bladder[4]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_PELVIS, &header_to_bladder[5]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_FOOT_1, &right_foot_to_bladder[1]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_FOOT_2, &right_foot_to_bladder[2]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_FOOT_3, &right_foot_to_bladder[3]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_FOOT_4, &right_foot_to_bladder[4]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_FOOT_1, &left_foot_to_bladder[1]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_FOOT_2, &left_foot_to_bladder[2]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_FOOT_3, &left_foot_to_bladder[3]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_FOOT_4, &left_foot_to_bladder[4]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_HAND_1, &right_hand[1]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_HAND_2, &right_hand[2]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_HAND_3, &right_hand[3]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_LEFT_HAND_4, &right_hand[4]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_HAND_1, &left_hand[1]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_HAND_2, &left_hand[2]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_HAND_3, &left_hand[3]) &&
			sdk::get_bone_by_player_index(i, sdk::BONE_POS_RIGHT_HAND_4, &left_hand[4]))
		{
			right_foot_to_bladder[0] = header_to_bladder[5];
			left_foot_to_bladder[0] = header_to_bladder[5];
			right_hand[0] = header_to_bladder[3];
			left_hand[0] = header_to_bladder[3];

			if (!sdk::is_valid_bone(origem, header_to_bladder, 6) ||
				!sdk::is_valid_bone(origem, right_foot_to_bladder, 5) ||
				!sdk::is_valid_bone(origem, left_foot_to_bladder, 5) ||
				!sdk::is_valid_bone(origem, right_hand, 5) ||
				!sdk::is_valid_bone(origem, left_hand, 5))
			{
				return;
			}

			if (!sdk::bones_to_screen(header_to_bladder, screen_header_to_bladder, 6))
				return;

			if (!sdk::bones_to_screen(right_foot_to_bladder, screen_right_foot_to_bladder, 5))
				return;

			if (!sdk::bones_to_screen(left_foot_to_bladder, screen_left_foot_to_bladder, 5))
				return;

			if (!sdk::bones_to_screen(right_hand, screen_right_hand, 5))
				return;

			if (!sdk::bones_to_screen(left_hand, screen_left_hand, 5))
				return;

			draw_bones(screen_header_to_bladder, 6, color);
			draw_bones(screen_right_foot_to_bladder, 5, color);
			draw_bones(screen_left_foot_to_bladder, 5, color);
			draw_bones(screen_right_hand, 5, color);
			draw_bones(screen_left_hand, 5, color);
		}
	}

	void draw_health(int i_health, vec3_t pos)
	{
		vec2_t bottom, top;

		if (!sdk::WorldToScreen(pos, &bottom))
			return;

		pos.z += 60;
		if (!sdk::WorldToScreen(pos, &top))
			return;

		top.y -= 5;
		auto height = top.y - bottom.y;
		auto width = height / 2.f;
		auto x = top.x - width / 1.8f;
		auto y = top.y;

		auto bar_width = max(0, min(127, i_health)) * (bottom.y - top.y) / 127.f;
		auto health = max(0, min(127, i_health));
		C_Color color_health(0x03FF00FF);
		C_Color color_health2(0xFF0F00FF);
		fill_rectangle(x, y, 4, 127 * (bottom.y - top.y) / 127.f, color_health2);
		fill_rectangle(x + 1, y + 1, 2, bar_width - 2, color_health);

	}
}

namespace g_radar {

	float RadarPosX = 60;
	float RadarPosY = 45;
	long  RadarSize = 220;
	long  RadarRadius = RadarSize / 2;
	float RadarLineInterval = 1.0f;
	float BackgroundInterval = 5.0f;

	void show_radar_background()
	{
		/*RadarPosX = g_data::refdef->Width - RadarSize - 50;*/

		g_draw::fill_rectangle(
			RadarPosX - BackgroundInterval,
			RadarPosY - BackgroundInterval,
			RadarSize + (BackgroundInterval * 2),
			RadarSize + (BackgroundInterval * 2),
			/*0xFFFFFEFE*/
			Settings::colors::radar_boarder.color());

		g_draw::fill_rectangle(
			RadarPosX,
			RadarPosY,
			RadarSize,
			RadarSize,
			/*0xFFA8AAAA*/
			Settings::colors::radar_bg.color());

		g_draw::draw_line(
			ImVec2(RadarPosX, RadarPosY + RadarLineInterval),
			ImVec2(RadarPosX, RadarPosY + RadarSize - RadarLineInterval + 1),
			Settings::colors::radar_boarder.color(),
			1.3f);

		g_draw::draw_line(
			ImVec2(RadarPosX + RadarSize, RadarPosY + RadarLineInterval),
			ImVec2(RadarPosX + RadarSize, RadarPosY + RadarSize - RadarLineInterval + 1),
			Settings::colors::radar_boarder.color(),
			1.3f);

		g_draw::draw_line(
			ImVec2(RadarPosX, RadarPosY),
			ImVec2(RadarPosX + RadarSize - RadarLineInterval + 2, RadarPosY),
			Settings::colors::radar_boarder.color(),
			1.3f);

		g_draw::draw_line(
			ImVec2(RadarPosX, RadarPosY + RadarSize),
			ImVec2(RadarPosX + RadarSize - RadarLineInterval + 2, RadarPosY + RadarSize),
			Settings::colors::radar_boarder.color(),
			1.3f);


		g_draw::draw_line(
			ImVec2(RadarPosX + RadarRadius, RadarPosY + RadarLineInterval),
			ImVec2(RadarPosX + RadarRadius, RadarPosY + RadarSize - RadarLineInterval),
			Settings::colors::radar_boarder.color(),
			1.3f);

		g_draw::draw_line(
			ImVec2(RadarPosX, RadarPosY + RadarRadius),
			ImVec2(RadarPosX + RadarSize - RadarLineInterval, RadarPosY + RadarRadius),
			Settings::colors::radar_boarder.color(),
			1.3f);


		g_draw::draw_box(RadarPosX + RadarRadius - 3, RadarPosY + RadarRadius - 3, 5, 5, Settings::colors::radar_boarder.color(), 1.0f);
		g_draw::fill_rectangle(RadarPosX + RadarRadius - 2, RadarPosY + RadarRadius - 2, 4, 4, Settings::colors::radar_boarder.color());
	}

	ImVec2 rotate(const ImVec2& center, const ImVec2& pos, float angle)
	{
		ImVec2 Return;
		angle *= -(M_PI / 180.0f);
		float cos_theta = cos(angle);
		float sin_theta = sin(angle);
		Return.x = (cos_theta * (pos[0] - center[0]) - sin_theta * (pos[1] - center[1])) + center[0];
		Return.y = (sin_theta * (pos[0] - center[0]) + cos_theta * (pos[1] - center[1])) + center[1];
		return Return;
	}

	vec2_t radar_rotate(const float x, const float y, float angle)
	{
		angle = (float)(angle * (M_PI / 180.f));
		float cosTheta = (float)cos(angle);
		float sinTheta = (float)sin(angle);
		vec2_t returnVec;
		returnVec.x = cosTheta * x + sinTheta * y;
		returnVec.y = sinTheta * x - cosTheta * y;
		return returnVec;
	}

	void draw_entity(const ImVec2& pos, float angle, C_Color color)
	{
		constexpr long up_offset = 7;
		constexpr long lr_offset = 5;

		for (int FillIndex = 0; FillIndex < 5; ++FillIndex)
		{
			ImVec2 up_pos(pos.x, pos.y - up_offset + FillIndex);
			ImVec2 left_pos(pos.x - lr_offset + FillIndex, pos.y + up_offset - FillIndex);
			ImVec2 right_pos(pos.x + lr_offset - FillIndex, pos.y + up_offset - FillIndex);

			ImVec2 p0 = rotate(pos, up_pos, angle);
			ImVec2 p1 = rotate(pos, left_pos, angle);
			ImVec2 p2 = rotate(pos, right_pos, angle);

			g_draw::draw_line(p0, p1, color, 1.0f);
			g_draw::draw_line(p1, p2, color, 1.0f);
			g_draw::draw_line(p2, p0, color, 1.0f);
		}
	}

	void draw_death_entity(const ImVec2& pos, C_Color color)
	{
		constexpr float line_radius = 5;
		ImVec2 p0(pos.x - line_radius, pos.y - line_radius);
		ImVec2 p1(pos.x + line_radius, pos.y + line_radius);
		ImVec2 p3(pos.x - line_radius, pos.y + line_radius);
		ImVec2 p4(pos.x + line_radius, pos.y - line_radius);
		g_draw::draw_line(p0, p1, color, 1.5f);
		g_draw::draw_line(p3, p4, color, 1.5f);
	}

	void draw_entity(sdk::player_t local_entity, sdk::player_t entity, bool IsFriendly, bool died, C_Color color)
	{
		const float local_rotation = local_entity.get_rotation();
		float rotation = entity.get_rotation();

		rotation = rotation - local_rotation;

		if (rotation < 0)
			rotation = 360.0f - std::fabs(rotation);

		float x_distance = local_entity.get_pos().x - entity.get_pos().x;
		float y_distance = local_entity.get_pos().y - entity.get_pos().y;

		float zoom = Settings::esp::radar_zoom * 0.001f;

		x_distance *= zoom;
		y_distance *= zoom;

		vec2_t point_offset = radar_rotate(x_distance, y_distance, local_rotation);

		long positiveRadarRadius = RadarRadius - 5;
		long negaRadarRadius = (RadarRadius * -1) + 5;

		if (point_offset.x > positiveRadarRadius)
		{
			point_offset.x = positiveRadarRadius;
		}
		else if (point_offset.x < negaRadarRadius)
		{
			point_offset.x = negaRadarRadius;
		}

		if (point_offset.y > positiveRadarRadius)
		{
			point_offset.y = positiveRadarRadius;
		}
		else if (point_offset.y < negaRadarRadius)
		{
			point_offset.y = negaRadarRadius;
		}

		if (!died)
		{
			draw_entity(ImVec2(RadarPosX + RadarRadius + point_offset.x, RadarPosY + RadarRadius + point_offset.y), rotation, color);
		}
		else
		{
			draw_death_entity(ImVec2(RadarPosX + RadarRadius + point_offset.x, RadarPosY + RadarRadius + point_offset.y), color);
		}
	}
}