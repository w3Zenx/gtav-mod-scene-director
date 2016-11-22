#pragma once

#include "BirdsEyeMode.h"
#include "keyboard.h"
#include "utils.h"
#include <vector>
#include <string>




BirdsEyeMode::BirdsEyeMode()
{
	shouldExitMode = false;
	shouldDrawMenu = true;
	shouldDrawRecordingMarkers = true;
	cameraSpeedFactor = 0.1;
	camLastPos = {};
}
/**
* Main loop which is called for each tick from script.cpp
* Returns false if BirdsEyeMode is finished
*/
bool BirdsEyeMode::actionOnTick(DWORD tick, std::vector<Actor> & actors)
{
	disableControls();

	/* ACTIONS WHICH MAY REQUIRE A WAIT PERIODE iN TICKS AFTERWAREDS */
	if (nextWaitTicks == 0 || GetTickCount() - mainTickLast >= nextWaitTicks) {
		nextWaitTicks = 0;
		shouldExitMode = false;
		if (shouldDrawMenu) {
			if (menu_up_key_pressed()) {
				if (submenu_active_index != -1) {
					submenu_active_index++;
					nextWaitTicks = 200;
				}
				else {
					menu_active_index++;
					nextWaitTicks = 200;
				}
			}
			else if (menu_down_key_pressed()) {
				if (submenu_active_index != -1) {
					submenu_active_index--;
					nextWaitTicks = 200;
				}
				else {
					menu_active_index--;
					nextWaitTicks = 200;
				}
			}
			else if (menu_select_key_pressed()) {
				if (submenu_active_index != -1) {
					actionSubMenuEditSelected();
				}
				else {
					actionMenuSelected();
				}

				nextWaitTicks = 200;
			}
			else if (menu_left_key_pressed()) {
				submenu_active_index = 0;
				nextWaitTicks = 200;
			}
			else if (menu_right_key_pressed()) {
				submenu_active_index = -1;
				//cancel edit location and highlighted
				selectedRecording = nullptr;
				selectedActor = nullptr;
				highlightedRecording = nullptr;
				highlightedActor = nullptr;
				nextWaitTicks = 200;
			}
		}

		checkInputAction();

		mainTickLast = GetTickCount();

	}

	if (checkInputMovement()) {
		//we have camera movement. Updated selected recording accordingly
		if (selectedRecording != nullptr) {
			log_to_file("#Cam pos vs last  (" + std::to_string(camNewPos.x) + ", " + std::to_string(camNewPos.y) + ", " + std::to_string(camNewPos.z) + ")(" + std::to_string(camLastPos.x) + ", " + std::to_string(camLastPos.y) + ", " + std::to_string(camLastPos.z) + ")");

			Vector3 deltaPos = {};
			deltaPos.x = camNewPos.x - camLastPos.x;
			deltaPos.y = camNewPos.y - camLastPos.y;
			deltaPos.z = 0.0;
			//only include if mouse buttons used to change z-elevation
			if (CONTROLS::IS_DISABLED_CONTROL_PRESSED(2, 329) || CONTROLS::IS_DISABLED_CONTROL_PRESSED(2, 330)) {
				deltaPos.z = camNewPos.z - camLastPos.z;
			}

			//log_to_file("#Delta location (" + std::to_string(deltaPos.x) + ", " + std::to_string(deltaPos.y) + ", " + std::to_string(deltaPos.z) + ")");

			Vector3 recordingLocation = selectedRecording->getLocation();
			recordingLocation.x += deltaPos.x;
			recordingLocation.y += deltaPos.y;
			recordingLocation.z += deltaPos.z;

			selectedRecording->setLocation(recordingLocation);
			//log_to_file("#New location of recording (" + std::to_string(recordingLocation.x) + ", " + std::to_string(recordingLocation.y) + ", " + std::to_string(recordingLocation.z) + ")");
		}

	}
	checkInputRotation();

	if (shouldDrawMenu) {
		drawMenu();
		drawSubMenuEdit();
		drawInstructions();
	}


	//actions to be used during active scene
	draw_spot_lights();

	//check if the player is dead/arrested, in order to swap back to original in order to avoid crash
	check_player_model();
	 
	//Highlighting of the 3 points we have for highlighted recording items
	std::shared_ptr<ActorRecordingItem> nearestRecording = getNearestRecording(actors);
	if (selectedRecording != nullptr) {
		nearestRecording->setMarkerType(MARKER_TYPE_NORMAL);
		if (highlightedRecording != nullptr) {
			highlightedRecording->setMarkerType(MARKER_TYPE_NORMAL);
		}
		selectedRecording->setMarkerType(MARKER_TYPE_SELECTED);
		submenu_is_active = true;
	}
	else if (highlightedRecording != nullptr) {
		nearestRecording->setMarkerType(MARKER_TYPE_NORMAL);
		highlightedRecording->setMarkerType(MARKER_TYPE_HIGHLIGHTED);
		submenu_is_active = true;
	}
	else if (nearestRecording != nullptr) {
		nearestRecording->setMarkerType(MARKER_TYPE_HIGHLIGHTED);
		submenu_is_active = true;
	}


	drawRecordingMarkers(actors);

	//update any recording playback
	for (auto & actor : actors) {
		if (actor.isNullActor() == false && actor.isCurrentlyPlayingRecording()) {
			Actor::update_tick_recording_replay(actor);
		}
	}

	return shouldExitMode;
}

