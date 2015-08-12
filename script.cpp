#include "script.h"
#include "keyboard.h"
#include "utils.h"

#include <string>
#include <ctime>
#include <vector>


Ped pedShortcuts[10] = {};


std::string statusText;
DWORD statusTextDrawTicksMax;
bool statusTextGxtEntry;

void update_status_text()
{
	if (GetTickCount() < statusTextDrawTicksMax)
	{
		UI::SET_TEXT_FONT(0);
		UI::SET_TEXT_SCALE(0.55, 0.55);
		UI::SET_TEXT_COLOUR(255, 255, 255, 255);
		UI::SET_TEXT_WRAP(0.0, 1.0);
		UI::SET_TEXT_CENTRE(1);
		UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
		UI::SET_TEXT_EDGE(1, 0, 0, 0, 205);
		if (statusTextGxtEntry)
		{
			UI::_SET_TEXT_ENTRY((char *)statusText.c_str());
		}
		else
		{
			UI::_SET_TEXT_ENTRY("STRING");
			UI::_ADD_TEXT_COMPONENT_STRING((char *)statusText.c_str());
		}
		UI::_DRAW_TEXT(0.5, 0.5);
	}
}

void set_status_text(std::string str, DWORD time = 2500, bool isGxtEntry = false)
{
	statusText = str;
	statusTextDrawTicksMax = GetTickCount() + time;
	statusTextGxtEntry = isGxtEntry;
}

static LPCSTR weaponNames[] = {
	"WEAPON_KNIFE", "WEAPON_NIGHTSTICK", "WEAPON_HAMMER", "WEAPON_BAT", "WEAPON_GOLFCLUB", "WEAPON_CROWBAR",
	"WEAPON_PISTOL", "WEAPON_COMBATPISTOL", "WEAPON_APPISTOL", "WEAPON_PISTOL50", "WEAPON_MICROSMG", "WEAPON_SMG",
	"WEAPON_ASSAULTSMG", "WEAPON_ASSAULTRIFLE", "WEAPON_CARBINERIFLE", "WEAPON_ADVANCEDRIFLE", "WEAPON_MG",
	"WEAPON_COMBATMG", "WEAPON_PUMPSHOTGUN", "WEAPON_SAWNOFFSHOTGUN", "WEAPON_ASSAULTSHOTGUN", "WEAPON_BULLPUPSHOTGUN",
	"WEAPON_STUNGUN", "WEAPON_SNIPERRIFLE", "WEAPON_HEAVYSNIPER", "WEAPON_GRENADELAUNCHER", "WEAPON_GRENADELAUNCHER_SMOKE",
	"WEAPON_RPG", "WEAPON_MINIGUN", "WEAPON_GRENADE", "WEAPON_STICKYBOMB", "WEAPON_SMOKEGRENADE", "WEAPON_BZGAS",
	"WEAPON_MOLOTOV", "WEAPON_FIREEXTINGUISHER", "WEAPON_PETROLCAN",
	"WEAPON_SNSPISTOL", "WEAPON_SPECIALCARBINE", "WEAPON_HEAVYPISTOL", "WEAPON_BULLPUPRIFLE", "WEAPON_HOMINGLAUNCHER",
	"WEAPON_PROXMINE", "WEAPON_SNOWBALL", "WEAPON_VINTAGEPISTOL", "WEAPON_DAGGER", "WEAPON_FIREWORK", "WEAPON_MUSKET",
	"WEAPON_MARKSMANRIFLE", "WEAPON_HEAVYSHOTGUN", "WEAPON_GUSENBERG", "WEAPON_HATCHET", "WEAPON_RAILGUN",
	"WEAPON_COMBATPDW", "WEAPON_KNUCKLE", "WEAPON_MARKSMANPISTOL"
};

void give_all_weapons(Player playerPed) {
	for (int i = 0; i < sizeof(weaponNames) / sizeof(weaponNames[0]); i++)
		WEAPON::GIVE_DELAYED_WEAPON_TO_PED(playerPed, GAMEPLAY::GET_HASH_KEY((char *)weaponNames[i]), 1000, 0);
	//set_status_text("all weapons added");
}

void give_basic_weapon(Player playerPed) {
	WEAPON::GIVE_DELAYED_WEAPON_TO_PED(playerPed, GAMEPLAY::GET_HASH_KEY((char *)weaponNames[7]), 1000, 0);
}



