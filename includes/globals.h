//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

//
// Okapi Module
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <amxxmodule.h>
#include "game_library.h"
#include "sm_stringhashmap.h"
#include "command.h"
#include "memory_.h"
#include "offset_handler.h"
#include "hl_type_conversion.h"
#include "fakemeta_const_to_address.h"
#include "function.h"
#include "allocator.h"
#include "utils.h"

struct GameLibrariesType
{
	GameLibrary* Mod;
	GameLibrary* Engine;
	StringHashMap<GameLibrary*> Others;
};

typedef ke::HashMap< long, Function*, ke::IntegerPolicy<long> > FunctionsDataMap;

enum ArgsType
{
	ArgVoid,
	ArgInt,
	ArgCBase,
	ArgFloat,
	ArgVec,
	ArgEntvars,
	ArgStr,
	ArgEdict,
	ArgVecPtr,

	ArgsCount
};

enum MemType
{
	MemType_Int,
	MemType_Byte,
	MemType_Foat,
	MemType_Entvars,
	MemType_Edict,
	MemType_Cbase,
	MemType_Array,
	MemType_String,
	MemType_Vector,

	MemType_Count
};

extern GameLibrariesType       G_GameLibraries;
extern StringHashMap<Command*> G_Commands;
extern Memory                  G_Memory;
extern OffsetHandler*          G_OffsetHandler;
extern HL_TypeConversion       G_HL_TypeConversion;
extern FakemetaConstToAddress* G_FakemetaConstToAddress;
extern FunctionsDataMap        G_Functions;
extern Allocator               G_Allocator;
extern TypeHandler*            G_ArgsTypeHandler[ArgsCount];

void list_commands();

#endif // GLOBALS_H