void BirdsEyeMode::onEnterMode(SCENE_MODE aSceneMode)
{
	sceneMode = aSceneMode;
	scaleForm = GRAPHICS::REQUEST_SCALEFORM_MOVIE("instructional_buttons");

	CAM::DO_SCREEN_FADE_OUT(1000);
	WAIT(1000);

	//Find the location of our camera based on the current actor
	Ped actorPed = PLAYER::PLAYER_PED_ID();
	Vector3 startLocation = ENTITY::GET_ENTITY_COORDS(actorPed, true);
	float startHeading = ENTITY::GET_ENTITY_HEADING(actorPed);
	Vector3 camOffset;
	camOffset.x = (float)sin((startHeading *PI / 180.0f))*10.0f;
	camOffset.y = (float)cos((startHeading *PI / 180.0f))*10.0f;
	camOffset.z = 12.4;

	if (startLocation.x < 0) {
		camOffset.x = -camOffset.x;
	}
	if (startLocation.y < 0) {
		camOffset.y = -camOffset.y;
	}

	log_to_file("actor location (" + std::to_string(startLocation.x) + ", " + std::to_string(startLocation.y) + ", " + std::to_string(startLocation.z) + ")");
	log_to_file("Camera offset (" + std::to_string(camOffset.x) + ", " + std::to_string(camOffset.y) + ", " + std::to_string(camOffset.z) + ")");

	Vector3 camLocation = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(actorPed, camOffset.x, camOffset.y, camOffset.z);
	log_to_file("Camera location (" + std::to_string(camLocation.x) + ", " + std::to_string(camLocation.y) + ", " + std::to_string(camLocation.z) + ")");
	cameraHandle = CAM::CREATE_CAM_WITH_PARAMS("DEFAULT_SCRIPTED_CAMERA", camLocation.x, camLocation.y, camLocation.z, 0.0, 0.0, 0.0, 40.0, 1, 2);
	//CAM::ATTACH_CAM_TO_ENTITY(cameraHandle, actorPed, camOffset.x, camOffset.y, camOffset.z, true);
	CAM::POINT_CAM_AT_ENTITY(cameraHandle, actorPed, 0.0f, 0.0f, 0.0f, true);
	CAM::RENDER_SCRIPT_CAMS(true, 0, 3000, 1, 0);
	WAIT(100);
	CAM::STOP_CAM_POINTING(cameraHandle);
	CAM::DO_SCREEN_FADE_IN(1000);
}

void BirdsEyeMode::onExitMode()
{
	CAM::DO_SCREEN_FADE_OUT(1000);
	WAIT(1000);
	//reset cam
	CAM::RENDER_SCRIPT_CAMS(false, 0, 3000, 1, 0);
	WAIT(100);
	CAM::DO_SCREEN_FADE_IN(1000);
}