// player model control, switching on normal ped model when needed	
void check_player_model()
{
	// common variables
	Player player = PLAYER::PLAYER_ID();
	Ped playerPed = PLAYER::PLAYER_PED_ID();

	if (!ENTITY::DOES_ENTITY_EXIST(playerPed)) return;

	Hash model = ENTITY::GET_ENTITY_MODEL(playerPed);
	if (ENTITY::IS_ENTITY_DEAD(playerPed) || PLAYER::IS_PLAYER_BEING_ARRESTED(player, TRUE))
		if (model != GAMEPLAY::GET_HASH_KEY("player_zero") &&
			model != GAMEPLAY::GET_HASH_KEY("player_one") &&
			model != GAMEPLAY::GET_HASH_KEY("player_two"))
		{
			set_status_text("turning to normal");
			WAIT(1000);

			model = GAMEPLAY::GET_HASH_KEY("player_zero");
			STREAMING::REQUEST_MODEL(model);
			while (!STREAMING::HAS_MODEL_LOADED(model))
				WAIT(0);
			PLAYER::SET_PLAYER_MODEL(PLAYER::PLAYER_ID(), model);
			PED::SET_PED_DEFAULT_COMPONENT_VARIATION(PLAYER::PLAYER_PED_ID());
			STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);

			// wait until player is ressurected
			while (ENTITY::IS_ENTITY_DEAD(PLAYER::PLAYER_PED_ID()) || PLAYER::IS_PLAYER_BEING_ARRESTED(player, TRUE))
				WAIT(0);

		}
}

void drive_to_waypoint(Ped pedInVehicle) {
	//code inspired by LUA plugin https://www.gta5-mods.com/scripts/realistic-vehicle-controls
	if (PED::IS_PED_IN_ANY_VEHICLE(pedInVehicle, 0) && UI::IS_WAYPOINT_ACTIVE()) {
		int waypointID = UI::GET_FIRST_BLIP_INFO_ID(UI::_GET_BLIP_INFO_ID_ITERATOR());
		Vector3 waypointCoord = UI::GET_BLIP_COORDS(waypointID);

		Vehicle playerVehicle = PED::GET_VEHICLE_PED_IS_USING(pedInVehicle);

		AI::TASK_VEHICLE_DRIVE_TO_COORD(pedInVehicle, playerVehicle, waypointCoord.x, waypointCoord.y, waypointCoord.z, 30, 1, ENTITY::GET_ENTITY_MODEL(playerVehicle), 1, 0xC00AB, -1);
		set_status_text("Driving to waypoint");
	}
	else {
		set_status_text("No waypoint exist when you swapped away from driver");
	}

}

void possess_ped(Ped swapToPed) {
	if (ENTITY::DOES_ENTITY_EXIST(swapToPed) && ENTITY::IS_ENTITY_A_PED(swapToPed)) {
		Ped swapFromPed = PLAYER::PLAYER_PED_ID();

		if (PED::IS_PED_IN_ANY_VEHICLE(swapFromPed, 0)) {
			drive_to_waypoint(swapFromPed);
		}

		PLAYER::CHANGE_PLAYER_PED(PLAYER::PLAYER_ID(), swapToPed, false, false);
		
		//pause any AI tasks such as driving to waypoint
		AI::TASK_PAUSE(swapToPed, 1);
		
		pedShortcuts[0] = swapFromPed;



		//set_status_text("Possessing ped. Swap back with ALT+0");
		WAIT(500);
	}
	else {
		set_status_text("Could not possess ped");
		WAIT(500);
	}

}




void action_possess_ped() {
	Player player = PLAYER::PLAYER_ID();

	//If player is aiming at a ped, control that
	if (PLAYER::IS_PLAYER_FREE_AIMING(player)) {
		Entity targetEntity;
		PLAYER::_GET_AIMED_ENTITY(player, &targetEntity);

		if (ENTITY::DOES_ENTITY_EXIST(targetEntity) && ENTITY::IS_ENTITY_A_PED(targetEntity)) {
			set_status_text("Possessing targeted pedestrian");

			possess_ped(targetEntity);
			//give a pistol, so they can easily target the next person
			give_basic_weapon(targetEntity);
			
		}
		else {
			set_status_text("Aim on a pedestrian and try again");
		}
	}
	else {//if not, take control of the closest one
		//inspired by https://github.com/Reck1501/GTAV-EnhancedNativeTrainer/blob/aea568cd1a7134c0a9adf3e9dc1fd7f4640d0a1c/EnhancedNativeTrainer/script.cpp#L1104
		const int numElements = 1;
		const int arrSize = numElements * 2 + 2;

		Ped *peds = new Ped[arrSize];
		peds[0] = numElements;
		int nrPeds = PED::GET_PED_NEARBY_PEDS(PLAYER::PLAYER_PED_ID(), peds, -1);
		for (int i = 0; i < nrPeds; i++)
		{
			int offsettedID = i * 2 + 2;

			if (!ENTITY::DOES_ENTITY_EXIST(peds[offsettedID]))
			{
				continue;
			}

			Ped closestPed = peds[offsettedID];
			set_status_text("Possessing closest pedestrian");

			possess_ped(closestPed);

		}
		delete peds;
	}
}




