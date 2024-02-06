#include "levelEditorInf.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "entity.h"
#include "selection.h"
#include "infoPanel.h"
#include "browser.h"
#include "camera.h"
#include "error.h"
#include "sharedState.h"
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editorLevel.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_Editor/editorResources.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Editor/LevelEditor/Rendering/grid2d.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/lineDraw3d.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>
#include <SDL.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	Editor_LevelInf s_levelInf;
	static char s_infArg0[256], s_infArg1[256], s_infArg2[256], s_infArg3[256], s_infArg4[256], s_infArgExtra[256];

	const char* c_elevStopCmdName[] =
	{
		"Message", // ISC_MESSAGE
		"Adjoin",  // ISC_ADJOIN
		"Texture", // ISC_TEXTURE
		"Page"     // ISC_PAGE
	};

	const char* c_infElevTypeName[] =
	{
		// Special, "high level" elevators.
		"Basic",		// IET_BASIC
		"Basic_Auto",	// IET_BASIC_AUTO
		"Inv",			// IET_INV
		"Door",			// IET_DOOR
		"Door_Inv",		// IET_DOOR_INV
		"Door_Mid",		// IET_DOOR_MID
		"Morph_Spin1",	// IET_MORPH_SPIN1
		"Morph_Spin2",	// IET_MORPH_SPIN2
		"Morph_Move1",	// IET_MORPH_MOVE1
		"Morph_Move1",	// IET_MORPH_MOVE2

		// Standard Elevators
		"Move_Ceiling",			// IET_MOVE_CEILING
		"Move_Floor",			// IET_MOVE_FLOOR
		"Move_FC",				// IET_MOVE_FC
		"Move_Offset",			// IET_MOVE_OFFSET
		"Move_Wall",			// IET_MOVE_WALL
		"Rotate_Wall",			// IET_ROTATE_WALL
		"Scroll_Wall",			// IET_SCROLL_WALL
		"Scroll_Floor",			// IET_SCROLL_FLOOR
		"Scroll_Ceiling",		// IET_SCROLL_CEILING
		"Change_Light",			// IET_CHANGE_LIGHT
		"Change_Wall_Light",	// IET_CHANGE_WALL_LIGHT
	};

	void editor_infInit()
	{
		s_levelInf.item.clear();
		s_levelInf.elevator.clear();
		s_levelInf.trigger.clear();
		s_levelInf.teleport.clear();
	}

	void editor_infDestroy()
	{
		s32 count = (s32)s_levelInf.elevator.size();
		for (s32 i = 0; i < count; i++)
		{
			delete s_levelInf.elevator[i];
		}

		count = (s32)s_levelInf.trigger.size();
		for (s32 i = 0; i < count; i++)
		{
			delete s_levelInf.trigger[i];
		}

		count = (s32)s_levelInf.teleport.size();
		for (s32 i = 0; i < count; i++)
		{
			delete s_levelInf.teleport[i];
		}

		editor_infInit();
	}

	Editor_InfElevator* allocElev(Editor_InfItem* item)
	{
		Editor_InfElevator* elev = new Editor_InfElevator();
		s_levelInf.elevator.push_back(elev);
		elev->classId = IIC_ELEVATOR;
		item->classData.push_back((Editor_InfClass*)elev);
		return elev;
	}

	Editor_InfTrigger* allocTrigger(Editor_InfItem* item)
	{
		Editor_InfTrigger* trigger = new Editor_InfTrigger();
		s_levelInf.trigger.push_back(trigger);
		trigger->classId = IIC_TRIGGER;
		item->classData.push_back((Editor_InfClass*)trigger);
		return trigger;
	}

	Editor_InfTeleporter* allocTeleporter(Editor_InfItem* item)
	{
		Editor_InfTeleporter* teleport = new Editor_InfTeleporter();
		s_levelInf.teleport.push_back(teleport);
		teleport->classId = IIC_TELEPORTER;
		item->classData.push_back((Editor_InfClass*)teleport);
		return teleport;
	}

	void freeElevator(Editor_InfElevator* elev)
	{
		const s32 count = (s32)s_levelInf.elevator.size();
		s32 index = -1;
		for (s32 i = 0; i < count; i++)
		{
			if (s_levelInf.elevator[i] == elev)
			{
				index = i;
				break;
			}
		}
		if (index < 0) { return; }
		delete elev;
		for (s32 i = index; i < count - 1; i++)
		{
			s_levelInf.elevator[i] = s_levelInf.elevator[i + 1];
		}
		s_levelInf.elevator.pop_back();
	}

	void freeTrigger(Editor_InfTrigger* trigger)
	{
		const s32 count = (s32)s_levelInf.trigger.size();
		s32 index = -1;
		for (s32 i = 0; i < count; i++)
		{
			if (s_levelInf.trigger[i] == trigger)
			{
				index = i;
				break;
			}
		}
		if (index < 0) { return; }
		delete trigger;
		for (s32 i = index; i < count - 1; i++)
		{
			s_levelInf.trigger[i] = s_levelInf.trigger[i + 1];
		}
		s_levelInf.trigger.pop_back();
	}

	void freeTeleporter(Editor_InfTeleporter* teleporter)
	{
		const s32 count = (s32)s_levelInf.teleport.size();
		s32 index = -1;
		for (s32 i = 0; i < count; i++)
		{
			if (s_levelInf.teleport[i] == teleporter)
			{
				index = i;
				break;
			}
		}
		if (index < 0) { return; }
		delete teleporter;
		for (s32 i = index; i < count - 1; i++)
		{
			s_levelInf.teleport[i] = s_levelInf.teleport[i + 1];
		}
		s_levelInf.teleport.pop_back();
	}

	Editor_InfElevator* getElevFromClassData(Editor_InfClass* data)
	{
		if (data->classId != IIC_ELEVATOR) { return nullptr; }
		return (Editor_InfElevator*)data;
	}

	const Editor_InfElevator* getElevFromClassData(const Editor_InfClass* data)
	{
		if (data->classId != IIC_ELEVATOR) { return nullptr; }
		return (Editor_InfElevator*)data;
	}

	Editor_InfTrigger* getTriggerFromClassData(Editor_InfClass* data)
	{
		if (data->classId != IIC_TRIGGER) { return nullptr; }
		return (Editor_InfTrigger*)data;
	}

	const Editor_InfTrigger* getSectorTriggerFromClassData(const Editor_InfClass* data)
	{
		if (data->classId != IIC_TRIGGER) { return nullptr; }
		return (Editor_InfTrigger*)data;
	}

	Editor_InfTeleporter* getTeleportFromClassData(Editor_InfClass* data)
	{
		if (data->classId != IIC_TELEPORTER) { return nullptr; }
		return (Editor_InfTeleporter*)data;
	}

	const Editor_InfTeleporter* getTeleportFromClassData(const Editor_InfClass* data)
	{
		if (data->classId != IIC_TELEPORTER) { return nullptr; }
		return (Editor_InfTeleporter*)data;
	}

	EditorSector* findSector(const char* itemName)
	{
		const s32 count = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < count; i++, sector++)
		{
			// Note in vanilla DF, only the first 16 characters are compared.
			if (strncasecmp(itemName, sector->name.c_str(), 16) == 0)
			{
				return sector;
			}
		}
		return nullptr;
	}

	void editor_parseMessage(Editor_InfMessageType* type, u32* arg1, u32* arg2, u32* evt, const char* infArg0, const char* infArg1, const char* infArg2, bool elevator)
	{
		const KEYWORD msgId = getKeywordIndex(infArg0);
		char* endPtr = nullptr;

		switch (msgId)
		{
			case KW_NEXT_STOP:
				*type = IMT_NEXT_STOP;
				break;
			case KW_PREV_STOP:
				*type = IMT_PREV_STOP;
				break;
			case KW_GOTO_STOP:
				*type = IMT_GOTO_STOP;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_MASTER_ON:
				*type = IMT_MASTER_ON;
				break;
			case KW_MASTER_OFF:
				*type = IMT_MASTER_OFF;
				break;
			case KW_SET_BITS:
				*type = IMT_SET_BITS;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				*arg2 = strtoul(infArg2, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_CLEAR_BITS:
				*type = IMT_CLEAR_BITS;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				*arg2 = strtoul(infArg2, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_COMPLETE:
				*type = IMT_COMPLETE;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_LIGHTS:
				*type = IMT_LIGHTS;
				break;
			case KW_M_TRIGGER:
			default:
				if (elevator)
				{
					// Elevators can use a few additional message types, but these are treated as M_TRIGGER for either trigger type.
					switch (msgId)
					{
						case KW_DONE:
							*type = IMT_DONE;
							break;
						case KW_WAKEUP:
							*type = IMT_WAKEUP;
							break;
						default:
							*type = IMT_TRIGGER;
					}
				}
				else // Trigger
				{
					*type = IMT_TRIGGER;
				}
		}
	}

	// TODO: Share code.
	void parseTarget(Editor_InfMessage* msg, char* arg)
	{
		char* endPtr = nullptr;
		msg->targetWall = -1;

		// There should be a variant of strstr() that returns a non-constant pointer, but in Visual Studio it is always constant.
		char* parenOpen = (char*)strstr(arg, "(");
		// This message targets a wall rather than a whole sector.
		if (parenOpen)
		{
			*parenOpen = 0;
			parenOpen++;

			char* parenClose = (char*)strstr(arg, ")");
			// This should never be true and this doesn't seem to be hit at runtime.
			// I wonder if this was meant to be { char* parenClose = (char*)strstr(parenOpen, ")"); } - which would make more sense.
			// Or it could have been check *before* the location at ")" was set to 0 above.
			if (parenClose)
			{
				*parenClose = 0;
			}
			// Finally parse the integer and set the wall index.
			msg->targetWall = strtol(parenOpen, &endPtr, 10);
		}
		msg->targetSector = arg;
	}

	void parseTarget(Editor_InfClient* client, char* arg)
	{
		char* endPtr = nullptr;
		client->targetWall = -1;

		// There should be a variant of strstr() that returns a non-constant pointer, but in Visual Studio it is always constant.
		char* parenOpen = (char*)strstr(arg, "(");
		// This message targets a wall rather than a whole sector.
		if (parenOpen)
		{
			*parenOpen = 0;
			parenOpen++;

			char* parenClose = (char*)strstr(arg, ")");
			// This should never be true and this doesn't seem to be hit at runtime.
			// I wonder if this was meant to be { char* parenClose = (char*)strstr(parenOpen, ")"); } - which would make more sense.
			// Or it could have been check *before* the location at ")" was set to 0 above.
			if (parenClose)
			{
				*parenClose = 0;
			}
			// Finally parse the integer and set the wall index.
			client->targetWall = strtol(parenOpen, &endPtr, 10);
		}
		client->targetSector = arg;
	}

	bool editor_parseElevatorCommand(s32 argCount, KEYWORD action, bool seqEnd, Editor_InfElevator* elev, s32& addon)
	{
		char* endPtr = nullptr;
		switch (action)
		{
			case KW_START:
			{
				elev->start = strtol(s_infArg0, &endPtr, 10);
				elev->overrideSet |= IEO_START;
			} break;
			case KW_STOP:
			{
				elev->stops.push_back({});
				Editor_InfStop* stop = &elev->stops.back();
				stop->overrideSet = ISO_NONE;

				// Calculate the stop value.
				if (s_infArg0[0] == '@')  // Relative Value
				{
					stop->relative = true;
					stop->value = strtof(&s_infArg0[1], &endPtr);
				}
				else if ((s_infArg0[0] >= '0' && s_infArg0[0] <= '9') || s_infArg0[0] == '-' || s_infArg0[0] == '.')  // Numeric Value
				{
					stop->relative = false;
					stop->value = strtof(s_infArg0, &endPtr);
				}
				else  // Value from named sector.
				{
					stop->relative = false;
					stop->value = 0.0f;
					stop->fromSectorFloor = s_infArg0;
				}

				if (argCount < 3)
				{
					return seqEnd;
				}

				stop->overrideSet |= ISO_DELAY;
				stop->delay = 0.0f;
				if ((s_infArg1[0] >= '0' && s_infArg1[0] <= '9') || s_infArg1[0] == '-' || s_infArg1[0] == '.')
				{
					stop->delayType = SDELAY_SECONDS;
					stop->delay = strtof(s_infArg1, &endPtr);
				}
				else if (strcasecmp(s_infArg1, "HOLD") == 0)
				{
					stop->delayType = SDELAY_HOLD;
				}
				else if (strcasecmp(s_infArg1, "TERMINATE") == 0)
				{
					stop->delayType = SDELAY_TERMINATE;
				}
				else if (strcasecmp(s_infArg1, "COMPLETE") == 0)
				{
					stop->delayType = SDELAY_COMPLETE;
				}
				else
				{
					// Note: this is actually a bug in the original game, but some mods rely on it so...
					stop->delayType = SDELAY_PREV_VALUE;
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_SPEED:
			{
				elev->speed = strtof(s_infArg0, &endPtr);
				elev->overrideSet |= IEO_SPEED;
			} break;
			case KW_MASTER:
			{
				elev->master = false;
				elev->overrideSet |= IEO_MASTER;
			} break;
			case KW_ANGLE:
			{
				elev->angle = strtof(s_infArg0, &endPtr) * PI / 180.0f;
				elev->overrideSet |= IEO_ANGLE;
			} break;
			case KW_ADJOIN:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->adjoinCmd.push_back({});
					Editor_InfAdjoinCmd* adjoinCmd = &stop->adjoinCmd.back();

					adjoinCmd->sector0 = s_infArg1;
					adjoinCmd->sector1 = s_infArg3;
					adjoinCmd->wallIndex0 = strtol(s_infArg2, &endPtr, 10);
					adjoinCmd->wallIndex1 = strtol(s_infArg4, &endPtr, 10);
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_TEXTURE:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->textureCmd.push_back({});
					Editor_InfTextureCmd* texCmd = &stop->textureCmd.back();

					texCmd->donorSector = s_infArg2;
					texCmd->fromCeiling = s_infArg1[0] == 'C' || s_infArg1[0] == 'c';
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_SLAVE:
			{
				elev->slaves.push_back({});
				Editor_InfSlave* slave = &elev->slaves.back();

				slave->name = s_infArg0;
				slave->angleOffset = 0.0f;
				if (argCount > 2)
				{
					slave->angleOffset = strtof(s_infArg1, &endPtr);
				}
				elev->overrideSet |= IEO_SLAVES;
			} break;
			case KW_MESSAGE:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->msg.push_back({});
					Editor_InfMessage* msg = &stop->msg.back();
					parseTarget(msg, s_infArg1);
					msg->type = IMT_TRIGGER;
					msg->eventFlags = INF_EVENT_NONE;

					if (argCount >= 5)
					{
						msg->eventFlags = strtoul(s_infArg0, &endPtr, 10);
					}
					if (argCount > 3)
					{
						editor_parseMessage(&msg->type, &msg->arg[0], &msg->arg[1], &msg->eventFlags, s_infArg2, s_infArg3, s_infArg4, true);
					}
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_EVENT_MASK:
			{
				if (s_infArg0[0] == '*')
				{
					elev->eventMask = INF_EVENT_ANY;	// everything
				}
				else
				{
					elev->eventMask = strtoul(s_infArg0, &endPtr, 10);
				}
				elev->overrideSet |= IEO_EVENT_MASK;
			} break;
			case KW_ENTITY_MASK:
			case KW_OBJECT_MASK:
			{
				// Entity_mask and Object_mask are buggy for elevators...
				if (s_infArg0[0] == '*')
				{
					elev->eventMask = INF_ENTITY_ANY;
				}
				else
				{
					elev->eventMask = strtoul(s_infArg0, &endPtr, 10);
				}
				elev->overrideSet |= IEO_ENTITY_MASK;
			} break;
			case KW_CENTER:
			{
				const f32 centerX = strtof(s_infArg0, &endPtr);
				const f32 centerZ = strtof(s_infArg1, &endPtr);
				elev->dirOrCenter = { centerX, centerZ };
				elev->overrideSet |= IEO_DIR;
			} break;
			case KW_KEY_COLON:
			{
				KEYWORD key = getKeywordIndex(s_infArg0);
				if (key == KW_RED)
				{
					elev->key[addon] = KEY_RED;
				}
				else if (key == KW_YELLOW)
				{
					elev->key[addon] = KEY_YELLOW;
				}
				else if (key == KW_BLUE)
				{
					elev->key[addon] = KEY_BLUE;
				}
				elev->overrideSet |= (addon > 0 ? IEO_KEY1 : IEO_KEY0);
			} break;
			// This entry is required for special cases, like IELEV_SP_DOOR_MID, where multiple elevators are created at once.
			// This way, we can modify the first created elevator as well as the last.
			// It allows allows us to modify previous classes... but this isn't recommended.
			case KW_ADDON:
			{
				addon = strtol(s_infArg0, &endPtr, 10);
			} break;
			case KW_FLAGS:
			{
				elev->flags = strtoul(s_infArg0, &endPtr, 10);
				elev->overrideSet |= IEO_FLAGS;
			} break;
			case KW_SOUND_COLON:
			{
				std::string soundName = s_infArg1;
				if (s_infArg1[0] >= '0' && s_infArg1[0] <= '9')
				{
					// Any numeric value means "no sound."
					soundName = "none";
				}

				// Determine which elevator sound to assign soundId to.
				s32 soundIdx = strtol(s_infArg0, &endPtr, 10) - 1;
				if (soundIdx >= 0 && soundIdx < 3)
				{
					elev->sounds[soundIdx] = soundName;
					elev->overrideSet |= (IEO_SOUND0 << soundIdx);
				}
			} break;
			case KW_PAGE:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->page = s_infArg1;
				}
				elev->overrideSet |= IEO_STOPS;
				stop->overrideSet |= ISO_PAGE;
			} break;
			case KW_SEQEND:
			{
				seqEnd = true;
			} break;
		}

		return seqEnd;
	}

	bool parseElevator(TFE_Parser& parser, size_t& bufferPos, const char* itemName, Editor_InfItem* item)
	{
		EditorSector* itemSector = findSector(itemName);
		if (!itemSector)
		{
			parser.readLine(bufferPos);
			return false;
		}
						
		KEYWORD itemSubclass = getKeywordIndex(s_infArg1);
		// Special classes - these map to the 11 core elevator types but have special defaults and/or automatically setup stops.
		Editor_InfElevType type;
		if (itemSubclass <= KW_MORPH_MOVE2)
		{
			switch (itemSubclass)
			{
			case KW_BASIC:
				type = IET_BASIC;
				break;
			case KW_BASIC_AUTO:
				type = IET_BASIC_AUTO;
				break;
			case KW_INV:
				type = IET_INV;
				break;
			case KW_DOOR:
				type = IET_DOOR;
				break;
			case KW_DOOR_INV:
				type = IET_DOOR_INV;
				break;
			case KW_DOOR_MID:
				type = IET_DOOR_MID;
				break;
			case KW_MORPH_SPIN1:
				type = IET_MORPH_SPIN1;
				break;
			case KW_MORPH_SPIN2:
				type = IET_MORPH_SPIN2;
				break;
			case KW_MORPH_MOVE1:
				type = IET_MORPH_MOVE1;
				break;
			case KW_MORPH_MOVE2:
				type = IET_MORPH_MOVE2;
				break;
			default:
				// Invalid type.
				LE_WARNING("Unsupported INF elevator type '%s' (ignored by Dark Forces).", s_infArg1);
				parser.readLine(bufferPos);
				return false;
			};
		}
		// One of the core 11 types.
		else
		{
			switch (itemSubclass)
			{
			case KW_MOVE_CEILING:
				type = IET_MOVE_CEILING;
				break;
			case KW_MOVE_FLOOR:
				type = IET_MOVE_FLOOR;
				break;
			case KW_MOVE_FC:
				type = IET_MOVE_FC;
				break;
			case KW_MOVE_OFFSET:
				type = IET_MOVE_OFFSET;
				break;
			case KW_MOVE_WALL:
				type = IET_MOVE_WALL;
				break;
			case KW_ROTATE_WALL:
				type = IET_ROTATE_WALL;
				break;
			case KW_SCROLL_WALL:
				type = IET_SCROLL_WALL;
				break;
			case KW_SCROLL_FLOOR:
				type = IET_SCROLL_FLOOR;
				break;
			case KW_SCROLL_CEILING:
				type = IET_SCROLL_CEILING;
				break;
			case KW_CHANGE_LIGHT:
				type = IET_CHANGE_LIGHT;
				break;
			case KW_CHANGE_WALL_LIGHT:
				type = IET_CHANGE_WALL_LIGHT;
				break;
			default:
				// Invalid type.
				LE_WARNING("Unsupported INF elevator type '%s' (ignored by Dark Forces).", s_infArg1);
				parser.readLine(bufferPos);
				return false;
			}
		}
		Editor_InfElevator* elev = allocElev(item);
		elev->type = type;
		elev->overrideSet = IEO_NONE;
		
		s32 addon = 0;
		bool seqEnd = false;
		while (!seqEnd)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }
			// There is another class in this sequence, exit out.
			if (strstr(line, "CLASS")) { break; }

			char id[256];
			s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArg4, s_infArgExtra);
			KEYWORD action = getKeywordIndex(id);
			if (action == KW_UNKNOWN)
			{
				LE_WARNING("Unknown elevator command - '%s'.", id);
			}
			seqEnd = editor_parseElevatorCommand(argCount, action, seqEnd, elev, addon);
		} // while (!seqEnd)

		return seqEnd;
	}

	bool parseSectorTrigger(TFE_Parser& parser, size_t& bufferPos, s32 argCount, const char* itemName, Editor_InfItem* item)
	{
		EditorSector* itemSector = findSector(itemName);
		if (!itemSector)
		{
			parser.readLine(bufferPos);
			return false;
		}

		Editor_InfTrigger* trigger = allocTrigger(item);
		trigger->overrideSet = ITO_NONE;
		trigger->type = ITRIGGER_SECTOR;

		bool seqEnd = false;
		char* endPtr = nullptr;
		while (!seqEnd)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }
			// There is another class in this sequence, exit out.
			if (strstr(line, "CLASS")) { break; }

			char id[256];
			s32 argCount = sscanf(line, " %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
			KEYWORD itemId = getKeywordIndex(id);
			assert(itemId != KW_UNKNOWN);
			switch (itemId)
			{
				case KW_CLIENT:
				{
					Editor_InfClient client = {};
					parseTarget(&client, s_infArg0);
					if (argCount > 2)
					{
						client.eventMask = strtoul(s_infArg1, &endPtr, 10);
					}
					trigger->clients.push_back(client);
				} break;
				case KW_MASTER:
				{
					trigger->master = false;
					trigger->overrideSet |= ITO_MASTER;
				} break;
				case KW_TEXT:
				{
					if (s_infArg0[0] >= '0' && s_infArg0[0] <= '9')
					{
						trigger->textId = strtoul(s_infArg0, &endPtr, 10);
						trigger->overrideSet |= ITO_TEXT;
					}
				} break;
				case KW_MESSAGE:
				{
					trigger->overrideSet |= ITO_MSG;
					editor_parseMessage(&trigger->cmd, &trigger->arg[0], &trigger->arg[1], nullptr, s_infArg0, s_infArg1, s_infArg2, false);
				} break;
				case KW_EVENT_MASK:
				{
					trigger->overrideSet |= ITO_EVENT_MASK;
					if (s_infArg0[0] == '*')
					{
						trigger->eventMask = INF_EVENT_ANY;
					}
					else
					{
						trigger->eventMask = strtoul(s_infArg0, &endPtr, 10);
					}
				} break;
				case KW_ENTITY_MASK:
				case KW_OBJECT_MASK:
				{
					trigger->overrideSet |= ITO_ENTITY_MASK;
					if (s_infArg0[0] == '*')
					{
						trigger->entityMask = INF_ENTITY_ANY;
					}
					else
					{
						trigger->entityMask = strtoul(s_infArg0, &endPtr, 10);
					}
				} break;
				case KW_EVENT:
				{
					trigger->overrideSet |= ITO_EVENT;
					trigger->event = strtoul(s_infArg0, &endPtr, 10);
				} break;
				case KW_SEQEND:
				{
					// The sequence for this item has completed (no more classes).
					seqEnd = true;
				} break;
			}
		} // while (!seqEnd)
		return true;
	}

	bool parseLineTrigger(TFE_Parser& parser, size_t& bufferPos, s32 argCount, const char* itemName, s32 wallNum, Editor_InfItem* item)
	{
		EditorSector* itemSector = findSector(itemName);
		if (!itemSector)
		{
			parser.readLine(bufferPos);
			return false;
		}
		if (wallNum < 0 || wallNum >= itemSector->walls.size())
		{
			parser.readLine(bufferPos);
			return false;
		}

		Editor_InfTrigger* trigger = allocTrigger(item);
		trigger->overrideSet = ITO_NONE;

		KEYWORD typeId = getKeywordIndex(s_infArg0);
		assert(typeId == KW_TRIGGER);

		trigger->type = ITRIGGER_WALL;
		if (argCount > 2)
		{
			KEYWORD subTypeId = getKeywordIndex(s_infArg1);
			switch (subTypeId)
			{
				case KW_SWITCH1:
				{
					trigger->type = ITRIGGER_SWITCH1;
				} break;
				case KW_TOGGLE:
				{
					trigger->type = ITRIGGER_TOGGLE;
				} break;
				case KW_SINGLE:
				{
					trigger->type = ITRIGGER_SINGLE;
				} break;
				case KW_STANDARD:
				default:
				{
					trigger->type = ITRIGGER_WALL;
				}
			}
		}

		// Trigger parameters
		const char* line;
		char* endPtr = nullptr;
		bool seqEnd = false;
		while (!seqEnd)
		{
			line = parser.readLine(bufferPos);
			if (!line || strstr(line, "CLASS"))
			{
				break;
			}

			char id[256];
			argCount = sscanf(line, " %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
			KEYWORD itemId = getKeywordIndex(id);
			if (itemId == KW_UNKNOWN)
			{
				LE_WARNING("Unknown trigger parameter - '%s'.", id);
			}
			switch (itemId)
			{
				case KW_SEQEND:
				{
					seqEnd = true;
				} break;
				case KW_CLIENT:
				{
					Editor_InfClient client = {};
					parseTarget(&client, s_infArg0);
					if (argCount > 2)
					{
						client.eventMask = strtoul(s_infArg1, &endPtr, 10);
					}
					trigger->clients.push_back(client);
				} break;
				case KW_EVENT_MASK:
				{
					trigger->overrideSet |= ITO_EVENT_MASK;
					if (s_infArg0[0] == '*')
					{
						trigger->eventMask = INF_EVENT_ANY;
					}
					else
					{
						trigger->eventMask = strtoul(s_infArg0, &endPtr, 10);
					}
				} break;
				case KW_MASTER:
				{
					trigger->master = false;
					trigger->overrideSet |= ITO_MASTER;
				} break;
				case KW_TEXT:
				{
					if (s_infArg0[0] >= '0' && s_infArg0[0] <= '9')
					{
						trigger->textId = strtoul(s_infArg0, &endPtr, 10);
						trigger->overrideSet |= ITO_TEXT;
					}
				} break;
				case KW_ENTITY_MASK:
				case KW_OBJECT_MASK:
				{
					trigger->overrideSet |= ITO_ENTITY_MASK;
					if (s_infArg0[0] == '*')
					{
						trigger->entityMask = INF_ENTITY_ANY;
					}
					else
					{
						trigger->entityMask = strtoul(s_infArg0, &endPtr, 10);
					}
				} break;
				case KW_EVENT:
				{
					trigger->overrideSet |= ITO_EVENT;
					trigger->event = strtoul(s_infArg0, &endPtr, 10);
				} break;
				case KW_SOUND_COLON:
				{
					// Not ascii
					if (s_infArg0[0] < '0' || s_infArg0[0] > '9')
					{
						trigger->overrideSet |= ITO_SOUND;
						trigger->sound = s_infArg0;
					}
				} break;
				case KW_MESSAGE:
				{
					trigger->overrideSet |= ITO_MSG;
					editor_parseMessage(&trigger->cmd, &trigger->arg[0], &trigger->arg[1], nullptr, s_infArg0, s_infArg1, s_infArg2, false);
				} break;
			}  // switch (itemId)
		}  // while (!seqEnd)

		return true;
	}

	bool parseTeleport(TFE_Parser& parser, size_t& bufferPos, const char* itemName, Editor_InfItem* item)
	{
		EditorSector* itemSector = findSector(itemName);
		if (!itemSector)
		{
			parser.readLine(bufferPos);
			return false;
		}

		Editor_InfTeleporter* teleporter = allocTeleporter(item);
		KEYWORD itemSubclass = getKeywordIndex(s_infArg1);
		// Special classes - these map to the 11 core elevator types but have special defaults and/or automatically setup stops.
		if (itemSubclass == KW_BASIC)
		{
			teleporter->type = TELEPORT_BASIC;
		}
		else if (itemSubclass == KW_CHUTE)
		{
			teleporter->type = TELEPORT_CHUTE;
		}
		else
		{
			// Invalid type.
			return false;
		}

		// Loop through trigger parameters.
		const char* line;
		bool seqEnd = false;
		while (!seqEnd)
		{
			line = parser.readLine(bufferPos);
			// There is another class in this sequence, so we are done with the trigger.
			if (!line || strstr(line, "CLASS"))
			{
				break;
			}

			char name[256];
			sscanf(line, " %s %s %s %s %s", name, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
			KEYWORD kw = getKeywordIndex(name);

			if (kw == KW_TARGET)  // Target:
			{
				teleporter->target = s_infArg0;
			}
			else if (kw == KW_MOVE)  // Move:
			{
				char* endPtr;
				teleporter->dstPos.x = strtof(s_infArg0, &endPtr);
				teleporter->dstPos.y = strtof(s_infArg1, &endPtr);
				teleporter->dstPos.z = strtof(s_infArg2, &endPtr);
				teleporter->dstAngle = strtof(s_infArg3, &endPtr);
			}
			else if (kw == KW_SEQEND)
			{
				seqEnd = true;
				break;
			}
		}
		return true;
	}

	bool loadLevelInfFromAsset(Asset* asset)
	{
		char infFile[TFE_MAX_PATH];
		s_fileData.clear();
		if (asset->archive)
		{
			FileUtil::replaceExtension(asset->name.c_str(), "INF", infFile);

			if (asset->archive->openFile(infFile))
			{
				const size_t len = asset->archive->getFileLength();
				s_fileData.resize(len);
				asset->archive->readFile(s_fileData.data(), len);
				asset->archive->closeFile();
			}
		}
		else
		{
			FileUtil::replaceExtension(asset->filePath.c_str(), "INF", infFile);

			FileStream file;
			if (file.open(infFile, Stream::MODE_READ))
			{
				const size_t len = file.getSize();
				s_fileData.resize(len);
				file.readBuffer(s_fileData.data(), (u32)len);
				file.close();
			}
		}

		if (s_fileData.empty()) { return false; }
		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init((char*)s_fileData.data(), s_fileData.size());
		parser.enableBlockComments();
		parser.addCommentString("//");
		parser.convertToUpperCase(true);

		const char* line;
		line = parser.readLine(bufferPos);

		// Keep looping until the version is found.
		while (strncasecmp(line, "INF", 3) != 0 && line)
		{
			line = parser.readLine(bufferPos);
		}
		if (!line)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot find INF version.");
			return false;
		}

		f32 version;
		if (sscanf(line, "INF %f", &version) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot read INF version.");
			return false;
		}
		if (version != 1.0f)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Incorrect INF version %f, should be 1.0.", version);
			return false;
		}

		// Keep looping until ITEMS is found.
		// Note - the editor always ignores the INF limit.
		// TODO: Produce a warning if using the vanilla feature-set.
		s32 itemCount = 0;
		while (1)
		{
			line = parser.readLine(bufferPos);
			if (!line)
			{
				LE_ERROR("Cannot find ITEMS in INF: '%s'.", infFile);
				return false;
			}

			if (sscanf(line, "ITEMS %d", &itemCount) == 1)
			{
				break;
			}
		}
		// Warn about vanilla compatibility.
		if (itemCount > 512)
		{
			LE_WARNING("Too many INF items for vanilla compatibility %d / 512.\nExtra items will be ignored if loading in DOS.", itemCount);
		}

		// Then loop through all of the items and parse their classes.
		s32 wallNum = 0;
		for (s32 i = 0; i < itemCount; i++)
		{
			line = parser.readLine(bufferPos);
			if (!line)
			{
				LE_WARNING("Hit the end of INF '%s' before parsing all items: %d/%d", infFile, i, itemCount);
				return true;
			}

			char item[256], name[256];
			while (sscanf(line, " ITEM: %s NAME: %s NUM: %d", item, name, &wallNum) < 1)
			{
				line = parser.readLine(bufferPos);
				if (!line)
				{
					LE_WARNING("Hit the end of INF '%s' before parsing all items: %d/%d", infFile, i, itemCount);
					return true;
				}
				continue;
			}

			s_levelInf.item.push_back({});
			Editor_InfItem* infItem = &s_levelInf.item.back();
			infItem->name = name;
			infItem->wallNum = wallNum;

			KEYWORD itemType = getKeywordIndex(item);
			switch (itemType)
			{
				case KW_LEVEL:
				{
					infItem->wallNum = -1;

					line = parser.readLine(bufferPos);
					if (line && strstr(line, "SEQ"))
					{
						while (line = parser.readLine(bufferPos))
						{
							char itemName[256];
							s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", itemName, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArgExtra, s_infArgExtra);
							KEYWORD levelItem = getKeywordIndex(itemName);
							switch (levelItem)
							{
								case KW_SEQEND:
									break;
								case KW_AMB_SOUND:
								{
									//level_addSound(s_infArg0, u32(atof(s_infArg1) * 145.65f), atoi(s_infArg2));
									continue;
								}
							}
							break;
						}
					}
				} break;
				case KW_SECTOR:
				{
					infItem->wallNum = -1;

					line = parser.readLine(bufferPos);
					if (!line || !strstr(line, "SEQ"))
					{
						continue;
					}

					line = parser.readLine(bufferPos);
					// Loop until seqend since an INF item may have multiple classes.
					while (1)
					{
						if (!line || !strstr(line, "CLASS") || strstr(line, "SEQEND"))
						{
							break;
						}

						char id[256];
						s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArg4, s_infArgExtra);
						KEYWORD itemClass = getKeywordIndex(s_infArg0);
						assert(itemClass != KW_UNKNOWN);

						if (itemClass == KW_ELEVATOR)
						{
							if (parseElevator(parser, bufferPos, name, infItem))
							{
								break;
							}
						}
						else if (itemClass == KW_TRIGGER)
						{
							if (parseSectorTrigger(parser, bufferPos, argCount, name, infItem))
							{
								break;
							}
						}
						else if (itemClass == KW_TELEPORTER)
						{
							if (parseTeleport(parser, bufferPos, name, infItem))
							{
								break;
							}
						}
						else
						{
							// Invalid item class.
							line = parser.readLine(bufferPos);
						}
					}
				} break;
				case KW_LINE:
				{
					line = parser.readLine(bufferPos);
					if (!line || !strstr(line, "SEQ"))
					{
						continue;
					}

					line = parser.readLine(bufferPos);
					// Loop until seqend since an INF item may have multiple classes.
					while (1)
					{
						if (!line || !strstr(line, "CLASS"))
						{
							break;
						}

						char id[256];
						s32 argCount = sscanf(line, " %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
						if (parseLineTrigger(parser, bufferPos, argCount, name, wallNum, infItem))
						{
							break;
						}
					}  // while (!seqEnd) - outer (Line Classes).
				} break;
			}
		}

		return true;
	}

	enum InfEditorMode
	{
		INF_MODE_UI = 0,
		INF_MODE_CODE,
		INF_MODE_COUNT
	};

	struct InfEditor
	{
		InfEditorMode mode = INF_MODE_UI;
		EditorSector* sector = nullptr;
		Editor_InfItem* item = nullptr;
		s32 itemWallIndex = -1;

		s32 comboClassIndex    =  0;
		s32 comboElevTypeIndex =  0;
		s32 comboElevVarIndex  =  0;
		s32 comboElevAddContentIndex = 0;
		s32 comboElevCmdIndex = 0;
		s32 curClassIndex      = -1;
		s32 curPropIndex       = -1;
		s32 curContentIndex    = -1;
		s32 curStopCmdIndex    = -1;
	};
	static InfEditor s_infEditor = {};

	void editor_infEditBegin(const char* sectorName, s32 wallIndex)
	{
		s_infEditor = {};
		if (!sectorName || strlen(sectorName) < 1)
		{
			return;
		}
		s_infEditor.itemWallIndex = wallIndex;

		const s32 count = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < count; i++, sector++)
		{
			if (sector->name.empty()) { continue; }
			if (strcasecmp(sectorName, sector->name.c_str()) == 0)
			{
				s_infEditor.sector = sector;
				break;
			}
		}

		const s32 itemCount = (s32)s_levelInf.item.size();
		Editor_InfItem* item = s_levelInf.item.data();
		for (s32 i = 0; i < itemCount; i++, item++)
		{
			if (strcasecmp(sectorName, item->name.c_str()) == 0 && wallIndex == item->wallNum)
			{
				s_infEditor.item = item;
				break;
			}
		}
	}

	const char* c_infElevFlagNames[] =
	{
		"Move Floor",         // INF_EFLAG_MOVE_FLOOR = FLAG_BIT(0),	// Move on floor.
		"Move Second Height", //INF_EFLAG_MOVE_SECHT = FLAG_BIT(1),	// Move on second height.
		"Move Ceiling",       //INF_EFLAG_MOVE_CEIL = FLAG_BIT(2),	// Move on ceiling (head has to be close enough?)
	};

	const char* c_infClassName[]=
	{
		"Elevator",
		"Trigger",
		"Teleporter"
	};

	const char* c_infElevVarName[] =
	{
		"Start",		//IEV_START
		"Speed",		//IEV_SPEED
		"Master",		//IEV_MASTER
		"Angle",		//IEV_ANGLE
		"Flags",		//IEV_FLAGS
		"Key",			//IEV_KEY0
		"Key1",	        //IEV_KEY1
		"Center",		//IEV_DIR
		"Sound 1",		//IEV_SOUND0
		"Sound 2",		//IEV_SOUND1
		"Sound 3",		//IEV_SOUND2
		"Event_Mask",	//IEV_EVENT_MASK
		"Entity_Mask",	//IEV_ENTITY_MASK
	};

	const char* c_infKeyNames[] =
	{
		"Red",
		"Yellow",
		"Blue"
	};

	const char* c_infEventMaskNames[] =
	{
		"Cross Line Front",	//INF_EVENT_CROSS_LINE_FRONT
		"Cross Line Back",	//INF_EVENT_CROSS_LINE_BACK
		"Enter Sector",		//INF_EVENT_ENTER_SECTOR
		"Leave Sector",		//INF_EVENT_LEAVE_SECTOR
		"Nudge Front",		//INF_EVENT_NUDGE_FRONT
		"Nudge Back",		//INF_EVENT_NUDGE_BACK
		"Explosion",		//INF_EVENT_EXPLOSION
		"Unused",			//INF_EVENT_UNUSED1
		"Shoot Line",		//INF_EVENT_SHOOT_LINE
		"Land",				//INF_EVENT_LAND
	};

	const char* c_infEntityMaskNames[] =
	{
		"Enemy",     // INF_ENTITY_ENEMY,
		"Weapon",    // INF_ENTITY_WEAPON,
		"Smart Obj", // INF_ENTITY_SMART_OBJ,
		"Player",    // INF_ENTITY_PLAYER,
	};

	const u32 c_infEntityMaskFlags[] =
	{
		u32(INF_ENTITY_ENEMY),
		u32(INF_ENTITY_WEAPON),
		u32(INF_ENTITY_SMART_OBJ),
		u32(INF_ENTITY_PLAYER),
	};

	const char* c_infStopDelayTypeName[] =
	{
		"Seconds",		// SDELAY_SECONDS = 0,
		"Hold",			// SDELAY_HOLD,
		"Terminate",	// SDELAY_TERMINATE,
		"Complete",		// SDELAY_COMPLETE,
		"Prev (Buggy)", // SDELAY_PREV_VALUE,
	};

	const char* c_editorInfMessageTypeName[] =
	{
		"Next_Stop",	// IMT_NEXT_STOP,
		"Prev_Stop",	// IMT_PREV_STOP,
		"Goto_Stop",	// IMT_GOTO_STOP,
		"Master_On",	// IMT_MASTER_ON,
		"Master_Off",	// IMT_MASTER_OFF,
		"Set_Bits",		// IMT_SET_BITS,
		"Clear_Bits",	// IMT_CLEAR_BITS,
		"Complete",		// IMT_COMPLETE,
		"Lights",		// IMT_LIGHTS,
		"M_Trigger",	// IMT_TRIGGER,
		"Done",			// IMT_DONE,
		"Wakeup",		// IMT_WAKEUP,
	};
		
	const ImVec4 colorKeywordOuterSel = { 0.453f, 0.918f, 1.00f, 1.0f };
	const ImVec4 colorKeywordOuter = { 0.302f, 0.612f, 0.84f, 1.0f };
	const ImVec4 colorKeywordInner = { 0.306f, 0.788f, 0.69f, 1.0f };

	const ImVec4 colorInnerHeaderBase = { 0.98f, 0.49f, 0.26f, 1.0f };
	const ImVec4 colorInnerHeader = { colorInnerHeaderBase.x, colorInnerHeaderBase.y, colorInnerHeaderBase.z, 0.31f };
	const ImVec4 colorInnerHeaderActive = { colorInnerHeaderBase.x, colorInnerHeaderBase.y, colorInnerHeaderBase.z, 0.80f };
	const ImVec4 colorInnerHeaderHovered = { colorInnerHeaderBase.x, colorInnerHeaderBase.y, colorInnerHeaderBase.z, 0.60f };

	u32 countBits(u32 bits)
	{
		u32 count = 0;
		while (bits)
		{
			if (bits & 1) { count++; }
			bits >>= 1;
		}
		return count;
	}

	void editor_infSelectEditClass(s32 id)
	{
		if (id != s_infEditor.curClassIndex)
		{
			s_infEditor.curPropIndex = -1;
			s_infEditor.curContentIndex = -1;
			s_infEditor.curStopCmdIndex = -1;
		}
		s_infEditor.curClassIndex = id;
	}

	void editor_infPropertySelectable(Editor_InfElevatorVar var, s32 classIndex)
	{
		bool sel = classIndex == s_infEditor.curClassIndex ? s_infEditor.curPropIndex == var : false;
		ImGui::PushStyleColor(ImGuiCol_Text, colorKeywordInner);

		char buffer[256];
		sprintf(buffer, "%s:", c_infElevVarName[var]);
		if (ImGui::Selectable(buffer, sel, 0, { 100.0f, 0.0f }))
		{
			editor_infSelectEditClass(classIndex);
			s_infEditor.curPropIndex = sel ? -1 : var;
		}
		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 8.0f);
	}

	f32 computeChildHeight(const Editor_InfClass* data, s32 contentSel, bool curClass, f32* propHeight, f32* contentHeight)
	{
		f32 height = 600.0f;
		switch (data->classId)
		{
			case IIC_ELEVATOR:
			{
				const Editor_InfElevator* elev = getElevFromClassData(data);
				assert(elev);

				*propHeight = 26.0f * countBits(elev->overrideSet & IEO_VAR_MASK) + 16;
				// Expand if flags are selected.
				if (curClass && (s_infEditor.curPropIndex == IEV_FLAGS || s_infEditor.curPropIndex == IEV_ENTITY_MASK)) { *propHeight += 26.0f; }
				else if (curClass && s_infEditor.curPropIndex == IEV_EVENT_MASK) { *propHeight += 26.0f * 3.0f; }

				const s32 stopCount = (s32)elev->stops.size();
				const s32 slaveCount = (s32)elev->slaves.size();
				*contentHeight = 26.0f * f32(stopCount) + 16;
				// Now count the number of stop messages, etc.
				const Editor_InfStop* stop = elev->stops.data();
				for (s32 s = 0; s < stopCount; s++, stop++)
				{
					*contentHeight += 26.0f * f32(stop->msg.size());
					*contentHeight += 26.0f * f32(stop->adjoinCmd.size());
					*contentHeight += 26.0f * f32(stop->textureCmd.size());
					*contentHeight += (stop->overrideSet & ISO_PAGE) ? 26.0f : 0.0f;
				}
				*contentHeight += 26.0f * f32(slaveCount);
				if (contentSel >= 0 && contentSel < (s32)elev->stops.size() && curClass)
				{
					// Room for buttons.
					*contentHeight += 26.0f;
				}

				height = 140.0f + (*propHeight) + (*contentHeight);
			} break;
			case IIC_TRIGGER:
			{
			} break;
			case IIC_TELEPORTER:
			{
			} break;
		}
		return height;
	}

	void editor_infSelectElevType(Editor_InfElevator* elev)
	{
		ImGui::SetNextItemWidth(180.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
		if (ImGui::BeginCombo(editor_getUniqueLabel(""), c_infElevTypeName[elev->type]))
		{
			s32 count = (s32)TFE_ARRAYSIZE(c_infElevTypeName);
			for (s32 t = 0; t < count; t++)
			{
				if (ImGui::Selectable(c_infElevTypeName[t], t == s_infEditor.comboElevTypeIndex))
				{
					elev->type = Editor_InfElevType(t);
				}
				//setTooltip(c_infClassName[i].tooltip.c_str());
			}
			ImGui::EndCombo();
		}
	}

	void editor_infEditElevProperties(Editor_InfElevator* elev, f32 propHeight, s32 itemClassIndex, const f32* btnTint)
	{
		ImGui::Text("%s", "Properties");
		if (elev->overrideSet & IEO_VAR_MASK)
		{
			ImGui::BeginChild(editor_getUniqueLabel(""), { 0.0f, propHeight }, true);
			{
				const u32 overrides = elev->overrideSet;
				if (overrides & IEO_START)
				{
					editor_infPropertySelectable(IEV_START, itemClassIndex);

					ImGui::SetNextItemWidth(128.0f);
					ImGui::InputInt(editor_getUniqueLabel(""), &elev->start);
				}
				if (overrides & IEO_SPEED)
				{
					editor_infPropertySelectable(IEV_SPEED, itemClassIndex);

					ImGui::SetNextItemWidth(128.0f);
					ImGui::InputFloat(editor_getUniqueLabel(""), &elev->speed, 0.1f, 1.0f, 3);
				}
				if (overrides & IEO_MASTER)
				{
					editor_infPropertySelectable(IEV_MASTER, itemClassIndex);
					ImGui::Checkbox(editor_getUniqueLabel(""), &elev->master);
				}
				if (overrides & IEO_ANGLE)
				{
					editor_infPropertySelectable(IEV_ANGLE, itemClassIndex);

					ImGui::SetNextItemWidth(128.0f);
					ImGui::SliderAngle(editor_getUniqueLabel(""), &elev->angle);
					ImGui::SameLine(0.0f, 8.0f);
					ImGui::SetNextItemWidth(128.0f);
					f32 angle = elev->angle * 180.0f / PI;
					if (ImGui::InputFloat(editor_getUniqueLabel(""), &angle, 0.1f, 1.0f, 3))
					{
						elev->angle = angle * PI / 180.0f;
					}

					ImGui::SameLine(0.0f, 8.0f);
					if (iconButtonInline(ICON_SELECT, "Select points to form the angle from the viewport.", btnTint, true))
					{
						// TODO
					}
				}
				if (overrides & IEO_FLAGS)
				{
					editor_infPropertySelectable(IEV_FLAGS, itemClassIndex);

					ImGui::SetNextItemWidth(128.0f);
					ImGui::InputUInt(editor_getUniqueLabel(""), &elev->flags);
					if (s_infEditor.curPropIndex == IEV_FLAGS)
					{
						for (s32 i = 0; i < TFE_ARRAYSIZE(c_infElevFlagNames); i++)
						{
							if (i != 0) { ImGui::SameLine(0.0f, 8.0f); }
							ImGui::CheckboxFlags(editor_getUniqueLabel(c_infElevFlagNames[i]), &elev->flags, 1 << i);
						}
					}
				}
				if (overrides & IEO_KEY0)
				{
					editor_infPropertySelectable(IEV_KEY0, itemClassIndex);

					s32 keyIndex = elev->key[0] - KeyItem::KEY_RED;
					ImGui::SetNextItemWidth(128.0f);
					if (ImGui::Combo(editor_getUniqueLabel(""), &keyIndex, c_infKeyNames, TFE_ARRAYSIZE(c_infKeyNames)))
					{
						elev->key[0] = KeyItem(keyIndex + KeyItem::KEY_RED);
					}

				}
				if (overrides & IEO_KEY1)
				{
					editor_infPropertySelectable(IEV_KEY1, itemClassIndex);

					s32 keyIndex = elev->key[1] - KeyItem::KEY_RED;
					ImGui::SetNextItemWidth(128.0f);
					if (ImGui::Combo(editor_getUniqueLabel(""), &keyIndex, c_infKeyNames, TFE_ARRAYSIZE(c_infKeyNames)))
					{
						elev->key[1] = KeyItem(keyIndex + KeyItem::KEY_RED);
					}
				}
				if (overrides & IEO_DIR)
				{
					editor_infPropertySelectable(IEV_DIR, itemClassIndex);

					ImGui::SetNextItemWidth(160.0f);
					ImGui::InputFloat2(editor_getUniqueLabel(""), elev->dirOrCenter.m, 3);

					ImGui::SameLine(0.0f, 8.0f);
					if (iconButtonInline(ICON_SELECT, "Select position in viewport.", btnTint, true))
					{
						// TODO
					}
					ImGui::SameLine(0.0f, 8.0f);
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
					if (iconButtonInline(ICON_BOX_CENTER, "Select sector in viewport and use its center.", btnTint, true))
					{
						// TODO
					}
					ImGui::SameLine(0.0f, 8.0f);
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
					if (iconButtonInline(ICON_CIRCLE_PLUS, "Calculate the center from current sector and slaves.", btnTint, true))
					{
						// TODO
					}
				}
				if (overrides & IEO_SOUND0)
				{
					editor_infPropertySelectable(IEV_SOUND0, itemClassIndex);

					char soundBuffer[256];
					strcpy(soundBuffer, elev->sounds[0].c_str());

					ImGui::SetNextItemWidth(160.0f);
					if (ImGui::InputText(editor_getUniqueLabel(""), soundBuffer, 256))
					{
						elev->sounds[0] = soundBuffer;
					}

					ImGui::SameLine(0.0f, 8.0f);
					if (editor_button("Browse"))
					{
						// TODO
					}
				}
				if (overrides & IEO_SOUND1)
				{
					editor_infPropertySelectable(IEV_SOUND1, itemClassIndex);

					char soundBuffer[256];
					strcpy(soundBuffer, elev->sounds[1].c_str());

					ImGui::SetNextItemWidth(160.0f);
					if (ImGui::InputText(editor_getUniqueLabel(""), soundBuffer, 256))
					{
						elev->sounds[1] = soundBuffer;
					}

					ImGui::SameLine(0.0f, 8.0f);
					if (editor_button("Browse"))
					{
						// TODO
					}
				}
				if (overrides & IEO_SOUND2)
				{
					editor_infPropertySelectable(IEV_SOUND2, itemClassIndex);

					char soundBuffer[256];
					strcpy(soundBuffer, elev->sounds[2].c_str());

					ImGui::SetNextItemWidth(160.0f);
					if (ImGui::InputText(editor_getUniqueLabel(""), soundBuffer, 256))
					{
						elev->sounds[2] = soundBuffer;
					}

					ImGui::SameLine(0.0f, 8.0f);
					if (editor_button("Browse"))
					{
						// TODO
					}
				}
				if (overrides & IEO_EVENT_MASK)
				{
					editor_infPropertySelectable(IEV_EVENT_MASK, itemClassIndex);

					ImGui::SetNextItemWidth(128.0f);
					ImGui::InputInt(editor_getUniqueLabel(""), &elev->eventMask);

					if (s_infEditor.curPropIndex == IEV_EVENT_MASK)
					{
						for (s32 i = 0; i < TFE_ARRAYSIZE(c_infEventMaskNames); i++)
						{
							if ((i % 4) != 0) { ImGui::SameLine(200.0f * (i % 4), 0.0f); }
							ImGui::CheckboxFlags(editor_getUniqueLabel(c_infEventMaskNames[i]), (u32*)&elev->eventMask, 1 << i);
						}
					}
				}
				if (overrides & IEO_ENTITY_MASK)
				{
					editor_infPropertySelectable(IEV_ENTITY_MASK, itemClassIndex);

					ImGui::SetNextItemWidth(128.0f);
					ImGui::InputInt(editor_getUniqueLabel(""), &elev->entityMask);

					if (s_infEditor.curPropIndex == IEV_ENTITY_MASK)
					{
						for (s32 i = 0; i < TFE_ARRAYSIZE(c_infEntityMaskNames); i++)
						{
							if ((i % 4) != 0) { ImGui::SameLine(200.0f * (i % 4), 0.0f); }
							ImGui::CheckboxFlags(editor_getUniqueLabel(c_infEntityMaskNames[i]), (u32*)&elev->entityMask, c_infEntityMaskFlags[i]);
						}
					}
				}
			}
			ImGui::EndChild();
		}
	}

	void editor_infAddOrRemoveElevProperty(Editor_InfElevator* elev, s32 itemClassIndex)
	{
		if (editor_button("+"))
		{
			if (s_infEditor.comboElevVarIndex >= 0)
			{
				elev->overrideSet |= (1 << s_infEditor.comboElevVarIndex);
				if (s_infEditor.comboElevVarIndex == IEV_KEY0)
				{
					elev->key[0] = KEY_RED;
				}
				else if (s_infEditor.comboElevVarIndex == IEV_KEY1)
				{
					elev->key[1] = KEY_RED;
				}
			}
			editor_infSelectEditClass(itemClassIndex);
		}
		setTooltip("Add a new property.");
		ImGui::SameLine(0.0f, 4.0f);
		if (editor_button("-"))
		{
			if (s_infEditor.curPropIndex >= 0)
			{
				elev->overrideSet &= ~(1 << s_infEditor.curPropIndex);
				if (s_infEditor.comboElevVarIndex == IEV_KEY0)
				{
					elev->key[0] = KEY_NONE;
				}
				else if (s_infEditor.comboElevVarIndex == IEV_KEY1)
				{
					elev->key[1] = KEY_NONE;
				}
			}
			editor_infSelectEditClass(itemClassIndex);
			s_infEditor.curPropIndex = -1;
		}
		setTooltip("Remove the selected property.");

		ImGui::SameLine(0.0f, 16.0f);
		ImGui::SetNextItemWidth(128.0f);
		if (ImGui::BeginCombo(editor_getUniqueLabel(""), c_infElevVarName[s_infEditor.comboElevVarIndex]))
		{
			s32 count = (s32)TFE_ARRAYSIZE(c_infElevVarName);
			for (s32 i = 0; i < count; i++)
			{
				if (ImGui::Selectable(c_infElevVarName[i], i == s_infEditor.comboElevVarIndex))
				{
					s_infEditor.comboElevVarIndex = i;
				}
				//setTooltip(c_infClassName[i].tooltip.c_str());
			}
			ImGui::EndCombo();
			editor_infSelectEditClass(itemClassIndex);
		}
		setTooltip("Property to add.");
	}
		
	void editor_infAddOrRemoveStopCmd(Editor_InfElevator* elev, Editor_InfStop* stop, s32 itemClassIndex)
	{
		ImGui::Text("    "); ImGui::SameLine(0.0f, 0.0f);
		if (editor_button("+"))
		{
			switch (s_infEditor.comboElevCmdIndex)
			{
				case ISC_MESSAGE:
				{
					stop->msg.push_back({});
				} break;
				case ISC_ADJOIN:
				{
					stop->adjoinCmd.push_back({});
				} break;
				case ISC_TEXTURE:
				{
					stop->textureCmd.push_back({});
				} break;
				case ISC_PAGE:
				{
					stop->page = {};
					stop->overrideSet |= ISO_PAGE;
				} break;
			}
		}
		setTooltip("Add a new command to the selected stop.");
		ImGui::SameLine(0.0f, 4.0f);
		if (editor_button("-") && s_infEditor.curStopCmdIndex >= 0)
		{
			const s32 msgCount    = (s32)stop->msg.size();
			const s32 adjoinCount = (s32)stop->adjoinCmd.size();
			const s32 texCount    = (s32)stop->textureCmd.size();

			s32 index = -1;
			s32 cmdIndexOffset = 0;
			if (index < 0 && s_infEditor.curStopCmdIndex < msgCount + cmdIndexOffset)
			{
				index = s_infEditor.curStopCmdIndex - cmdIndexOffset;
				for (s32 i = index; i < msgCount - 1; i++)
				{
					stop->msg[i] = stop->msg[i + 1];
				}
				stop->msg.pop_back();
				s_infEditor.curStopCmdIndex = -1;
			}
			cmdIndexOffset += msgCount;

			if (index < 0 && s_infEditor.curStopCmdIndex < adjoinCount + cmdIndexOffset)
			{
				index = s_infEditor.curStopCmdIndex - cmdIndexOffset;
				for (s32 i = index; i < adjoinCount - 1; i++)
				{
					stop->adjoinCmd[i] = stop->adjoinCmd[i + 1];
				}
				stop->adjoinCmd.pop_back();
				s_infEditor.curStopCmdIndex = -1;
			}
			cmdIndexOffset += (s32)stop->adjoinCmd.size();

			if (index < 0 && s_infEditor.curStopCmdIndex < (s32)stop->textureCmd.size() + cmdIndexOffset)
			{
				index = s_infEditor.curStopCmdIndex - cmdIndexOffset;
				for (s32 i = index; i < texCount - 1; i++)
				{
					stop->textureCmd[i] = stop->textureCmd[i + 1];
				}
				stop->textureCmd.pop_back();
				s_infEditor.curStopCmdIndex = -1;
			}
			cmdIndexOffset += (s32)stop->textureCmd.size();

			if (index < 0 && s_infEditor.curStopCmdIndex <= cmdIndexOffset)
			{
				stop->page = {};
				stop->overrideSet &= ~ISO_PAGE;
				index = 0;
			}
		}
		setTooltip("Remove the selected command from the stop.");
		ImGui::SameLine(0.0f, 16.0f);

		ImGui::SetNextItemWidth(128.0f);
		ImGui::Combo(editor_getUniqueLabel(""), &s_infEditor.comboElevCmdIndex, c_elevStopCmdName, TFE_ARRAYSIZE(c_elevStopCmdName));
		setTooltip("Type of stop command to add.");
	}

	void editor_infAddOrRemoveElevStopOrSlave(Editor_InfElevator* elev, s32 itemClassIndex)
	{
		if (editor_button("+"))
		{
			if (s_infEditor.comboElevAddContentIndex == 0)
			{
				// insert a stop after the selected stop.
				Editor_InfStop newStop = {};
				if (s_infEditor.curContentIndex >= 0 && s_infEditor.curContentIndex < elev->stops.size())
				{
					newStop.value = elev->stops[s_infEditor.curContentIndex].value + 1.0f;
					elev->stops.insert(elev->stops.begin() + s_infEditor.curContentIndex + 1, newStop);
				}
				else
				{
					elev->stops.push_back(newStop);
				}
			}
			else if (s_infEditor.comboElevAddContentIndex == 1)
			{
				// insert a slave.
				Editor_InfSlave newSlave = {};
				const s32 stopCount = (s32)elev->stops.size();
				const s32 slaveCount = (s32)elev->slaves.size();
				if (s_infEditor.curContentIndex >= stopCount && s_infEditor.curContentIndex < stopCount + slaveCount)
				{
					elev->slaves.insert(elev->slaves.begin() + s_infEditor.curContentIndex - stopCount + 1, newSlave);
				}
				else
				{
					elev->slaves.push_back(newSlave);
				}
			}
			editor_infSelectEditClass(itemClassIndex);
		}
		setTooltip("Add a new stop or slave.");
		ImGui::SameLine(0.0f, 4.0f);
		if (editor_button("-"))
		{
			const s32 stopCount = (s32)elev->stops.size();
			const s32 slaveCount = (s32)elev->slaves.size();
			if (s_infEditor.curContentIndex >= 0 && s_infEditor.curContentIndex < stopCount)
			{
				for (s32 s = s_infEditor.curContentIndex; s < stopCount - 1; s++)
				{
					elev->stops[s] = elev->stops[s + 1];
				}
				elev->stops.pop_back();
				s_infEditor.curContentIndex = -1;
				s_infEditor.curStopCmdIndex = -1;
			}
			else if (s_infEditor.curContentIndex >= stopCount && s_infEditor.curContentIndex < stopCount + slaveCount)
			{
				for (s32 s = s_infEditor.curContentIndex - stopCount; s < slaveCount - 1; s++)
				{
					elev->slaves[s] = elev->slaves[s + 1];
				}
				elev->slaves.pop_back();
				s_infEditor.curContentIndex = -1;
				s_infEditor.curStopCmdIndex = -1;
			}
			editor_infSelectEditClass(itemClassIndex);
		}
		setTooltip("Remove the selected elevator item.");
		ImGui::SameLine(0.0f, 16.0f);

		const char* c_elevAddTypes[] = { "Stop", "Slave" };
		ImGui::SetNextItemWidth(128.0f);
		ImGui::Combo(editor_getUniqueLabel(""), &s_infEditor.comboElevAddContentIndex, c_elevAddTypes, TFE_ARRAYSIZE(c_elevAddTypes));
		setTooltip("Type of elevator item to add - Stop or Slave.");
	}

	void editor_stopCmdSelectable(Editor_InfElevator* elev, Editor_InfStop* stop, s32 itemClassIndex, s32 cmdIndex, const char* label)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, colorKeywordInner);
		ImGui::PushStyleColor(ImGuiCol_Header, colorInnerHeader);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, colorInnerHeaderActive);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colorInnerHeaderHovered);
		const s32 stopId = TFE_ARRAYPOS(stop, elev->stops.data());
		bool sel = s_infEditor.curClassIndex == itemClassIndex && s_infEditor.curContentIndex == stopId && s_infEditor.curStopCmdIndex == cmdIndex;
		ImGui::Text("    "); ImGui::SameLine(0.0f, 0.0f);
		if (ImGui::Selectable(editor_getUniqueLabel(label), sel, 0, { 80.0f, 0.0f }))
		{
			editor_infSelectEditClass(itemClassIndex);
			s_infEditor.curStopCmdIndex = sel ? -1 : cmdIndex;
			s_infEditor.curContentIndex = stopId;
		}
		ImGui::PopStyleColor(4);
		ImGui::SameLine(0.0f, 8.0f);
	}

	void editor_infEditElevStops(Editor_InfElevator* elev, f32 contentHeight, s32 itemClassIndex, const f32* btnTint)
	{
		char buffer[256];
		const s32 stopCount = (s32)elev->stops.size();
		if (!stopCount) { return; }
		Editor_InfStop* stop = elev->stops.data();
		for (s32 s = 0; s < stopCount; s++, stop++)
		{
			///////////////////////////////
			// Stop
			///////////////////////////////
			sprintf(buffer, "Stop: %d", s);
			ImGui::PushStyleColor(ImGuiCol_Text, colorKeywordOuter);
			bool sel = s_infEditor.curClassIndex == itemClassIndex && s_infEditor.curContentIndex == s;
			if (ImGui::Selectable(editor_getUniqueLabel(buffer), sel, 0, { 80.0f, 0.0f }))
			{
				editor_infSelectEditClass(itemClassIndex);
				s_infEditor.curContentIndex = sel ? -1 : s;
				s_infEditor.curStopCmdIndex = -1;
				// Update select.
				sel = s_infEditor.curClassIndex == itemClassIndex && s_infEditor.curContentIndex == s;
			}
			ImGui::PopStyleColor();
			ImGui::SameLine(0.0f, 16.0f);
			ImGui::Text("Value:");
			ImGui::SameLine(0.0f, 8.0f);

			ImGui::SetNextItemWidth(128.0f);
			if (stop->fromSectorFloor.empty())
			{
				// TODO: How to turn into a string?
				ImGui::InputFloat(editor_getUniqueLabel(""), &stop->value, 0.1f, 1.0f, 3);
			}
			else
			{
				// TODO: How to turn into a number?
				char nameBuffer[256];
				strcpy(nameBuffer, stop->fromSectorFloor.c_str());
				if (ImGui::InputText(editor_getUniqueLabel(""), nameBuffer, 256))
				{
					stop->fromSectorFloor = nameBuffer;
				}
			}
			ImGui::SameLine(0.0f, 16.0f);

			ImGui::Text("Relative:");
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::Checkbox(editor_getUniqueLabel(""), &stop->relative);
			ImGui::SameLine(0.0f, 16.0f);

			ImGui::Text("Delay Type:");
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::SetNextItemWidth(100.0f);
			if (ImGui::BeginCombo(editor_getUniqueLabel(""), c_infStopDelayTypeName[stop->delayType]))
			{
				s32 count = (s32)TFE_ARRAYSIZE(c_infStopDelayTypeName);
				for (s32 c = 0; c < count; c++)
				{
					if (ImGui::Selectable(editor_getUniqueLabel(c_infStopDelayTypeName[c]), c == stop->delayType))
					{
						stop->delayType = Editor_InfStopDelayType(c);
					}
					//setTooltip(c_infClassName[i].tooltip.c_str());
				}
				ImGui::EndCombo();
			}

			// Only show the delay time if needed.
			if (stop->delayType == SDELAY_SECONDS)
			{
				ImGui::SameLine(0.0f, 16.0f);
				ImGui::Text("Delay:");
				ImGui::SameLine(0.0f, 8.0f);
				ImGui::SetNextItemWidth(128.0f);
				ImGui::InputFloat(editor_getUniqueLabel(""), &stop->delay, 0.1f, 1.0f, 3);
			}

			///////////////////////////////
			// Commands.
			///////////////////////////////
			s32 cmdIndexOffset = 0;
			const s32 msgCount = (s32)stop->msg.size();
			Editor_InfMessage* msg = stop->msg.data();
			char targetBuffer[256];
			for (s32 m = 0; m < msgCount; m++, msg++)
			{
				editor_stopCmdSelectable(elev, stop, itemClassIndex, m + cmdIndexOffset, "Message:");

				ImGui::Text("Target"); ImGui::SameLine(0.0f, 8.0f);
				if (msg->targetWall >= 0) { sprintf(targetBuffer, "%s(%d)", msg->targetSector.c_str(), msg->targetWall); }
				else { strcpy(targetBuffer, msg->targetSector.c_str()); }
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::InputText(editor_getUniqueLabel(""), targetBuffer, 256))
				{
					parseTarget(msg, targetBuffer);
				}

				ImGui::SameLine(0.0f, 8.0f);
				if (iconButtonInline(ICON_SELECT, "Select target sector or wall in the viewport.", btnTint, true))
				{
					// TODO
				}

				ImGui::SameLine(0.0f, 16.0f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
				ImGui::Text("Type"); ImGui::SameLine(0.0f, 8.0f);
				ImGui::SetNextItemWidth(128.0f);
				ImGui::Combo(editor_getUniqueLabel(""), (s32*)&msg->type, c_editorInfMessageTypeName, IMT_COUNT);

				switch (msg->type)
				{
					// 0-args
					case IMT_NEXT_STOP:
					case IMT_PREV_STOP:
					case IMT_MASTER_ON:
					case IMT_MASTER_OFF:
					case IMT_LIGHTS:
					case IMT_TRIGGER:
					case IMT_DONE:
					case IMT_WAKEUP:
					{
						// Nothing
					} break;
					// 1-arg
					case IMT_GOTO_STOP:
					case IMT_COMPLETE:
					{
						ImGui::SameLine(0.0f, 16.0f);
						ImGui::Text(msg->type == IMT_GOTO_STOP ? "Stop ID" : "Goal ID"); ImGui::SameLine(0.0f, 8.0f);

						ImGui::SetNextItemWidth(128.0f);
						ImGui::InputUInt(editor_getUniqueLabel(""), &msg->arg[0]);
					} break;
					// 2-args
					case IMT_SET_BITS:
					case IMT_CLEAR_BITS:
					{
						ImGui::SameLine(0.0f, 16.0f);

						ImGui::Text("Flags"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::SetNextItemWidth(48.0f);
						s32 flag = min(2, max(0, (s32)msg->arg[0] - 1));
						const char* flagNames[] = { "1", "2", "3" };
						if (ImGui::Combo(editor_getUniqueLabel(""), &flag, flagNames, TFE_ARRAYSIZE(flagNames)))
						{
							msg->arg[0] = flag + 1;
						}

						ImGui::SameLine(0.0f, 16.0f);
						ImGui::Text("Bit"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::SetNextItemWidth(128.0f);
						ImGui::InputUInt(editor_getUniqueLabel(""), &msg->arg[1]);

						// TODO: Handle showing checkboxes if selected.
					} break;
				}
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
			}
			cmdIndexOffset += msgCount;

			const s32 adjoinCount = (s32)stop->adjoinCmd.size();
			Editor_InfAdjoinCmd* cmd = stop->adjoinCmd.data();
			for (s32 c = 0; c < adjoinCount; c++, cmd++)
			{
				editor_stopCmdSelectable(elev, stop, itemClassIndex, c + cmdIndexOffset, "Adjoin:");

				ImGui::Text("Sector 1"); ImGui::SameLine(0.0f, 8.0f);
				strcpy(targetBuffer, cmd->sector0.c_str());
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::InputText(editor_getUniqueLabel(""), targetBuffer, 256))
				{
					cmd->sector0 = targetBuffer;
				}
				ImGui::SameLine(0.0f, 8.0f);
				ImGui::SetNextItemWidth(80.0f);
				ImGui::InputInt(editor_getUniqueLabel(""), &cmd->wallIndex0);
				ImGui::SameLine(0.0f, 8.0f);
				if (iconButtonInline(ICON_SELECT, "Select wall to adjoin in the viewport.", btnTint, true))
				{
					// TODO
				}

				ImGui::SameLine(0.0f, 16.0f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

				ImGui::Text("Sector 2"); ImGui::SameLine(0.0f, 8.0f);
				strcpy(targetBuffer, cmd->sector1.c_str());
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::InputText(editor_getUniqueLabel(""), targetBuffer, 256))
				{
					cmd->sector1 = targetBuffer;
				}
				ImGui::SameLine(0.0f, 8.0f);
				ImGui::SetNextItemWidth(80.0f);
				ImGui::InputInt(editor_getUniqueLabel(""), &cmd->wallIndex1);
				ImGui::SameLine(0.0f, 8.0f);
				if (iconButtonInline(ICON_SELECT, "Select wall to adjoin in the viewport.", btnTint, true))
				{
					// TODO
				}
			}
			cmdIndexOffset += adjoinCount;

			const s32 texCount = (s32)stop->textureCmd.size();
			Editor_InfTextureCmd* texCmd = stop->textureCmd.data();
			for (s32 t = 0; t < texCount; t++, texCmd++)
			{
				editor_stopCmdSelectable(elev, stop, itemClassIndex, t + cmdIndexOffset, "Texture:");

				ImGui::Text("Donor Sector"); ImGui::SameLine(0.0f, 8.0f);
				strcpy(targetBuffer, texCmd->donorSector.c_str());
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::InputText(editor_getUniqueLabel(""), targetBuffer, 256))
				{
					texCmd->donorSector = targetBuffer;
				}
				ImGui::SameLine(0.0f, 8.0f);
				if (iconButtonInline(ICON_SELECT, "Select donor sector in the viewport.", btnTint, true))
				{
					// TODO
				}

				ImGui::SameLine(0.0f, 16.0f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
				ImGui::Text("From Ceiling"); ImGui::SameLine(0.0f, 8.0f);
				ImGui::Checkbox(editor_getUniqueLabel(""), &texCmd->fromCeiling);
			}
			cmdIndexOffset += texCount;

			if (stop->overrideSet & ISO_PAGE)
			{
				editor_stopCmdSelectable(elev, stop, itemClassIndex, cmdIndexOffset, "Page:");

				strcpy(targetBuffer, stop->page.c_str());
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::InputText(editor_getUniqueLabel(""), targetBuffer, 256))
				{
					stop->page = targetBuffer;
				}

				ImGui::SameLine(0.0f, 8.0f);
				if (editor_button("Browse"))
				{
					// TODO
				}
			}

			// Show buttons to add new commands inline.
			if (sel)
			{
				editor_infAddOrRemoveStopCmd(elev, stop, itemClassIndex);
			}
		}
	}

	void editor_infEditElevSlaves(Editor_InfElevator* elev, f32 contentHeight, s32 itemClassIndex, const f32* btnTint)
	{
		const s32 slaveCount = (s32)elev->slaves.size();
		const s32 stopCount = (s32)elev->stops.size();
		if (!slaveCount) { return; }

		Editor_InfSlave* slave = elev->slaves.data();
		char nameBuffer[256];
		for (s32 s = 0; s < slaveCount; s++, slave++)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, colorKeywordOuter);
			bool sel = s_infEditor.curClassIndex == itemClassIndex && s_infEditor.curContentIndex == s + stopCount;
			if (ImGui::Selectable(editor_getUniqueLabel("Slave:"), sel, 0, { 80.0f, 0.0f }))
			{
				editor_infSelectEditClass(itemClassIndex);
				s_infEditor.curContentIndex = sel ? -1 : s + stopCount;
				s_infEditor.curStopCmdIndex = -1;
			}
			ImGui::PopStyleColor();

			ImGui::SameLine(0.0f, 16.0f);
			ImGui::Text("Sector"); ImGui::SameLine(0.0f, 8.0f);
			ImGui::SetNextItemWidth(128.0f);
			strcpy(nameBuffer, slave->name.c_str());
			if (ImGui::InputText(editor_getUniqueLabel(""), nameBuffer, 256))
			{
				slave->name = nameBuffer;
			}
			ImGui::SameLine(0.0f, 8.0f);
			if (iconButtonInline(ICON_SELECT, "Select the slave sector in the viewport.", btnTint, true))
			{
				// TODO
			}
			// Reset the cursor Y position so that controls line up after icon buttons.
			// This must be done after "same line" since it modifies the y position.
			ImGui::SameLine(0.0f, 16.0f);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

			ImGui::Text("Angle Offset"); ImGui::SameLine(0.0f, 8.0f);
			ImGui::SetNextItemWidth(128.0f);
			ImGui::InputFloat(editor_getUniqueLabel(""), &slave->angleOffset);

			// Avoid adding extra spacing due to inline icon buttons.
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
		}
	}

	void editor_infSectorEdit_UI()
	{
		const f32 tint[] = { 103.0f / 255.0f, 122.0f / 255.0f, 139.0f / 255.0f, 1.0f };

		s32 deleteIndex = -1;
		const s32 count = (s32)s_infEditor.item->classData.size();
		Editor_InfClass** dataList = s_infEditor.item->classData.data();
		for (s32 i = 0; i < count; i++)
		{
			Editor_InfClass* data = dataList[i];
			f32 propHeight = 0.0f, contentHeight = 0.0f;
			f32 childHeight = computeChildHeight(data, s_infEditor.curContentIndex, s_infEditor.curClassIndex == i, &propHeight, &contentHeight);

			if (ImGui::BeginChild(editor_getUniqueLabel(""), { 0, childHeight }, true))
			{
				// Class label.
				ImGui::TextColored(i == s_infEditor.curClassIndex ? colorKeywordOuterSel : colorKeywordOuter, "Class:");
				ImGui::SameLine(0.0f, 8.0f);

				switch (data->classId)
				{
					case IIC_ELEVATOR:
					{
						// Class name.
						ImGui::TextColored(colorKeywordInner, "Elevator");
						ImGui::SameLine(0.0f, 8.0f);

						// Class data.
						Editor_InfElevator* elev = getElevFromClassData(data);
						assert(elev);
						// Elevator Type.
						editor_infSelectElevType(elev);

						// Properties.
						editor_infEditElevProperties(elev, propHeight, i, tint);
						editor_infAddOrRemoveElevProperty(elev, i);
						ImGui::Separator();

						// Content - stops and slaves.
						if (!elev->stops.empty() || !elev->slaves.empty())
						{
							const s32 stopCount = (s32)elev->stops.size();
							const s32 slaveCount = (s32)elev->slaves.size();
							if (stopCount && slaveCount) { ImGui::Text("Stops: %d    Slaves: %d", stopCount, slaveCount); }
							else if (stopCount)  { ImGui::Text("Stops: %d", stopCount); }
							else if (slaveCount) { ImGui::Text("Slaves: %d", slaveCount); }
							ImGui::BeginChild(editor_getUniqueLabel(""), { 0.0f, contentHeight }, true);
							{
								// Edit Stops.
								editor_infEditElevStops(elev, contentHeight, i, tint);
								// Edit Slaves.
								editor_infEditElevSlaves(elev, contentHeight, i, tint);
							}
							ImGui::EndChild();
						}
						editor_infAddOrRemoveElevStopOrSlave(elev, i);
					} break;
					case IIC_TRIGGER:
					{
						// Class name.
						ImGui::TextColored(colorKeywordInner, "Trigger");
						ImGui::SameLine(0.0f, 8.0f);

						// Class data.
						Editor_InfTrigger* trigger = getTriggerFromClassData(data);
						assert(trigger);
					} break;
					case IIC_TELEPORTER:
					{
						// Class name.
						ImGui::TextColored(colorKeywordInner, "Teleporter");
						ImGui::SameLine(0.0f, 8.0f);
					} break;
				}
			}
			ImGui::EndChild();
		}

		if (deleteIndex >= 0 && deleteIndex < count)
		{
			switch (s_infEditor.item->classData[deleteIndex]->classId)
			{
				case IIC_ELEVATOR:
				{
					Editor_InfElevator* elev = getElevFromClassData(s_infEditor.item->classData[deleteIndex]);
					assert(elev);
					freeElevator(elev);
				} break;
				case IIC_TRIGGER:
				{
					Editor_InfTrigger* trigger = getTriggerFromClassData(s_infEditor.item->classData[deleteIndex]);
					assert(trigger);
					freeTrigger(trigger);
				} break;
				case IIC_TELEPORTER:
				{
					Editor_InfTeleporter* teleporter = getTeleportFromClassData(s_infEditor.item->classData[deleteIndex]);
					assert(teleporter);
					freeTeleporter(teleporter);
				} break;
			}

			for (s32 i = deleteIndex; i < count - 1; i++)
			{
				s_infEditor.item->classData[i] = s_infEditor.item->classData[i + 1];
			}
			s_infEditor.item->classData.pop_back();
		}
	}

	char s_floatToStrBuffer[256];
	char s_floatToStrBuffer1[256];
	const char* infFloatToString(f32 value, s32 index = 0);

	const char* infFloatToString(f32 value, s32 index)
	{
		char* outStr = index == 0 ? s_floatToStrBuffer : s_floatToStrBuffer1;
		const f32 eps = 0.0001f;
		// Integer value.
		if (fabsf(floorf(value) - value) < eps)
		{
			sprintf(outStr, "%d", s32(value));
		}
		// Is it a single digit?
		else if (fabsf(floorf(value * 10.0f) * 0.1f - value) < eps)
		{
			sprintf(outStr, "%0.1f", value);
		}
		// Two digits.
		else if (fabsf(floorf(value * 100.0f) * 0.01f - value) < eps)
		{
			sprintf(outStr, "%0.2f", value);
		}
		// Three digits.
		else
		{
			sprintf(outStr, "%0.3f", value);
		}
		return outStr;
	}

	void editor_InfSectorEdit_Code()
	{
		const s32 count = (s32)s_infEditor.item->classData.size();
		Editor_InfClass** dataList = s_infEditor.item->classData.data();
		for (s32 i = 0; i < count; i++)
		{
			if (i > 0)
			{
				ImGui::NewLine();
			}

			Editor_InfClass* data = dataList[i];
			switch (data->classId)
			{
				case IIC_ELEVATOR:
				{
					Editor_InfElevator* elev = getElevFromClassData(data);

					// Class
					ImGui::TextColored(colorKeywordOuter, "Class:"); ImGui::SameLine(0.0f, 8.0f);
					ImGui::TextColored(colorKeywordInner, "Elevator"); ImGui::SameLine(0.0f, 8.0f);
					ImGui::Text(c_infElevTypeName[elev->type]);

					const char* tab = "    ";

					// Properties.
					const u32 overrides = elev->overrideSet;
					if (overrides & IEO_START)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Start:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%d", elev->start);
					}
					if (overrides & IEO_SPEED)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Speed:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%s", infFloatToString(elev->speed));
					}
					if ((overrides & IEO_MASTER) && !elev->master)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Master:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("Off");
					}
					if (overrides & IEO_ANGLE)
					{
						f32 angle = elev->angle * 180.0f / PI;
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Angle:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%s", infFloatToString(angle));
					}
					if (overrides & IEO_FLAGS)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Flags:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%d", elev->flags);
					}
					if (overrides & IEO_KEY0)
					{
						if (elev->type == IET_DOOR_MID)
						{
							ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextColored(colorKeywordInner, "Addon:"); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%d", 0);
						}
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Key:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%s", c_infKeyNames[elev->key[0] - KeyItem::KEY_RED]);
					}
					if (overrides & IEO_KEY1)
					{
						if (elev->type == IET_DOOR_MID)
						{
							ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextColored(colorKeywordInner, "Addon:"); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%d", 1);
							ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextColored(colorKeywordInner, "Key:"); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%s", c_infKeyNames[elev->key[1] - KeyItem::KEY_RED]);
						}
					}
					if (overrides & IEO_DIR)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Center:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%s %s", infFloatToString(elev->dirOrCenter.x), infFloatToString(elev->dirOrCenter.z, 1));
					}
					if (overrides & IEO_SOUND0)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Sound:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%d %s", 1, elev->sounds[0].c_str());
					}
					if (overrides & IEO_SOUND1)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Sound:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%d %s", 2, elev->sounds[1].c_str());
					}
					if (overrides & IEO_SOUND2)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Sound:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%d %s", 3, elev->sounds[2].c_str());
					}
					if (overrides & IEO_EVENT_MASK)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Event_Mask:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%d", elev->eventMask);
					}
					if (overrides & IEO_ENTITY_MASK)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordInner, "Entity_Mask:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%d", elev->entityMask);
					}

					const s32 stopCount = (s32)elev->stops.size();
					const Editor_InfStop* stop = elev->stops.data();
					for (s32 s = 0; s < stopCount; s++, stop++)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordOuter, "Stop:"); ImGui::SameLine(0.0f, 8.0f);
						if (!stop->fromSectorFloor.empty())
						{
							ImGui::Text("%s", stop->fromSectorFloor.c_str()); ImGui::SameLine(0.0f, 8.0f);
						}
						else if (stop->relative)
						{
							ImGui::Text("@%s", infFloatToString(stop->value)); ImGui::SameLine(0.0f, 8.0f);
						}
						else
						{
							ImGui::Text("%s", infFloatToString(stop->value)); ImGui::SameLine(0.0f, 8.0f);
						}

						if (stop->delayType == SDELAY_SECONDS)
						{
							ImGui::Text("%s", infFloatToString(stop->delay));
						}
						else if (stop->delayType == SDELAY_HOLD)
						{
							ImGui::Text("%s", "hold");
						}
						else if (stop->delayType == SDELAY_COMPLETE)
						{
							ImGui::Text("%s", "complete");
						}
						else if (stop->delayType == SDELAY_TERMINATE)
						{
							ImGui::Text("%s", "terminate");
						}
						else
						{
							ImGui::Text("%s", "prev");
						}

						// Stop commands.
						const s32 msgCount = (s32)stop->msg.size();
						const Editor_InfMessage* msg = stop->msg.data();
						for (s32 m = 0; m < msgCount; m++, msg++)
						{
							ImGui::Text("%s%s", tab, tab); ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextColored(colorKeywordInner, "Message:"); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%d", s); ImGui::SameLine(0.0f, 8.0f);
							if (msg->targetWall >= 0)
							{
								ImGui::Text("%s(%d)", msg->targetSector.c_str(), msg->targetWall); ImGui::SameLine(0.0f, 8.0f);
							}
							else
							{
								ImGui::Text("%s", msg->targetSector.c_str()); ImGui::SameLine(0.0f, 8.0f);
							}
							ImGui::Text("%s", c_editorInfMessageTypeName[msg->type]);

							if (msg->type == IMT_GOTO_STOP || msg->type == IMT_COMPLETE)
							{
								ImGui::SameLine(0.0f, 8.0f);
								ImGui::Text("%u", msg->arg[0]);
							}
							else if (msg->type == IMT_SET_BITS || msg->type == IMT_CLEAR_BITS)
							{
								ImGui::SameLine(0.0f, 8.0f);
								ImGui::Text("%u %u", msg->arg[0], msg->arg[1]);
							}
						}

						const s32 adjoinCount = (s32)stop->adjoinCmd.size();
						const Editor_InfAdjoinCmd* adjoinCmd = stop->adjoinCmd.data();
						for (s32 a = 0; a < adjoinCount; a++, adjoinCmd++)
						{
							ImGui::Text("%s%s", tab, tab); ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextColored(colorKeywordInner, "Adjoin:"); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%d", s); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%s %d %s %d", adjoinCmd->sector0.c_str(), adjoinCmd->wallIndex0, adjoinCmd->sector1.c_str(), adjoinCmd->wallIndex1);
						}

						const s32 texCount = (s32)stop->textureCmd.size();
						const Editor_InfTextureCmd* texCmd = stop->textureCmd.data();
						for (s32 t = 0; t < texCount; t++, texCmd++)
						{
							ImGui::Text("%s%s", tab, tab); ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextColored(colorKeywordInner, "Texture:"); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%d", s); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%s %s", texCmd->fromCeiling ? "C" : "F", texCmd->donorSector.c_str());
						}

						if (stop->overrideSet & ISO_PAGE)
						{
							ImGui::Text("%s%s", tab, tab); ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextColored(colorKeywordInner, "Page:"); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%d", s); ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%s", stop->page.c_str());
						}
					}

					const s32 slaveCount = (s32)elev->slaves.size();
					const Editor_InfSlave* slave = elev->slaves.data();
					for (s32 s = 0; s < slaveCount; s++, slave++)
					{
						ImGui::Text("%s", tab); ImGui::SameLine(0.0f, 0.0f);
						ImGui::TextColored(colorKeywordOuter, "Slave:"); ImGui::SameLine(0.0f, 8.0f);
						ImGui::Text("%s", slave->name.c_str());
						if (slave->angleOffset != 0.0f)
						{
							ImGui::SameLine(0.0f, 8.0f);
							ImGui::Text("%s", infFloatToString(slave->angleOffset));
						}
					}
				} break;
				case IIC_TRIGGER:
				{
				} break;
				case IIC_TELEPORTER:
				{
				} break;
			}
		}
	}

	bool editor_infSectorEdit()
	{
		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);
		const f32 winWidth  = min(940.0f, (f32)info.width - 16);
		const f32 winHeight = (f32)info.height - 16;
				
		pushFont(TFE_Editor::FONT_SMALL);

		bool active = true;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;
		ImGui::SetNextWindowSize({ winWidth, winHeight });
		if (ImGui::BeginPopupModal("Sector INF", &active, window_flags))
		{
			if (!s_infEditor.sector)
			{
				ImGui::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "Sectors with INF functionality must have names.\nPlease name the sector and try again.");
			}
			else if (!s_infEditor.item)
			{
				if (ImGui::Button("Create INF Item"))
				{
					s_levelInf.item.push_back({});
					s_infEditor.item = &s_levelInf.item.back();
					s_infEditor.item->name = s_infEditor.sector->name;
					s_infEditor.item->wallNum = -1;
				}
			}
			else if (s_infEditor.item)
			{
				// Display it for now.
				ImGui::Text("Item: %s, Sector ID: %d", s_infEditor.item->name.c_str(), s_infEditor.sector->id);
				ImGui::Separator();

				if (editor_button("+"))
				{
					if (s_infEditor.comboClassIndex == IIC_ELEVATOR)
					{
						allocElev(s_infEditor.item);
					}
					else if (s_infEditor.comboClassIndex == IIC_TRIGGER)
					{
						// TODO
					}
					else if (s_infEditor.comboClassIndex == IIC_TELEPORTER)
					{
						// TODO
					}
				}
				ImGui::SameLine(0.0f, 4.0f);
				if (editor_button("-"))
				{
					// TODO
				}
				ImGui::SameLine(0.0f, 16.0f);
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::BeginCombo("##SectorClassCombo", c_infClassName[s_infEditor.comboClassIndex]))
				{
					s32 count = (s32)TFE_ARRAYSIZE(c_infClassName);
					for (s32 i = 0; i < count; i++)
					{
						if (ImGui::Selectable(c_infClassName[i], i == s_infEditor.comboClassIndex))
						{
							s_infEditor.comboClassIndex = i;
						}
						//setTooltip(c_infClassName[i].tooltip.c_str());
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine(winWidth - 128.0f);
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::Button(s_infEditor.mode == INF_MODE_UI ? "INF Code" : "INF UI"))
				{
					if (s_infEditor.mode == INF_MODE_UI)
					{
						// TODO: Generate code.
						s_infEditor.mode = INF_MODE_CODE;
					}
					else
					{
						s_infEditor.mode = INF_MODE_UI;
					}
				}

				ImGui::Separator();

				if (ImGui::BeginChild(editor_getUniqueLabel(""), { 0, 0 }, false))
				{
					if (s_infEditor.mode == INF_MODE_UI)
					{
						editor_infSectorEdit_UI();
					}
					else
					{
						editor_InfSectorEdit_Code();
					}
					ImGui::EndChild();
				}
			}
			else
			{
				// Create new.
				ImGui::Text("Stub... create new INF item.");
			}

			ImGui::EndPopup();
		}

		popFont();

		return !active;
	}
}