std::shared_ptr<ActorRecordingItem> BirdsEyeMode::getNearestRecording(std::vector<Actor> & actors) {
	Vector3 camPos = CAM::GET_CAM_COORD(cameraHandle);
	float nearestDistance = FLT_MAX;

	//only the one nearest recording item should be marked as highlighted 
	for (auto & actor : actors) {
		if (shouldDrawRecordingMarkers && actor.hasRecording()) {
			std::shared_ptr<ActorRecordingItem> nearestRecordingForActor = actor.getNearestRecording(camPos);
			if (nearestRecordingForActor != nullptr) {
				//calc distance to cam
				Vector3 recordingLocation = nearestRecordingForActor->getLocation();
				float recDistance = SYSTEM::VDIST(camPos.x, camPos.y, camPos.z, recordingLocation.x, recordingLocation.y, recordingLocation.z);

				if (recDistance < nearestDistance) {
					nearestRecording = nearestRecordingForActor;
					nearestActor = std::make_shared<Actor>(actor);
					nearestDistance = recDistance;
				}
			}
		}
	}
	return nearestRecording;
}

void BirdsEyeMode::drawRecordingMarkers(std::vector<Actor> & actors) {
	if (shouldDrawRecordingMarkers) {
		for (auto & actor : actors) {
			actor.drawMarkersForRecording();
		}
	}
}