void action_clone_myself() {
	Ped playerPed = PLAYER::PLAYER_PED_ID();

	Ped clonedPed = PED::CLONE_PED(playerPed,0.0f, false, true);
	pedShortcuts[0] = clonedPed;

	if (PED::IS_PED_IN_ANY_VEHICLE(playerPed, 0))
	{
		Vehicle vehicle = PED::GET_VEHICLE_PED_IS_USING(playerPed);
		PED::SET_PED_INTO_VEHICLE(clonedPed, vehicle, -2);
	}

	set_status_text("Cloned myself. Possess clone with ALT+0");
}



bool possess_key_pressed()
{
	return IsKeyJustUp(VK_F9);
}

bool clone_key_pressed()
{
	return IsKeyJustUp(VK_F10);
}

bool swap_to_previous_possessed_key_pressed()
{
	//see https://msdn.microsoft.com/de-de/library/windows/desktop/dd375731(v=vs.85).aspx for key codes	
	return IsKeyDown(VK_CONTROL) && IsKeyDown(0x30);
}

void action_if_ped_assign_shortcut_key_pressed()
{	
	//ALT key
	if (IsKeyDown(VK_CONTROL)) {
		int pedShortcutsIndex = -1;
		//Valid keys - 31=1 39=9
		if (IsKeyDown(0x31)) {
			//0 index is reserved for previously possessed or cloned
			pedShortcutsIndex = 1;
		}
		else if (IsKeyDown(0x32)) {
			pedShortcutsIndex = 2;
		}
		else if (IsKeyDown(0x33)) {
			pedShortcutsIndex = 3;
		}
		else if (IsKeyDown(0x34)) {
			pedShortcutsIndex = 4;
		}
		else if (IsKeyDown(0x35)) {
			pedShortcutsIndex = 5;
		}
		else if (IsKeyDown(0x36)) {
			pedShortcutsIndex = 6;
		}
		else if (IsKeyDown(0x37)) {
			pedShortcutsIndex = 7;
		}
		else if (IsKeyDown(0x38)) {
			pedShortcutsIndex = 8;
		}
		else if (IsKeyDown(0x39)) {
			pedShortcutsIndex = 9;
		}

		if (pedShortcutsIndex != -1) {
			Ped playerPed = PLAYER::PLAYER_PED_ID();
			pedShortcuts[pedShortcutsIndex] = playerPed;
			set_status_text("Stored current ped. Retrieve with ALT+" + std::to_string(pedShortcutsIndex));
		}
	}
}

void action_if_ped_execute_shortcut_key_pressed()
{
	//ALT key
	if (IsKeyDown(VK_MENU)) {
		int pedShortcutsIndex = -1;
		//Valid keys - 31=1 39=9
		if (IsKeyDown(0x30)) {
			//0 index cannot be assigned, but is previously possessed or cloned
			pedShortcutsIndex = 0;
		}else if (IsKeyDown(0x31)) {
			pedShortcutsIndex = 1;
		}else if (IsKeyDown(0x31)) {
			pedShortcutsIndex = 1;
		}
		else if (IsKeyDown(0x32)) {
			pedShortcutsIndex = 2;
		}
		else if (IsKeyDown(0x33)) {
			pedShortcutsIndex = 3;
		}
		else if (IsKeyDown(0x34)) {
			pedShortcutsIndex = 4;
		}
		else if (IsKeyDown(0x35)) {
			pedShortcutsIndex = 5;
		}
		else if (IsKeyDown(0x36)) {
			pedShortcutsIndex = 6;
		}
		else if (IsKeyDown(0x37)) {
			pedShortcutsIndex = 7;
		}
		else if (IsKeyDown(0x38)) {
			pedShortcutsIndex = 8;
		}
		else if (IsKeyDown(0x39)) {
			pedShortcutsIndex = 9;
		}

		if (pedShortcutsIndex != -1) {
			//TODO: Check if it exist

			if (pedShortcuts[pedShortcutsIndex] == 0) {
				set_status_text("No stored ped. Store with CTRL+" + std::to_string(pedShortcutsIndex));
			}
			else {
				possess_ped(pedShortcuts[pedShortcutsIndex]);
			}
		}
	}
}








void main()
{
	while (true)
	{
		update_status_text();

		if (possess_key_pressed()) {
			set_status_text("Possessing nearest pedestrian");
			action_possess_ped();
		}

		if (clone_key_pressed()) {
			action_clone_myself();
		}

		action_if_ped_assign_shortcut_key_pressed();

		action_if_ped_execute_shortcut_key_pressed();

		//check if the player is dead/arrested, in order to swap back to original
		check_player_model();

		WAIT(0);
	}
}

void ScriptMain()
{
	main();
}