void BirdsEyeMode::drawMenu() {
	if (menu_active_index > menu_max_index) {
		menu_active_index = 0;
	}
	if (menu_active_index < 0) {
		menu_active_index = menu_max_index;
	}

	int drawIndex = 0;
	//colors for swapping from active to inactive... messy
	int textColorR = 255, textColorG = 255, textColorB = 255;
	int bgColorR = 0, bgColorG = 0, bgColorB = 0;
	

	if (menu_active_index == drawIndex) {
		textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
	}
	else {
		textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
	}

	DRAW_TEXT("Exit mode", 0.88, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
	GRAPHICS::DRAW_RECT(0.93, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);

	if (menu_active_index == drawIndex) {
		menu_active_action = MENU_ITEM_EXIT_BIRDS_EYE_MODE;
	}

	drawIndex++;

	if (menu_active_index == drawIndex) {
		textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
	}
	else {
		textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
	}

	//Scene mode
	if (sceneMode == SCENE_MODE_ACTIVE) {
		DRAW_TEXT("Scene mode: Active", 0.88, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
		GRAPHICS::DRAW_RECT(0.93, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);
	}
	else {
		DRAW_TEXT("Scene mode: Setup", 0.88, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
		GRAPHICS::DRAW_RECT(0.93, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);
	}

	if (menu_active_index == drawIndex) {
		menu_active_action = MENU_ITEM_SCENE_MODE;
	}
	drawIndex++;

	if (menu_active_index == drawIndex) {
		textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
	}
	else {
		textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
	}

	if (shouldDrawRecordingMarkers) {
		DRAW_TEXT("Rec. markers: Show", 0.88, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
		GRAPHICS::DRAW_RECT(0.93, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);
	}
	else {
		DRAW_TEXT("Rec. markers: Hide", 0.88, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
		GRAPHICS::DRAW_RECT(0.93, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);
	}

	if (menu_active_index == drawIndex) {
		menu_active_action = MENU_ITEM_SHOW_REC_MARKERS;
	}


	if (nearestRecording != nullptr) {
		drawIndex++;
		if (menu_active_index == drawIndex) {
			menu_active_action = MENU_ITEM_EDIT_RECORDING;
		}

		if (menu_active_index == drawIndex) {
			textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
		}
		else {
			textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
		}


		DRAW_TEXT("Edit nearest recording", 0.88, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
		GRAPHICS::DRAW_RECT(0.93, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);

	}


	drawIndex++;

	if (menu_active_index == -1) {
		menu_active_index = 0;
	}

	menu_max_index = drawIndex - 1;
	if (menu_active_index > menu_max_index) {
		menu_active_index = menu_max_index;
	}
}

void BirdsEyeMode::drawSubMenuEdit() {
	if (submenu_active_index > submenu_max_index) {
		submenu_active_index = submenu_max_index;
	}
	if (submenu_active_index < -1) {
		submenu_active_index = -1;
	}

	if (submenu_is_active != true) {
		return;
	}
	std::shared_ptr<ActorRecordingItem> activeRecordingItem = getActiveRecordingItem();
	std::shared_ptr<Actor> activeActor = getActiveActor();


	int submenu_index = 0;
	//Edit nearest recording is always index 3
	int drawIndex = 3; 
	//colors for swapping from active to inactive... messy
	int textColorR = 255, textColorG = 255, textColorB = 255;
	int bgColorR = 0, bgColorG = 0, bgColorB = 0;

	//dynamic actions based on the type of recording item
	if (activeRecordingItem) {
		std::shared_ptr<ActorOnFootMovementRecordingItem> onfootRecordingItem = std::dynamic_pointer_cast<ActorOnFootMovementRecordingItem>(activeRecordingItem);

		if (onfootRecordingItem) {
			if (submenu_is_active && submenu_active_index == submenu_index) {
				textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
				submenu_active_action = SUBMENU_ITEM_EDIT_SPEED;
			}
			else {
				textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
			}

			DRAW_TEXT(strdup(("Walking speed:"+ roundNumber(onfootRecordingItem->getWalkSpeed())).c_str()), 0.76, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
			GRAPHICS::DRAW_RECT(0.81, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);

			drawIndex++;
			submenu_index++;
		}

	}


	/*if (submenu_is_active && submenu_active_index == submenu_index) {
		textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
		submenu_active_action = SUBMENU_ITEM_SAVE_ACTORS;
	}
	else {
		textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
	}


	DRAW_TEXT("Save actors", 0.76, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
	GRAPHICS::DRAW_RECT(0.81, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);


	drawIndex++;
	submenu_index++;

	if (submenu_is_active && submenu_active_index == submenu_index) {
		textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
		submenu_active_action = SUBMENU_ITEM_PREV_RECORDING;
	}
	else {
		textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
	}

	DRAW_TEXT("TBD", 0.76, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
	GRAPHICS::DRAW_RECT(0.81, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);

	drawIndex++;
	submenu_index++;*/

	if (submenu_is_active && submenu_active_index == submenu_index) {
		textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
		submenu_active_action = SUBMENU_ITEM_EDIT_LOCATION;
	}
	else {
		textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
	}
	
	DRAW_TEXT("Edit position", 0.76, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
	GRAPHICS::DRAW_RECT(0.81, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);

	drawIndex++;
	submenu_index++;

	if (submenu_is_active && submenu_active_index == submenu_index) {
		textColorR = 0, textColorG = 0, textColorB = 0, bgColorR = 255, bgColorG = 255, bgColorB = 255;
		submenu_active_action = SUBMENU_ITEM_NEXT_RECORDING;
	}
	else {
		textColorR = 255, textColorG = 255, textColorB = 255, bgColorR = 0, bgColorG = 0, bgColorB = 0;
	}

	std::string menuText;
	if (selectedRecording != nullptr) {
		menuText =  "Item " + std::to_string(selectedRecording->getIndex()) + ": " + selectedRecording->toUserFriendlyName()+ " (" + selectedActor->getName() + ")";
	}
	else if (highlightedRecording != nullptr) {
		menuText = "Item " + std::to_string(highlightedRecording->getIndex()) + ": " + highlightedRecording->toUserFriendlyName() + " (" + highlightedActor->getName() + ")";
	}
	else if(nearestRecording!=nullptr){
		menuText = "Item " + std::to_string(nearestRecording->getIndex()) + ": " + nearestRecording->toUserFriendlyName() + " (" + nearestActor->getName() + ")";
	}

	DRAW_TEXT(strdup((menuText).c_str()), 0.76, 0.888 - (0.04)*drawIndex, 0.3, 0.3, 0, false, false, false, false, textColorR, textColorG, textColorB, 200);
	GRAPHICS::DRAW_RECT(0.81, 0.900 - (0.04)*drawIndex, 0.113, 0.034, bgColorR, bgColorG, bgColorB, 100);

	submenu_max_index = submenu_index;
}

void BirdsEyeMode::actionMenuSelected() {
	
	if (menu_active_action == MENU_ITEM_SCENE_MODE) {
		nextWaitTicks = 200;
		if (sceneMode == SCENE_MODE_ACTIVE) {
			sceneMode = SCENE_MODE_SETUP;
		}
		else {
			sceneMode = SCENE_MODE_ACTIVE;
		}
		action_toggle_scene_mode();
	}
	else if (menu_active_action == MENU_ITEM_EXIT_BIRDS_EYE_MODE) {
		nextWaitTicks = 200;
		shouldExitMode = true;
	}
	else if (menu_active_action == MENU_ITEM_SHOW_REC_MARKERS) {
		nextWaitTicks = 200;
		shouldDrawRecordingMarkers = !shouldDrawRecordingMarkers;
	}
}

void BirdsEyeMode::actionSubMenuEditSelected()
{
	if (submenu_active_action == SUBMENU_ITEM_NEXT_RECORDING) {
		nextWaitTicks = 200;
		int nextRecordingIndex = 0;

		//cancel any ongoing location edits
		selectedRecording = nullptr;
		selectedActor = nullptr;

		std::shared_ptr<ActorRecordingItem> activeRecordingItem = getActiveRecordingItem();
		if (activeRecordingItem != nullptr) {
			
			std::shared_ptr<Actor> activeActor = getActiveActor();
			nextRecordingIndex = (activeRecordingItem->getIndex()-1)+1;
			highlightedRecording = activeActor->getRecordingAt(nextRecordingIndex);
			highlightedActor = activeActor;
			if (highlightedRecording == nullptr) {
				highlightedRecording = activeActor->getRecordingAt(0);
			}
		}

	}
	else if (submenu_active_action == SUBMENU_ITEM_EDIT_LOCATION) {
		actionToggleEditLocation();
		nextWaitTicks = 200;
	}
	else if (submenu_active_action == SUBMENU_ITEM_EDIT_SPEED) {
		actionInputWalkSpeed();
		nextWaitTicks = 200;
	}
}

void BirdsEyeMode::actionToggleEditLocation()
{
	if (selectedRecording == nullptr) {
		if (highlightedRecording != nullptr) {
			log_to_file("Selected nearest recording");
			selectedRecording = highlightedRecording;
			highlightedRecording->setMarkerType(MARKER_TYPE_SELECTED);
			selectedActor = highlightedActor;
		}
		else if (nearestRecording != nullptr) {
			log_to_file("Selected nearest recording");
			selectedRecording = nearestRecording;
			selectedRecording->setMarkerType(MARKER_TYPE_SELECTED);
			selectedActor = nearestActor;
		}
		nextWaitTicks = 200;
	}
	else if (selectedRecording != nullptr) {
		log_to_file("Deselected nearest recording");
		selectedRecording->setMarkerType(MARKER_TYPE_NORMAL);
		selectedRecording = nullptr;
		selectedActor = nullptr;
		nextWaitTicks = 200;
	}
}

float BirdsEyeMode::actionInputFloat()
{
	GAMEPLAY::DISPLAY_ONSCREEN_KEYBOARD(true, "FMMC_KEY_TIP8", "", "", "", "", "", 6);

	while (GAMEPLAY::UPDATE_ONSCREEN_KEYBOARD() == 0) {
		WAIT(0);
	}


	if (GAMEPLAY::IS_STRING_NULL_OR_EMPTY(GAMEPLAY::GET_ONSCREEN_KEYBOARD_RESULT())) {
		log_to_file("Got null keyboard value");
		return -1.0f;
	}
	char * keyboardValue = GAMEPLAY::GET_ONSCREEN_KEYBOARD_RESULT();
	std::string strValue = std::string(keyboardValue);
	log_to_file("Got keyboard value " + strValue);
	return atof(keyboardValue);
}

void BirdsEyeMode::actionInputWalkSpeed()
{
	std::shared_ptr<ActorRecordingItem> activeRecordingItem = getActiveRecordingItem();
	std::shared_ptr<ActorOnFootMovementRecordingItem> onfootRecordingItem = std::dynamic_pointer_cast<ActorOnFootMovementRecordingItem>(activeRecordingItem);
	if (onfootRecordingItem
		) {
		set_status_text("Enter walk speed (1.0 = walk 2.0 = run)");
		float f = actionInputFloat();
		if (f > 0.0) {
			onfootRecordingItem->setWalkSpeed(f);
		}
	}
}


void BirdsEyeMode::drawInstructions() {
	if (GRAPHICS::HAS_SCALEFORM_MOVIE_LOADED(scaleForm)) {
		GRAPHICS::CALL_SCALEFORM_MOVIE_METHOD(scaleForm, "CLEAR_ALL");

		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION(scaleForm, "TOGGLE_MOUSE_BUTTONS");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_BOOL(0);
		GRAPHICS::_POP_SCALEFORM_MOVIE_FUNCTION_VOID();

		GRAPHICS::CALL_SCALEFORM_MOVIE_METHOD(scaleForm, "CREATE_CONTAINER");

		char* altControlKey = CONTROLS::_GET_CONTROL_ACTION_NAME(2, 19, 1);
		char* mouseLeftButton = CONTROLS::_GET_CONTROL_ACTION_NAME(2, 330, 1);
		char* mouseRightButton = CONTROLS::_GET_CONTROL_ACTION_NAME(2, 329, 1);
		char* spaceControlKey = CONTROLS::_GET_CONTROL_ACTION_NAME(2, 22, 1);
		char* shiftControlKey = CONTROLS::_GET_CONTROL_ACTION_NAME(2, 21, 1);

		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION(scaleForm, "SET_DATA_SLOT");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_INT(0);
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("t_D");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("t_A");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("t_S");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("t_W");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("Move camera");
		GRAPHICS::_POP_SCALEFORM_MOVIE_FUNCTION_VOID();

		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION(scaleForm, "SET_DATA_SLOT");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_INT(1);
		GRAPHICS::_0xE83A3E3557A56640(shiftControlKey);
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("Increase camera speed");
		GRAPHICS::_POP_SCALEFORM_MOVIE_FUNCTION_VOID();

		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION(scaleForm, "SET_DATA_SLOT");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_INT(2);
		GRAPHICS::_0xE83A3E3557A56640(mouseLeftButton);
		GRAPHICS::_0xE83A3E3557A56640(mouseRightButton);
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("Camera up/down");
		GRAPHICS::_POP_SCALEFORM_MOVIE_FUNCTION_VOID();

		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION(scaleForm, "SET_DATA_SLOT");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_INT(3);
		GRAPHICS::_0xE83A3E3557A56640(spaceControlKey);
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_STRING("Quick edit position");
		GRAPHICS::_POP_SCALEFORM_MOVIE_FUNCTION_VOID();

		GRAPHICS::_POP_SCALEFORM_MOVIE_FUNCTION_VOID();

		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION(scaleForm, "DRAW_INSTRUCTIONAL_BUTTONS");
		GRAPHICS::_PUSH_SCALEFORM_MOVIE_FUNCTION_PARAMETER_INT(-1);
		GRAPHICS::_POP_SCALEFORM_MOVIE_FUNCTION_VOID();

		GRAPHICS::DRAW_SCALEFORM_MOVIE_FULLSCREEN(scaleForm, 255, 255, 255, 255,1);
	}
	else {
		log_to_file("Scaleform has not loaded. scaleForm has value " + std::to_string(scaleForm));
	}
}

bool BirdsEyeMode::checkInputRotation()
{

	float rightAxisX = CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, 220);
	float rightAxisY = CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, 221);

	if (rightAxisX != 0.0 || rightAxisY != 0.0) {
		//Rotate camera - Multiply by sensitivity settings
		Vector3 currentRotation = CAM::GET_CAM_ROT(cameraHandle, 2);
		currentRotation.z += rightAxisX*-10.0f;
		currentRotation.x += rightAxisY*-5.0f;
		CAM::SET_CAM_ROT(cameraHandle, currentRotation.x, currentRotation.y, currentRotation.z,2);
		return true;
	}
	else {
		return false;
	}

}

std::shared_ptr<ActorRecordingItem> BirdsEyeMode::getActiveRecordingItem()
{
	std::shared_ptr<ActorRecordingItem> activeRecordingItem = nullptr;
	if (selectedRecording != nullptr) {
		activeRecordingItem = selectedRecording;
	}
	else if (highlightedRecording != nullptr) {
		activeRecordingItem = highlightedRecording;
	}
	else if (nearestRecording != nullptr) {
		activeRecordingItem = nearestRecording;
	}
	return activeRecordingItem;
}

std::shared_ptr<Actor> BirdsEyeMode::getActiveActor()
{
	std::shared_ptr<Actor> activeActor = nullptr;
	if (selectedActor != nullptr) {
		activeActor = selectedActor;
	}
	else if (highlightedActor != nullptr) {
		activeActor = highlightedActor;
	}
	else if (nearestActor != nullptr) {
		activeActor = nearestActor;
	}
	return activeActor;
}

bool BirdsEyeMode::checkInputAction()
{
	if (is_key_pressed_for_select_item()) {
		actionToggleEditLocation();
		nextWaitTicks = 200;

		return true;
	} 
	else {
		return false;
	}
}


bool BirdsEyeMode::checkInputMovement()
{

	Vector3 camDelta = {};

	if (is_key_pressed_for_exit_mode()) {
		shouldExitMode = true;
	}
	else {
		bool isMovement = false;
		if (is_key_pressed_for_forward()) {
			camDelta.x = 1.0;
			isMovement = true;
		}
		if (is_key_pressed_for_backward()) {
			camDelta.x = -1.0;
			isMovement = true;
		}
		if (is_key_pressed_for_left()) {
			camDelta.y = -1.0;
			isMovement = true;
		}
		if (is_key_pressed_for_right()) {
			camDelta.y = 1.0;
			isMovement = true;
		}
		if (CONTROLS::IS_DISABLED_CONTROL_PRESSED(2, 329)) {//LMouseBtn
			camDelta.z = 0.3;
			isMovement = true;
		}
		if (CONTROLS::IS_DISABLED_CONTROL_PRESSED(2, 330) ){//RMouseBtn
			camDelta.z = -0.3;
			isMovement = true;
		}
		if (isMovement) {
			if (is_key_pressed_for_run()) {
				camDelta.x *= 3; 
				camDelta.y *= 3;
				camDelta.z *= 3;
			}

			camNewPos = CAM::GET_CAM_COORD(cameraHandle);
			camLastPos.x = camNewPos.x;
			camLastPos.y = camNewPos.y;
			camLastPos.z = camNewPos.z;

			Vector3 camRot = {};
			camRot= CAM::GET_CAM_ROT(cameraHandle, 2);
			//camera rotation is not as expected. .x value is rotation in the z-plane (view up/down) and third paramter is the rotation in the x,y plane.

			Vector3 direction = {};
			direction = MathUtils::rotationToDirection(camRot);

			//forward motion
			if (camDelta.x != 0.0) {
				camNewPos.x += direction.x * camDelta.x * cameraSpeedFactor;
				camNewPos.y += direction.y * camDelta.x * cameraSpeedFactor;
				camNewPos.z += direction.z * camDelta.x * cameraSpeedFactor;
			}

			//sideways motion
			if (camDelta.y != 0.0 ) {
				//straight up
				Vector3 b = { };
				b.z = 1.0;

				Vector3 sideWays = {};
				sideWays = MathUtils::crossProduct(direction, b);
				
				camNewPos.x += sideWays.x * camDelta.y * cameraSpeedFactor;
				camNewPos.y += sideWays.y * camDelta.y * cameraSpeedFactor;
			}

			//up/down
			if (camDelta.z != 0.0) {
				camNewPos.z += camDelta.z * cameraSpeedFactor;
			}

			CAM::SET_CAM_COORD(cameraHandle, camNewPos.x, camNewPos.y, camNewPos.z);

			//log_to_file("Cam pos vs last  (" + std::to_string(camNewPos.x) + ", " + std::to_string(camNewPos.y) + ", " + std::to_string(camNewPos.z) + ")(" + std::to_string(camLastPos.x) + ", " + std::to_string(camLastPos.y) + ", " + std::to_string(camLastPos.z) + ")");
			return true;
		}

	}
	return false;

}

void BirdsEyeMode::disableControls() {
	std::vector<int> disabledControls = {
		0,2,3,4,5,6,16,17,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,44,45,156,243,257,261,262,263,264,267,268,269,270,271,272,273
	};

	for (auto & controlCode : disabledControls) {
		CONTROLS::DISABLE_CONTROL_ACTION(0, controlCode, 1);
	}
	//INPUT_NEXT_CAMERA = 0,
	//INPUT_LOOK_UD = 2,
	//INPUT_LOOK_UP_ONLY = 3,
	//INPUT_LOOK_DOWN_ONLY = 4,
	//INPUT_LOOK_LEFT_ONLY = 5,
	//INPUT_LOOK_RIGHT_ONLY = 6,
	//INPUT_SELECT_NEXT_WEAPON = 16,
	//INPUT_SELECT_PREV_WEAPON = 17,
	//INPUT_CHARACTER_WHEEL = 19,
	//INPUT_MULTIPLAYER_INFO = 20,
	//INPUT_SPRINT = 21,
	//INPUT_JUMP = 22,
	//INPUT_ENTER = 23,
	//INPUT_ATTACK = 24,
	//INPUT_AIM = 25,
	//INPUT_LOOK_BEHIND = 26,
	//INPUT_PHONE = 27,
	//INPUT_SPECIAL_ABILITY = 28,
	//INPUT_SPECIAL_ABILITY_SECONDARY = 29,
	//INPUT_MOVE_LR = 30,
	//INPUT_MOVE_UD = 31,
	//INPUT_MOVE_UP_ONLY = 32,
	//INPUT_MOVE_DOWN_ONLY = 33,
	//INPUT_MOVE_LEFT_ONLY = 34,
	//INPUT_MOVE_RIGHT_ONLY = 35,
	//INPUT_DUCK = 36,
	//INPUT_SELECT_WEAPON = 37,
	//INPUT_COVER = 44,
	//INPUT_RELOAD = 45,
	//INPUT_MAP = 156,
	//INPUT_ENTER_CHEAT_CODE = 243,
	//INPUT_ATTACK2 = 257,
	//INPUT_PREV_WEAPON = 261,
	//INPUT_NEXT_WEAPON = 262,
	//INPUT_MELEE_ATTACK1 = 263,
	//INPUT_MELEE_ATTACK2 = 264,
	//INPUT_MOVE_LEFT = 266,
	//INPUT_MOVE_RIGHT = 267,
	//INPUT_MOVE_UP = 268,
	//INPUT_MOVE_DOWN = 269,
	//INPUT_LOOK_LEFT = 270,
	//INPUT_LOOK_RIGHT = 271,
	//INPUT_LOOK_UP = 272,
	//INPUT_LOOK_DOWN = 273,

}

bool BirdsEyeMode::is_key_pressed_for_select_item() {
	//ALT+ R
	if (IsKeyDown(VK_SPACE)) {
		return true;
	}
	else {
		return false;
	}
}

bool BirdsEyeMode::is_key_pressed_for_exit_mode() {
	//ALT+ R
	if (IsKeyDown(VK_MENU) && IsKeyDown(0x52)) {
		return true;
	}
	else {
		return false;
	}
}

bool BirdsEyeMode::is_key_pressed_for_forward() {
	//W
	if (IsKeyDown(0x57)) {
		return true;
	}
	else {
		return false;
	}
}
bool BirdsEyeMode::is_key_pressed_for_backward() {
	//W
	if (IsKeyDown(0x53)) {
		return true;
	}
	else {
		return false;
	}
}
bool BirdsEyeMode::is_key_pressed_for_left() {
	//A
	if (IsKeyDown(0x41)) {
		return true;
	}
	else {
		return false;
	}
}
bool BirdsEyeMode::is_key_pressed_for_right() {
	//D
	if (IsKeyDown(0x44)) {
		return true;
	}
	else {
		return false;
	}
}

bool BirdsEyeMode::is_key_pressed_for_run() {
	//D
	if (IsKeyDown(VK_SHIFT)) {
		return true;
	}
	else {
		return false;
	}
}


