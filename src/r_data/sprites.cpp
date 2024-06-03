
#include "doomtype.h"
#include "w_wad.h"
#include "i_system.h"
#include "s_sound.h"
#include "c_console.h"
#include "d_player.h"
#include "d_netinf.h"
#include "gi.h"
#include "colormatcher.h"
#include "c_dispatch.h"
#include "r_defs.h"
#include "v_text.h"
#include "r_data/sprites.h"
#include "r_data/voxels.h"
#include "textures/textures.h"
#include "v_video.h"

void gl_InitModels();

// variables used to look up
//	and range check thing_t sprites patches
TArray<spritedef_t> sprites;
TArray<spriteframe_t> SpriteFrames;
DWORD			NumStdSprites;		// The first x sprites that don't belong to skins.

struct spriteframewithrotate : public spriteframe_t
{
	int rotate;
}
sprtemp[MAX_SPRITE_FRAMES];
int 			maxframe;
char*			spritename;

// [RH] skin globals
// [BL] Changed to TArray
TArray<FPlayerSkin> skins;
TArray<FPlayerSkinRemover> skinremove; // [BOF]
BYTE			OtherGameSkinRemap[256];
PalEntry		OtherGameSkinPalette[256];



//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
// [RH] Removed checks for coexistance of rotation 0 with other
//		rotations and made it look more like BOOM's version.
//
static bool R_InstallSpriteLump (FTextureID lump, unsigned frame, char rot, bool flipped)
{
	unsigned rotation;

	if (rot >= '0' && rot <= '9')
	{
		rotation = rot - '0';
	}
	else if (rot >= 'A')
	{
		rotation = rot - 'A' + 10;
	}
	else
	{
		rotation = 17;
	}

	if (frame >= MAX_SPRITE_FRAMES || rotation > 16)
	{
		Printf (TEXTCOLOR_RED"R_InstallSpriteLump: Bad frame characters in lump %s\n", TexMan[lump]->Name);
		return false;
	}

	if ((int)frame > maxframe)
		maxframe = frame;

	if (rotation == 0)
	{
		// the lump should be used for all rotations
        // false=0, true=1, but array initialised to -1
        // allows doom to have a "no value set yet" boolean value!
		int r;

		for (r = 14; r >= 0; r -= 2)
		{
			if (!sprtemp[frame].Texture[r].isValid())
			{
				sprtemp[frame].Texture[r] = lump;
				if (flipped)
				{
					sprtemp[frame].Flip |= 1 << r;
				}
				sprtemp[frame].rotate = false;
			}
		}
	}
	else
	{
		if (rotation <= 8)
		{
			rotation = (rotation - 1) * 2;
		}
		else
		{
			rotation = (rotation - 9) * 2 + 1;
		}

		if (!sprtemp[frame].Texture[rotation].isValid())
		{
			// the lump is only used for one rotation
			sprtemp[frame].Texture[rotation] = lump;
			if (flipped)
			{
				sprtemp[frame].Flip |= 1 << rotation;
			}
			sprtemp[frame].rotate = true;
		}
	}
	return true;
}


// [RH] Seperated out of R_InitSpriteDefs()
static void R_InstallSprite (int num)
{
	int frame;
	int framestart;
	int rot;
//	int undefinedFix;

	if (maxframe == -1)
	{
		sprites[num].numframes = 0;
		return;
	}

	maxframe++;

	// [RH] If any frames are undefined, but there are some defined frames, map
	// them to the first defined frame. This is a fix for Doom Raider, which actually
	// worked with ZDoom 2.0.47, because of a bug here. It does not define frames A,
	// B, or C for the sprite PSBG, but because I had sprtemp[].rotate defined as a
	// bool, this code never detected that it was not actually present. After switching
	// to the unified texture system, this caused it to crash while loading the wad.

// [RH] Let undefined frames actually be blank because LWM uses this in at least
// one of her wads.
//	for (frame = 0; frame < maxframe && sprtemp[frame].rotate == -1; ++frame)
//	{ }
//
//	undefinedFix = frame;

	for (frame = 0; frame < maxframe; ++frame)
	{
		switch (sprtemp[frame].rotate)
		{
		case -1:
			// no rotations were found for that frame at all
			//I_FatalError ("R_InstallSprite: No patches found for %s frame %c", sprites[num].name, frame+'A');
			break;
			
		case 0:
			// only the first rotation is needed
			for (rot = 1; rot < 16; ++rot)
			{
				sprtemp[frame].Texture[rot] = sprtemp[frame].Texture[0];
			}
			// If the frame is flipped, they all should be
			if (sprtemp[frame].Flip & 1)
			{
				sprtemp[frame].Flip = 0xFFFF;
			}
			break;
					
		case 1:
			// must have all 8 frame pairs
			for (rot = 0; rot < 8; ++rot)
			{
				if (!sprtemp[frame].Texture[rot*2+1].isValid())
				{
					sprtemp[frame].Texture[rot*2+1] = sprtemp[frame].Texture[rot*2];
					if (sprtemp[frame].Flip & (1 << (rot*2)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2+1);
					}
				}
				if (!sprtemp[frame].Texture[rot*2].isValid())
				{
					sprtemp[frame].Texture[rot*2] = sprtemp[frame].Texture[rot*2+1];
					if (sprtemp[frame].Flip & (1 << (rot*2+1)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2);
					}
				}

			}
			for (rot = 0; rot < 16; ++rot)
			{
				if (!sprtemp[frame].Texture[rot].isValid())
					I_FatalError ("R_InstallSprite: Sprite %s frame %c is missing rotations",
									sprites[num].name, frame+'A');
			}
			break;
		}
	}

	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == -1)
		{
			memset (&sprtemp[frame].Texture, 0, sizeof(sprtemp[0].Texture));
			sprtemp[frame].Flip = 0;
			sprtemp[frame].rotate = 0;
		}
	}
	
	// allocate space for the frames present and copy sprtemp to it
	sprites[num].numframes = maxframe;
	sprites[num].spriteframes = WORD(framestart = SpriteFrames.Reserve (maxframe));
	for (frame = 0; frame < maxframe; ++frame)
	{
		memcpy (SpriteFrames[framestart+frame].Texture, sprtemp[frame].Texture, sizeof(sprtemp[frame].Texture));
		SpriteFrames[framestart+frame].Flip = sprtemp[frame].Flip;
		SpriteFrames[framestart+frame].Voxel = sprtemp[frame].Voxel;
	}

	// Let the textures know about the rotations
	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == 1)
		{
			for (int rot = 0; rot < 16; ++rot)
			{
				TexMan[sprtemp[frame].Texture[rot]]->Rotations = framestart + frame;
			}
		}
	}
}


//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//	(4 chars exactly) to be used.
// Builds the sprite rotation matrices to account
//	for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//	a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//	letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs () 
{
	struct Hasher
	{
		int Head, Next;
	} *hashes;
	struct VHasher
	{
		int Head, Next, Name, Spin;
		char Frame;
	} *vhashes;
	unsigned int i, j, smax, vmax;
	DWORD intname;

	// Create a hash table to speed up the process
	smax = TexMan.NumTextures();
	hashes = new Hasher[smax];
	clearbuf(hashes, sizeof(Hasher)*smax/4, -1);
	for (i = 0; i < smax; ++i)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if (tex->UseType == FTexture::TEX_Sprite && strlen(tex->Name) >= 6)
		{
			size_t bucket = tex->dwName % smax;
			hashes[i].Next = hashes[bucket].Head;
			hashes[bucket].Head = i;
		}
	}

	// Repeat, for voxels
	vmax = Wads.GetNumLumps();
	vhashes = new VHasher[vmax];
	clearbuf(vhashes, sizeof(VHasher)*vmax/4, -1);
	for (i = 0; i < vmax; ++i)
	{
		if (Wads.GetLumpNamespace(i) == ns_voxels)
		{
			char name[9];
			size_t namelen;
			int spin;
			int sign;

			Wads.GetLumpName(name, i);
			name[8] = 0;
			namelen = strlen(name);
			if (namelen < 4)
			{ // name is too short
				continue;
			}
			if (name[4] != '\0' && name[4] != ' ' && (name[4] < 'A' || name[4] >= 'A' + MAX_SPRITE_FRAMES))
			{ // frame char is invalid
				continue;
			}
			spin = 0;
			sign = 2;	// 2 to convert from deg/halfsec to deg/sec
			j = 5;
			if (j < namelen && name[j] == '-')
			{ // a minus sign is okay, but only before any digits
				j++;
				sign = -2;
			}
			for (; j < namelen; ++j)
			{ // the remainder to the end of the name must be digits
				if (name[j] >= '0' && name[j] <= '9')
				{
					spin = spin * 10 + name[j] - '0';
				}
				else
				{
					break;
				}
			}
			if (j < namelen)
			{ // the spin part is invalid
				continue;
			}
			memcpy(&vhashes[i].Name, name, 4);
			vhashes[i].Frame = name[4];
			vhashes[i].Spin = spin * sign;
			size_t bucket = vhashes[i].Name % vmax;
			vhashes[i].Next = vhashes[bucket].Head;
			vhashes[bucket].Head = i;
		}
	}

	// scan all the lump names for each of the names, noting the highest frame letter.
	for (i = 0; i < sprites.Size(); ++i)
	{
		memset (sprtemp, 0xFF, sizeof(sprtemp));
		for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
		{
			sprtemp[j].Flip = 0;
			sprtemp[j].Voxel = NULL;
		}
				
		maxframe = -1;
		intname = sprites[i].dwName;

		// scan the lumps, filling in the frames for whatever is found
		int hash = hashes[intname % smax].Head;
		while (hash != -1)
		{
			FTexture *tex = TexMan[hash];
			if (tex->dwName == intname)
			{
				bool res = R_InstallSpriteLump (FTextureID(hash), tex->Name[4] - 'A', tex->Name[5], false);

				if (tex->Name[6] && res)
					R_InstallSpriteLump (FTextureID(hash), tex->Name[6] - 'A', tex->Name[7], true);
			}
			hash = hashes[hash].Next;
		}

		// repeat, for voxels
		hash = vhashes[intname % vmax].Head;
		while (hash != -1)
		{
			VHasher *vh = &vhashes[hash];
			if (vh->Name == (int)intname)
			{
				FVoxelDef *voxdef = R_LoadVoxelDef(hash, vh->Spin);
				if (voxdef != NULL)
				{
					if (vh->Frame == ' ' || vh->Frame == '\0')
					{ // voxel applies to every sprite frame
						for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
						{
							if (sprtemp[j].Voxel == NULL)
							{
								sprtemp[j].Voxel = voxdef;
							}
						}
						maxframe = MAX_SPRITE_FRAMES-1;
					}
					else
					{ // voxel applies to a specific frame
						j = vh->Frame - 'A';
						sprtemp[j].Voxel = voxdef;
						maxframe = MAX<int>(maxframe, j);
					}
				}
			}
			hash = vh->Next;
		}
		
		R_InstallSprite ((int)i);
	}

	delete[] hashes;
	delete[] vhashes;
}

//==========================================================================
//
// R_ExtendSpriteFrames
//
// Extends a sprite so that it can hold the desired frame.
//
//==========================================================================

static void R_ExtendSpriteFrames(spritedef_t &spr, int frame)
{
	unsigned int i, newstart;

	if (spr.numframes >= ++frame)
	{ // The sprite already has enough frames, so do nothing.
		return;
	}

	if (spr.numframes == 0 || (spr.spriteframes + spr.numframes == SpriteFrames.Size()))
	{ // Sprite's frames are at the end of the array, or it has no frames
	  // at all, so we can tack the new frames directly on to the end
	  // of the SpriteFrames array.
		newstart = SpriteFrames.Reserve(frame - spr.numframes);
		if (spr.numframes == 0)
		{
			spr.spriteframes = WORD(newstart);
		}
	}
	else
	{ // We need to allocate space for all the sprite's frames and copy
	  // the existing ones over to the new space. The old space will be
	  // lost.
		newstart = SpriteFrames.Reserve(frame);
		for (i = 0; i < spr.numframes; ++i)
		{
			SpriteFrames[newstart + i] = SpriteFrames[spr.spriteframes + i];
		}
		spr.spriteframes = WORD(newstart);
		newstart += i;
	}
	// Initialize all new frames to 0.
	memset(&SpriteFrames[newstart], 0, sizeof(spriteframe_t)*(frame - spr.numframes));
	spr.numframes = frame;
}

//==========================================================================
//
// VOX_AddVoxel
//
// Sets a voxel for a single sprite frame.
//
//==========================================================================

void VOX_AddVoxel(int sprnum, int frame, FVoxelDef *def)
{
	R_ExtendSpriteFrames(sprites[sprnum], frame);
	SpriteFrames[sprites[sprnum].spriteframes + frame].Voxel = def;
}



// [RH]
// R_InitSkins
// Reads in everything applicable to a skin. The skins should have already
// been counted and had their identifiers assigned to namespaces.
//
#define NUMSKINSOUNDS 18
static const char *skinsoundnames[NUMSKINSOUNDS][2] =
{ // The *painXXX sounds must be the first four
	{ "dsplpain",	"*pain100" },
	{ "dsplpain",	"*pain75" },
	{ "dsplpain",	"*pain50" },
	{ "dsplpain",	"*pain25" },
	{ "dsplpain",	"*poison" },

	{ "dsoof",		"*grunt" },
	{ "dsoof",		"*land" },

	{ "dspldeth",	"*death" },
	{ "dspldeth",	"*wimpydeath" },

	{ "dspdiehi",	"*xdeath" },
	{ "dspdiehi",	"*crazydeath" },

	{ "dsnoway",	"*usefail" },
	{ "dsnoway",	"*puzzfail" },

	{ "dsslop",		"*gibbed" },
	{ "dsslop",		"*splat" },

	{ "dspunch",	"*fist" },
	{ "dsjump",		"*jump" },
	{ "dstaunt",	"*taunt" },
};

/*
static int STACK_ARGS skinsorter (const void *a, const void *b)
{
	return stricmp (((FPlayerSkin *)a)->name, ((FPlayerSkin *)b)->name);
}
*/

static void R_CreateSkin();
void R_InitSkins (void)
{
	FSoundID playersoundrefs[NUMSKINSOUNDS];
	spritedef_t temp;
	int sndlumps[NUMSKINSOUNDS];

	//[BOF] Allow key to be any length
	FString key;

	//[BOF] 'intname' turned into an array to parse and store multiple sprites with.
	TMap<DWORD, DWORD> intname;

	int pclass;
	int i;
	int j, k, base;
	int lastlump;
	int aliasid;
	bool remove;
	bool rangeChanged;
	const PClass *basetype, *transtype;
	int s_skin = 1; // (0 = skininfo, 1 = s_skin, 2 = s_skin non-changeable)
	bool lumpSkininfo = false; // are we parsing the SKININFO lumps

	i = PlayerClasses.Size () - 1;
	lastlump = 0;

	for (j = 0; j < NUMSKINSOUNDS; ++j)
	{
		playersoundrefs[j] = skinsoundnames[j][1];
	}

	//[BL] We need to parse two different lumps.
	while (true)
	{
		if ( !lumpSkininfo && (base = Wads.FindLump ("S_SKIN", &lastlump, true)) == -1)
		{
			lastlump = 0;
			lumpSkininfo = true;
			continue;
		}
		else if( lumpSkininfo && (base = Wads.FindLump( "SKININFO", &lastlump, true )) == -1)
		{
			break;
		}

		//[BL] removed some restrictions since Skulltag doesn't need them.
		//     Also, default to S_SKIN format.
		s_skin = 1;

		R_CreateSkin();
		i++;

		FScanner sc(base);

		
		// [BOF] Slight parser rework using Tokens instead of Strings.
		

		// Data is stored as "key = data".
		while (sc.GetToken())
		{
			// [BB] The original SKININFO parser ate everything before the starting bracket.
			// To retain compatibility with existing wads, we need to keep this behavior.
			if (lumpSkininfo)
			{
				// Parse until we find a starting bracket.
				if (sc.String[0] != '{')
				{
					continue;
				}
			}

			if (sc.String[0] == '{')
			{
				if (s_skin == 1)
					s_skin = 0; // Change to SKININFO
				else if (s_skin == 0)
				{
					R_CreateSkin();
					i++; // new skin
				}
				if (s_skin != 2) // If this is at S_SKIN unchangeable then no nothing else get a new string.
					sc.GetToken();
			}

			// Ready up for the next potential skin.
			skins[i].namespc = Wads.GetLumpNamespace (base); 
			skins[i].parentwad = Wads.GetParentWad(Wads.GetWadnumFromLumpnum(base)); // [BOF] For Removal of Skins outside of KEYCONF wad.
			for (j = 0; j < NUMSKINSOUNDS; j++) sndlumps[j] = -1; // Clear temp sndlumps
			remove = false;
			rangeChanged = false;
			pclass = NULL;
			intname.Clear();	//	Clear temp sprites list
			basetype = transtype = NULL;	//	Clear class value.
			if (s_skin == 1) s_skin = 2; // If it doesn't find a '{' permanently use S_SKIN format.

			do
			{
				
				if (!s_skin && sc.End && !sc.CheckToken('}')) // Remove skin at end of SKININFO with no ending bracket
				{
					Printf(PRINT_BOLD, "Unexpected end of file for skin %i. %s\n", i,
						(strlen(skins[i].name)) ? skins[i].name : "");
					remove = true;
					break;
				}
				
				key = sc.String;key.ToLower(); // Keep all lowercase to prevent inconsistencies with GetSkinInfo
				
				if (!sc.CheckToken('='))
				{
					Printf(PRINT_BOLD, "Bad format for skin %d: %s\n", (int)i, key);
					// [BB] If there was a problem parsing the skin, remove it. Otherwise bad things may happen.
					remove = true;
					break;
				}

				sc.GetString();

				// Name 
				if (!key.Compare("name"))
				{
					// [BC] MAX_SKIN_NAME.
					strncpy(skins[i].name, sc.String, MAX_SKIN_NAME);

					skins[i].param[key].list[1] = skins[i].param[key].list[0] = skins[i].name;

					// [BOF] Check for name after parsing and erase duplicates within same class instead.
					
					// [BOF] Prevent skins from intentionally being named 'skin#'
					if (strncmp(skins[i].name, "skin", 4) == 0 && sc.StringLen > 4)
					{
						char check[MAX_SKIN_NAME - 4];
						strncpy(check, &sc.String[4], MAX_SKIN_NAME - 4);
						if (IsNum(check))
						{
							Printf(PRINT_BOLD, "Skin %s renamed to skin%d\n",
								skins[i].name, (int)i);
							mysnprintf(skins[i].name, countof(skins[i].name), "skin%d", (int)i);
						}
					}
				}

				// Sprite
				else if (!key.Compare("sprite"))
				{
					for (j = 3; j >= 0; j--)
						sc.String[j] = toupper(sc.String[j]);
					intname[0] = *((DWORD*)sc.String);

					skins[i].param[key].list[0] = sc.String;
					skins[i].param[key].list[0].Truncate(4);
				}

				// Crouching Sprite
				else if (!key.Compare("crouchsprite"))
				{
					for (j = 3; j >= 0; j--)
						sc.String[j] = toupper(sc.String[j]);
					intname[-1] = *((DWORD*)sc.String);

					skins[i].param[key].list.Resize(1);
					skins[i].param[key].list[0] = sc.String;
					skins[i].param[key].list[0].Truncate(4);
				}

				// HUD Face
				else if (!key.Compare("face"))
				{
					for (j = 2; j >= 0; j--)
						skins[i].face[j] = toupper(sc.String[j]);
					skins[i].face[3] = '\0';
					
					skins[i].param[key].list.Resize(1);
					skins[i].param[key].list[0] = skins[i].face;
				}

				// Gender
				else if (!key.Compare("gender"))
				{
					skins[i].gender = D_GenderToInt(sc.String);

					skins[i].param[key].list.Resize(2);
					skins[i].param[key].list[0].Format("%i", skins[i].gender);	// Integer value of gender
					skins[i].param[key].list[1] = sc.String;		// Actual field entry
				}

				// Scale
				else if (!key.Compare("scale"))
				{ // [BOF] You can set X and Y scales independently now. Just one argument will still set both to the same value.
					sc.UnGet();
					sc.GetToken();
					skins[i].ScaleX[0] = clamp<fixed_t>(FLOAT2FIXED(atof(sc.String)), 1, 256 * FRACUNIT);
					skins[i].param[key].list.Resize(2);
					skins[i].param[key].list[0].Format("%i", skins[i].ScaleX);

					if (sc.CheckToken(','))
					{
						sc.GetToken();
						skins[i].ScaleY[0] = clamp<fixed_t>(FLOAT2FIXED(atof(sc.String)), 1, 256 * FRACUNIT);
						skins[i].param[key].list[1].Format("%i", skins[i].ScaleY);
					}
					else
					{
						skins[i].ScaleY[0] = skins[i].ScaleX[0];
						skins[i].param[key].list[1] = skins[i].param[key].list[0];
					}
				}

				// Game
				else if (!key.Compare("game"))
				{
					if (gameinfo.gametype == GAME_Heretic)
						basetype = PClass::FindClass (NAME_HereticPlayer);
					else if (gameinfo.gametype == GAME_Strife)
						basetype = PClass::FindClass (NAME_StrifePlayer);
					else
						basetype = PClass::FindClass (NAME_DoomPlayer);

					transtype = basetype;

					if (stricmp (sc.String, "heretic") == 0)
					{
						if (gameinfo.gametype & GAME_DoomChex)
						{
							transtype = PClass::FindClass (NAME_HereticPlayer);
							skins[i].othergame = true;
						}
						else if (gameinfo.gametype != GAME_Heretic)
						{
							remove = true;
						}
					}
					else if (stricmp (sc.String, "strife") == 0)
					{
						if (gameinfo.gametype != GAME_Strife)
						{
							remove = true;
						}
					}
					else
					{
						if (gameinfo.gametype == GAME_Heretic)
						{
							transtype = PClass::FindClass (NAME_DoomPlayer);
							skins[i].othergame = true;
						}
						else if (!(gameinfo.gametype & GAME_DoomChex))
						{
							remove = true;
						}
					}

					if (remove)
						break;

					skins[i].param[key].list.Resize(1);
					skins[i].param[key].list[0] = sc.String;
				}

				// Class
				else if (!key.Compare("class"))
				{ // [GRB] Define the skin for a specific player class
					pclass = D_PlayerClassToInt (sc.String);

					if (pclass < 0)
					{
						remove = true;
						break;
					}

					basetype = transtype = PlayerClasses[pclass].Type;

					skins[i].param[key].list.Resize(2);
					skins[i].param[key].list[0].Format("%i", pclass); //Class Number
					skins[i].param[key].list[1] = sc.String; //Class Name
				}


				// [BL] Skulltag additions

				// Hidden Skin
				else if (!key.Compare("hidden"))
				{ // [BOF] This is backwards, but should be left untouched for compatability.
					if ((stricmp(sc.String, "true") == 0) || (stricmp(sc.String, "yes") == 0))
					{
						skins[i].bRevealed = true;
						skins[i].param[key].list[0] = "0";
					}
					else if ((stricmp(sc.String, "false") == 0) || (stricmp(sc.String, "no") == 0))
					{
						skins[i].bRevealed = false;
						skins[i].param[key].list[0] = "1";
					}
				}

				// Cheat Skin
				else if (!key.Compare("cheat"))
				{
					if ((stricmp(sc.String, "true") == 0) || (stricmp(sc.String, "yes") == 0))
					{
						skins[i].param[key].list[0] = "1";
						skins[i].bCheat = true;
					}
					else if ((stricmp(sc.String, "false") == 0) || (stricmp(sc.String, "no") == 0))
					{
						skins[i].param[key].list[0] = "0";
						skins[i].bCheat = false;
					}
				}

				// Color
				else if (!key.Compare("color"))
				{
					skins[i].szColor = V_GetColor(NULL, sc.String); // [BOF] Set an actual color now
					skins[i].param[key].list.Resize(1);
					skins[i].param[key].list[0].Format("%06X", skins[i].szColor);
					
				}


				// [BOF] Zandronum additions

				// Array of Sprites
				else if (!key.Compare("sprites"))
				{
					sc.UnGet();
					if (!sc.CheckToken('['))
					{
						Printf(PRINT_BOLD, "Bad format for skin %d: sprites\n", (int)i);
						remove = true;
						break;
					}

					skins[i].param[key].charlist.Clear();
					while (!sc.CheckToken(']'))
					{

						DWORD spritename;

						for (j = 3; j >= 0; j--)
							sc.String[j] = toupper(sc.String[j]);
						spritename = *((DWORD*)sc.String);

						FString charkey = sc.String;
						charkey.Truncate(4);
						charkey.ToLower(); // Keep all keys lowercase for GetSkinInfo

						sc.GetToken();
						if (!sc.CheckToken('='))
						{
							Printf(PRINT_BOLD, "Bad format for skin %d: sprites \"%.4s\"\n", (int)i, &spritename);
							remove = true;
							break;
						}

						sc.GetToken();
						for (j = 3; j >= 0; j--)
							sc.String[j] = toupper(sc.String[j]);
						intname[spritename] = *((DWORD*)sc.String);

						skins[i].param[key].charlist[charkey] = sc.String;
						skins[i].param[key].charlist[charkey].Truncate(4);

						if (!sc.CheckToken(',')) //Set Scale for a sprite,
						{
							skins[i].ScaleX[spritename] = -1;
							skins[i].ScaleY[spritename] = -1;
							continue;
						}
						if (sc.GetToken() && atof(sc.String))
							skins[i].ScaleX[spritename] = clamp<fixed_t>(FLOAT2FIXED(atof(sc.String)), 1, 256 * FRACUNIT);
						if (sc.CheckToken(',') && sc.GetString() && atof(sc.String))
							skins[i].ScaleY[spritename] = clamp<fixed_t>(FLOAT2FIXED(atof(sc.String)), 1, 256 * FRACUNIT);
						else if (skins[i].ScaleX.CheckKey(spritename))
						{
							skins[i].ScaleY[spritename] = skins[i].ScaleX[spritename];
						}
					}
					if (remove == true) break;
				}

				// Display Name for Menus
				else if (!key.Compare("displayname"))
				{
					strncpy(skins[i].displayname, sc.String, MAX_SKIN_NAME);
					skins[i].param[key].list[0] = skins[i].displayname;
				}

				// Color Range
				else if (!key.Compare("colorrange"))
				{ // [BOF] Override a class's translation with the exception of a class using 0,0 Translation
					sc.UnGet();
					sc.GetToken();
					BYTE tempbyte[2];

					tempbyte[0] = (BYTE)atoi(sc.String);

					if (sc.CheckToken(','))
					{
						sc.GetToken();
						tempbyte[1]  = (BYTE)atoi(sc.String);
					}
					else
					{
						break;
					}

					skins[i].range0start = MIN(tempbyte[0], tempbyte[1]);
					skins[i].range0end = MAX(tempbyte[0], tempbyte[1]);
					rangeChanged = true;

					skins[i].param[key].list[0].Format("%i", skins[i].range0start);
					skins[i].param[key].list[1].Format("%i", skins[i].range0end);
				}

				// Selectablility
				else if (!key.Compare("selectable"))
				{ // Only accessible by Overriding (Weapon/ACS)
					if ((stricmp(sc.String, "false") == 0) || (stricmp(sc.String, "no") == 0))
					{
						skins[i].countSkin = skins[i].bRevealedByDefault = false;
						skins[i].param[key].list[0] = "0";
					}
					else 
					{
						skins[i].countSkin = skins[i].bRevealedByDefault = true;
						skins[i].param[key].list[0] = "1";
					}

				}

				// Removable
				else if (!key.Compare("removable"))
				{
					if ((stricmp(sc.String, "true") == 0) || (stricmp(sc.String, "yes") == 0))
					{
						skins[i].removable = true;
						skins[i].param[key].list[0] = "1";
					}
					else 
					{
						skins[i].removable = false;
						skins[i].param[key].list[0] = "0";
					}
				}


				// Sounds

				// ZDoom Sound Replacement
				else if (key[0] == '*')
				{ // Player sound replacment (ZDoom extension)
					int lump = Wads.CheckNumForName(sc.String, skins[i].namespc);
					if (lump == -1)
					{
						lump = Wads.CheckNumForFullName(sc.String, true, ns_sounds);
					}
					if (lump != -1)
					{
						if (stricmp(key, "*pain") == 0)
						{ // Replace all pain sounds in one go
							aliasid = S_AddPlayerSound(skins[i].name, skins[i].gender,
								playersoundrefs[0], lump, true);
							for (int l = 3; l > 0; --l)
							{
								S_AddPlayerSoundExisting(skins[i].name, skins[i].gender,
									playersoundrefs[l], aliasid, true);
							}
						}
						else
						{
							int sndref = S_FindSoundNoHash(key);
							if (sndref != 0)
							{
								S_AddPlayerSound(skins[i].name, skins[i].gender, sndref, lump, true);
							}
						}
					}
					skins[i].param[key].list.Resize(1);
					skins[i].param[key].list[0] = sc.String;
				}

				// Sound Replacement / Custom Parameters
				else
				{
					bool cont = false;
					for (j = 0; j < NUMSKINSOUNDS; j++)
					{
						if (stricmp(key, skinsoundnames[j][0]) == 0)
						{
							sndlumps[j] = Wads.CheckNumForName(sc.String, skins[i].namespc);
							if (sndlumps[j] == -1)
							{ // [BL] no replacement, search all wads?
								sndlumps[j] = Wads.CheckNumForName(sc.String);
							}
							if (sndlumps[j] == -1)
							{ // Replacement not found, try finding it in the global namespace
								sndlumps[j] = Wads.CheckNumForFullName(sc.String, true, ns_sounds);
							}
							skins[i].param[key].list.Resize(1);
							skins[i].param[key].list[0] = sc.String;
							cont = true;
						}

					}
					if (cont) continue; // Don't parse these audio files again below

					// [BOF] Custom value support for GetSkinInfo

					// Custom string array
					if (sc.String[0] == '[')
					{
						skins[i].param[key].charlist.Clear();
						skins[i].param[key].list.Clear();
						sc.GetString();
						do
						{
							if (sc.String[0] == ']')
								break;
							FString charkey = sc.String;

							// If there's no more parsing without hitting ']' or you hit '}' the skin is considered invalid
							if (!sc.GetString() || sc.String[0] != '=' || sc.String[0] == '}')
							{
								Printf(PRINT_BOLD, "Bad format for skin %d: %s\n", (int)i, key);
								remove = true;
								break;
							}
							sc.GetString();
							skins[i].param[key].charlist[charkey] = sc.String;
							//Printf(PRINT_BOLD,"%s : %s = %s\n", key, charkey, skins[i].param[key].charlist[charkey]);
						} while (sc.GetString());
						if (remove == true) break;
					}
					// Custom param or array
					else
					{
						skins[i].param[key].charlist.Clear();
						skins[i].param[key].list.Clear();
						do
						{
							if (skins[i].param[key].list.Size() != 0)
								sc.GetString();
							//Inserts valid classes into a paramlist, if no classes are valid, param["class"] will be empty.
							skins[i].param[key].list.Insert(
								skins[i].param[key].list.Size(),
								sc.String);
							//Printf(PRINT_BOLD, "%s #%i = %s\n", key, skins[i].param[key].list.Size(), sc.String);
						} while (sc.CheckString(","));
					}
				}

			} while ((!s_skin && !sc.CheckToken('}') && sc.GetToken()) || (s_skin && sc.GetToken())); // Check for closing bracket in SKININFO, and end in S_SKIN


			// [BOF] Remove skins through clearplayerskins
			if (!remove)
			{	// (clearplayerskins [all?] [class])
				//pclass = D_PlayerClassToInt(skins[i].param["key"].list[1]); // Have to reinitialize pclass; use skin param.
				for (j = 0; j < skinremove.Size(); j++)
				{
					if (skins[i].parentwad < skinremove[j].KeyConf &&  // Don't delete skins in or after the same wad as the KEYCONF
						(skinremove[j].ClassNum < 0 || (skinremove[j].ClassNum >= 0 && // If ClassNum is -1 
							(pclass == skinremove[j].ClassNum))) && // or pclass matches Classnum
						(skinremove[j].RemoveAll || // Remove all skins if true.
							skins[i].removable)) //Remove removable skins regardless
						remove = true;
				}
			}

			// [GRB] Assume Doom skin by default
			if (!remove && basetype == NULL)
			{
				if (gameinfo.gametype & GAME_DoomChex)
				{
					basetype = transtype = PClass::FindClass (NAME_DoomPlayer);
				}
				else if (gameinfo.gametype == GAME_Heretic)
				{
					basetype = PClass::FindClass (NAME_HereticPlayer);
					transtype = PClass::FindClass (NAME_DoomPlayer);
					skins[i].othergame = true;
				}
				else
				{
					remove = true;
				}
			}

			if (!remove)
			{
				BYTE range0start = transtype->Meta.GetMetaInt(APMETA_ColorRange) & 0xff;
				BYTE range0end = transtype->Meta.GetMetaInt(APMETA_ColorRange) >> 8;

				if (!rangeChanged || (range0start == 0 && range0end == 0)) // [BOF] Don't translate if the Class doesn't want to be translated or if no custom range was set.
				{
					skins[i].range0start = range0start;
					skins[i].range0end = range0end;
					skins[i].param["colorrange"].list[0].Format("%i", skins[i].range0start);
					skins[i].param["colorrange"].list[1].Format("%i", skins[i].range0end);
				}
			
				remove = true;
				for (j = 0; j < (int)PlayerClasses.Size (); j++)
				{
					const PClass *type = PlayerClasses[j].Type;
			
					if (type->IsDescendantOf (basetype) &&
						GetDefaultByType (type)->SpawnState->sprite == GetDefaultByType (basetype)->SpawnState->sprite &&
						type->Meta.GetMetaInt (APMETA_ColorRange) == basetype->Meta.GetMetaInt (APMETA_ColorRange))
					{
						PlayerClasses[j].Skins.Push ((int)i);

						// [BOF] Set default Scale for undefined Sprite array scales
						intmap::Pair* scalepair;
						intmap::Iterator checkScaleX(skins[i].ScaleX);
						while (checkScaleX.NextPair(scalepair))
						{
							if (scalepair->Value == -1) scalepair->Value = GetDefaultByType(type)->scaleX;
						}
						intmap::Iterator checkScaleY(skins[i].ScaleY);
						while (checkScaleY.NextPair(scalepair))
						{
							if (scalepair->Value == -1) scalepair->Value = GetDefaultByType(type)->scaleY;
						}

						remove = false;
					}
				}
			}
			
			if (!remove)
			{

				for (j = 0; j < i; j++) // Mark all previous skins of the same name as hidden if such, and remove from numSkins value for 'skins' command
				{
					if (stricmp(skins[i].name, skins[j].name) == 0)
					{
						skins[i].countSkin = false; // Don't count this skin through 'skins' command
						skins[j].bRevealed = skins[i].bRevealed; // Set all skins of the same anme's hidden status as this skins.
						break;
					}
				}

				if (skins[i].name[0] == 0)
				{
					mysnprintf(skins[i].name, countof(skins[i].name), "skin%d", (int)i);
				}

				// [BOF] Check Skin name within its own class instead of globally.
				bool initialname = false;
				for (j = 0; j < PlayerClasses[pclass].Skins.Size(); j++)
				{
					if (stricmp(skins[i].name, skins[PlayerClasses[pclass].Skins[j]].name) == 0)
					{
						if (initialname == true)
						{
							mysnprintf(skins[i].name, countof(skins[i].name), "skin%d", (int)i);
							Printf(PRINT_BOLD, "Skin %s duplicated as %s\n",
								skins[PlayerClasses[pclass].Skins[j]].name, skins[i].name);
							break;
						}
						initialname = true;
					}
				}


				if (skins[i].displayname[0] == 0)
				{
					strcpy(skins[i].displayname, skins[i].name); //Use CVAR name if no Displayname is available
					skins[i].param["displayname"].list.Resize(1);
					skins[i].param["displayname"].list[0] = skins[i].displayname;
				}

				// Now collect the sprite frames for this skin. If the sprite name was not
				// specified, use whatever immediately follows the specifier lump.
				// [BL] S_SKIN only
				if (!intname.CheckKey(0) && s_skin != 0)
				{
					char name[9];
					Wads.GetLumpName(name, base + 1);
					intname[0] = *(DWORD*)name;
				}
				else if (intname.CountUsed() == 0) //Use class'spawnstate sprite if no intname is added for SKININFO skins.
				{
					skins[i].sprite = GetDefaultByType(basetype)->SpawnState->sprite;
					skins[i].param["sprite"].list[0] = sprites[skins[i].sprite].name;
					continue;
				}

				int basens = Wads.GetLumpNamespace(base);

				//[BOF] Iterate list of sprites instead of just potentially 2
				const TMap<DWORD, DWORD>::Pair* sprpair;
				TMap<DWORD, DWORD>::ConstIterator addSprite(intname);
				int spritesUsed = intname.CountUsed();
				//for (int spr = 0; spr < 2; spr++)
				while (addSprite.NextPair(sprpair))
				{
					auto sprkey = sprpair->Key;
					auto sprval = sprpair->Value;
					memset(sprtemp, 0xFFFF, sizeof(sprtemp));
					for (k = 0; k < MAX_SPRITE_FRAMES; ++k)
					{
						sprtemp[k].Flip = 0;
						sprtemp[k].Voxel = NULL;
					}
					maxframe = -1;

					// [BL] From the SKININFO parser
					// Loop through all the lumps searching for frames for this skin.
					for (k = 0; static_cast<signed> (k) < Wads.GetNumLumps(); k++)
					{
						// Only process skin entries from the wad the SKININFO lump is in.
						// NOTE: If this isn't done, Skulltag doesn't work with hr.wad.
						if (Wads.GetLumpFile(base) != Wads.GetLumpFile(k))
							continue;

						char lname[9];
						DWORD lnameint;
						Wads.GetLumpName(lname, k);
						memcpy(&lnameint, lname, 4);
						if (lnameint == sprval)
						{
							FTextureID picnum = TexMan.CreateTexture(k, FTexture::TEX_SkinSprite);
							if (!picnum.isValid())
								continue;

							bool res = R_InstallSpriteLump(picnum, lname[4] - 'A', lname[5], false);

							if (lname[6] && res)
								R_InstallSpriteLump(picnum, lname[6] - 'A', lname[7], true);
						}
					}

					//Go on to the next sprite if there's no frames.
					if (maxframe <= 0)
					{
						/*Printf(PRINT_BOLD, "Skin %s (#%d) - Sprite %.4s has no frames.\n",
						skins[i].name, (int)i, &sprval);*/
						intname.Remove(sprval);
						spritesUsed--;
						continue;
					}

					memcpy(temp.name, &sprval, 4);
					temp.name[4] = 0;
					int sprno = (int)sprites.Push(temp);

					if (sprkey == 0)
					{
						skins[i].sprites[0] = skins[i].sprite = sprno;
					}

					else if (sprkey == -1)
					{
						skins[i].crouchsprite = sprno;
					}

					else //if (GetSpriteIndex((char*)&sprkey) != -1)
						skins[i].sprites[*(DWORD*)&sprkey] = sprno;

					R_InstallSprite(sprno);
				}
				
				//If there's no sprites at the end, then remove the skin.
				if (spritesUsed == 0)
				{
					Printf(PRINT_BOLD, "Skin %s (#%d) has no frames. Removing.\n", skins[i].name, (int)i);
					remove = true;
					break;
				}

			}

			if (remove)
			{
				skins.Delete(i);
				i--;
				// [TRSR] If a skin is deleted, we need to finish parsing it until its end to prevent the
				// last skin in Zandronum's SKININFO lump from double-removing in niche circumstances.
				if (s_skin == 0)
				{ 

					while (sc.String[0] != '}') // Finish Parsing That Skin
					{
						if (!sc.GetString())
							break;
					}
					continue;
				}
				else
					break; //S_SKIN Generally contains one skin in a lump, unless converted to SKININFO
			}

			// Register any sounds this skin provides
			aliasid = 0;
			for (j = 0; j < NUMSKINSOUNDS; j++)
			{
				if (sndlumps[j] != -1)
				{
					if (j == 0 || sndlumps[j] != sndlumps[j-1])
					{
						aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
							playersoundrefs[j], sndlumps[j], true);
					}
					else
					{
						S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
							playersoundrefs[j], aliasid, true);
					}
				}
			}

			// Make sure face prefix is a full 3 chars
			if (skins[i].face[1] == 0 || skins[i].face[2] == 0)
			{
				skins[i].face[0] = 0;
				skins[i].param["face"].list.Clear();
			}
		}
	}

	if (skins.Size() > PlayerClasses.Size ())
	{ // The sound table may have changed, so rehash it.
		S_HashSounds ();
		S_ShrinkPlayerSoundLists ();
	}
}

// [RH] Find a skin by name
int R_FindSkin (const char *name, int pclass, bool override = false)
{
	// [BOF] Shrink this down a fair bit by parsing skin numbers within class itself.
	for (unsigned i = 0; i < PlayerClasses[pclass].Skins.Size(); i++)
	{
		int classSkin = PlayerClasses[pclass].Skins[i];
		// [BC] Changed from 16 to MAX_SKIN_NAME.
		if (strnicmp(skins[classSkin].name, name, MAX_SKIN_NAME) == 0 &&
			(skins[classSkin].bRevealedByDefault || override))
				return classSkin;
	}
	return pclass;
}

// [RH] List the names of all installed skins
CCMD (skins)
{
	int i;
	ULONG	ulNumSkins;
	ULONG	ulNumHiddenSkins;

	ulNumSkins = 0;
	ulNumHiddenSkins = 0;
	for (i = 0; i < (int)skins.Size(); i++)
	{
		if (!skins[i].countSkin) continue;
		if (skins[i].bRevealed)
		{
			Printf("% 3d %s\n", static_cast<unsigned int> (++ulNumSkins), skins[i].name);
		}
		else
			ulNumHiddenSkins++;
	}

	if (ulNumHiddenSkins == 0)
		Printf("\n%d skins; All hidden skins unlocked!\n", ulNumSkins);
	else
		Printf("\n%d skins; %d remain%s hidden.\n", ulNumSkins, static_cast<unsigned int> (ulNumHiddenSkins), ulNumHiddenSkins == 1 ? "s" : "");
}

//*****************************************************************************
//
static void R_CreateSkin()
{
	FPlayerSkin skin;
	memset(&skin, 0, sizeof(FPlayerSkin));

	const PClass *type = PlayerClasses[0].Type;
	skin.ScaleX = (intmap)skin.ScaleX;
	skin.ScaleY = (intmap)skin.ScaleY;
	skin.ScaleX[0] = GetDefaultByType(type)->scaleX;
	skin.ScaleY[0] = GetDefaultByType(type)->scaleY;

	// [BC/BB] We need to initialize the default sprite, because when we create a skin
	// using SKININFO, we don't necessarily specify a sprite.
	skin.sprite = GetDefaultByType (type)->SpawnState->sprite;
	// [BL] Hidden skins
	skin.bRevealed = true;
	skin.bRevealedByDefault = true;

	skin.countSkin = true; //For 'skins' command

	// [BOF] Default Param Values
	skin.param = (paramlist)skin.param;


	skin.param["name"].list.Resize(2);
	
	skin.param["sprite"].list.Resize(1);
	skin.param["sprite"].list[0] = sprites[skin.sprite].name;

	skin.param["scale"].list.Resize(2);
	skin.param["scale"].list[0].Format("%i", skin.ScaleX);
	skin.param["scale"].list[1].Format("%i", skin.ScaleY);

	skin.param["class"].list.Resize(2);
	skin.param["class"].list[0] = "0";
	skin.param["class"].list[1] = type->Meta.GetMetaString(APMETA_DisplayName);

	skin.param["cheat"].list.Resize(1);
	skin.param["cheat"].list[0] = "0";

	skin.param["hidden"].list.Resize(1);
	skin.param["hidden"].list[0] = "0";

	skin.param["displayname"].list.Resize(1);

	skin.param["selectable"].list.Resize(1);
	skin.param["selectable"].list[0] = "1";
	
	skin.param["colorrange"].list.Resize(2);

	skin.param["removable"].list.Resize(1);
	skin.param["removable"].list[0] = "0";

	skins.Push(skin);
}

// [BB] Helper code for the effective skin sprite width/height check.
static bool R_IsCharUsuableAsSpriteRotation ( const char rot )
{
	unsigned rotation = 17;

	if (rot >= '0' && rot <= '9')
		rotation = rot - '0';
	else if (rot >= 'A')
		rotation = rot - 'A' + 10;

	return ( rotation <= 16 );
}

static void R_CreateSkinTranslation (const char *palname)
{
	FMemLump lump = Wads.ReadLump (palname);
	const BYTE *otherPal = (BYTE *)lump.GetMem();
 
	for (int i = 0; i < 256; ++i)
	{
		OtherGameSkinRemap[i] = ColorMatcher.Pick (otherPal[0], otherPal[1], otherPal[2]);
		OtherGameSkinPalette[i] = PalEntry(otherPal[0], otherPal[1], otherPal[2]);
		otherPal += 3;
	}
}


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites ()
{
	// [BB] Zandronum's skin handling doesn't need this 'lump'.
	int /*lump,*/ lastlump;
	unsigned int i, j;

	// [RH] Create a standard translation to map skins between Heretic and Doom
	if (gameinfo.gametype == GAME_DoomChex)
	{
		R_CreateSkinTranslation ("SPALHTIC");
	}
	else
	{
		R_CreateSkinTranslation ("SPALDOOM");
	}

	lastlump = 0;

	SpriteFrames.Clear();

	// [RH] Do some preliminary setup
	skins.Clear(); // [BB] Adapted to Zandronum's skin handling
	for (i = 0; i < PlayerClasses.Size (); i++)
	{
		R_CreateSkin();
		if (i != 0) skins[i].countSkin = false; // [BOF] Exclude all other Base skins from 'skins' command.
	}

	R_InitSpriteDefs ();
	R_InitVoxels();		// [RH] Parse VOXELDEF
	NumStdSprites = sprites.Size();
	R_InitSkins ();		// [RH] Finish loading skin data

	// [RH] Set up base skin
	// [GRB] Each player class has its own base skin
	for (i = 0; i < PlayerClasses.Size (); i++)
	{
		const PClass *basetype = PlayerClasses[i].Type;
		const char *pclassface = basetype->Meta.GetMetaString (APMETA_Face);

		strcpy (skins[i].name, "Base");
		strcpy(skins[i].displayname, "Base");

		if (pclassface == NULL || strcmp(pclassface, "None") == 0)
		{
			skins[i].face[0] = 'S';
			skins[i].face[1] = 'T';
			skins[i].face[2] = 'F';
			skins[i].face[3] = '\0';
		}
		else
		{
			strcpy(skins[i].face, pclassface);
		}
		skins[i].range0start = basetype->Meta.GetMetaInt (APMETA_ColorRange) & 255;
		skins[i].range0end = basetype->Meta.GetMetaInt (APMETA_ColorRange) >> 8;
		skins[i].ScaleX[0] = GetDefaultByType(basetype)->scaleX;
		skins[i].ScaleY[0] = GetDefaultByType(basetype)->scaleY;
		skins[i].sprite = GetDefaultByType (basetype)->SpawnState->sprite;
		skins[i].namespc = ns_global;

		PlayerClasses[i].Skins.Push (i);

		if (memcmp (sprites[skins[i].sprite].name, "PLAY", 4) == 0)
		{
			for (j = 0; j < sprites.Size (); j++)
			{
				if (memcmp (sprites[j].name, deh.PlayerSprite, 4) == 0)
				{
					skins[i].sprite = (int)j;
					break;
				}
			}
		}

		// [BOF] Base GetSkinInfo



		skins[i].param["displayname"].list[0] =
		skins[i].param["name"].list[1] = skins[i].param["name"].list[0] = skins[i].name;

		skins[i].param["sprite"].list[0] = sprites[skins[i].sprite].name;

		skins[i].param["face"].list.Resize(1);
		skins[i].param["face"].list[0] = skins[i].face;

		skins[i].param["scale"].list[0].Format("%i", skins[i].ScaleX);
		skins[i].param["scale"].list[1].Format("%i", skins[i].ScaleY);

		skins[i].param["class"].list[0] = 0;
		skins[i].param["class"].list[0].Format("%i", i);
		skins[i].param["class"].list[1] = basetype->Meta.GetMetaString(APMETA_DisplayName);

		skins[i].param["colorrange"].list[0].Format("%i", skins[i].range0start);
		skins[i].param["colorrange"].list[1].Format("%i", skins[i].range0end);

	}

	// [BB] Check if any of the skin sprites are ridiculously big to prevent
	// abusing the possibility to replace the skin sprites.
	for ( unsigned int skinIdx = 0; skinIdx < skins.Size(); skinIdx++ )
	{
		// [BB] If the skin doesn't have a name, it's removed and doesn't need to be checked.
		// Removed skins for example are Doom skins in a Hexen games.
		if ( skins[skinIdx].name[0] == 0 )
			continue;

		int maxwidth = 0, maxheight = 0;
		char	szTempLumpName[9];
		szTempLumpName[8]=0;
		FString maxwidthSprite, maxheightSprite;

		// [BB] Loop through all the lumps searching for sprites of this skin.
		// This may look very inefficient, but since this is only called once on
		// startup it's ok.

		// [BOF] Make it even longer, parsing through potentially
		// arrays of sprites per skin as well, while we're at it.

		const TMap<int, int>::Pair* sprpair;
		TMap<int, int>::ConstIterator checkSprite(skins[skinIdx].sprites);
		while (checkSprite.NextPair(sprpair))
		{
			int maxwidth = 0, maxheight = 0;
			char	szTempLumpName[9];
			szTempLumpName[8] = 0;
			FString maxwidthSprite, maxheightSprite;

			int sprval = sprpair->Value;
			int sprkey = sprpair->Key;

			if (sprval == NULL) continue;

			for (ULONG ulIdx = 0; static_cast<signed> (ulIdx) < Wads.GetNumLumps(); ulIdx++)
			{
				Wads.GetLumpName(szTempLumpName, ulIdx);
				if ((strnicmp(szTempLumpName, sprites[sprval].name, 4) == 0)
					// [BB] Only check lumps that possibly can be used as sprite frames.
					&& (static_cast<unsigned> (szTempLumpName[4] - 'A') < MAX_SPRITE_FRAMES)
					// [BB] No need to check Death/XDeath frames.
					&& (szTempLumpName[4] < 'H'))
				{
					// [BB] Check if the lump can be used as sprite. If not, no need to check it.
					if (R_IsCharUsuableAsSpriteRotation(szTempLumpName[5]) == false)
						continue;

					if (szTempLumpName[6])
					{
						// [BB] Only check lumps that possibly can be used as sprite frames.
						if (static_cast<unsigned> (szTempLumpName[6] - 'A') >= MAX_SPRITE_FRAMES)
							continue;

						// [BB] No need to check Death/XDeath frames.
						if (szTempLumpName[6] >= 'H')
							continue;

						if (R_IsCharUsuableAsSpriteRotation(szTempLumpName[7]) == false)
							continue;
					}

					FTextureID texnum = TexMan.CheckForTexture(szTempLumpName, FTexture::TEX_Sprite);
					FTexture* tex = (texnum.Exists()) ? TexMan[texnum] : NULL;
					if (tex)
					{
						if (tex->GetScaledHeight() > maxheight)
						{
							maxheight = tex->GetScaledHeight();
							maxheightSprite = szTempLumpName;
						}
						if (tex->GetScaledWidth() > maxwidth)
						{
							maxwidth = tex->GetScaledWidth();
							maxwidthSprite = szTempLumpName;
						}
					}
				}
			}


			int classSkinIdx = -1;

			// [BB] Find the player class this skin belongs to.
			if (!skins[skinIdx].othergame)
			{
				for (unsigned int pcIdx = 0; pcIdx < PlayerClasses.Size(); pcIdx++)
				{
					if (classSkinIdx != -1)
						break;

					for (unsigned int pcSkinIdx = 0; pcSkinIdx < PlayerClasses[pcIdx].Skins.Size(); pcSkinIdx++)
					{
						if (PlayerClasses[pcIdx].Skins[pcSkinIdx] == static_cast<int> (skinIdx))
						{
							classSkinIdx = pcIdx;
							break;
						}
					}
				}
				// [BB] The skin doesn't seem to belong to any of the the available player classes, so just check it against the standard player class.
				if (classSkinIdx == -1)
					classSkinIdx = 0;
			}
			else
			{
				// [BB] The skin doesn't belong to this game, so just check it against the standard player class.
				classSkinIdx = 0;
			}

			// [TP] How big can the skin be?
			const FMetaTable& meta = PlayerClasses[classSkinIdx].Type->Meta;
			fixed_t maxwidthfactor = meta.GetMetaFixed(APMETA_MaxSkinWidthFactor);
			fixed_t maxheightfactor = meta.GetMetaFixed(APMETA_MaxSkinHeightFactor);

			// [TP] If either of the size factors are 0, we can just skip this.
			if ((maxwidthfactor == 0) || (maxheightfactor == 0))
				continue;

			AActor* def = GetDefaultByType(PlayerClasses[classSkinIdx].Type);
			fixed_t maxAllowedHeight = FixedMul(maxheightfactor, def->height);
			// [BB] 2*radius is approximately the actor width.
			fixed_t maxAllowedWidth = FixedMul(maxwidthfactor, 2 * def->radius);
			FPlayerSkin& skin = skins[skinIdx];

			// [BB] If a skin sprite violates the limits, just downsize it.
			bool sizeLimitsExceeded = false;
			// [BB] Compare the maximal sprite height/width to the height/radius of the player class this skin belongs to.
			// Massmouth is very big, so we have to be pretty lenient here with the checks.
			// [TP] Also, allow 1px lee-way so that we won't complain about scale being off by a
			// fraction of a pixel (would cause messages such as "52px, max is 52px").
			int scalekey = sprkey == 0  ? 0 : *(DWORD*)&sprkey;
			if (maxheight * skin.ScaleY[scalekey] > maxAllowedHeight + FRACUNIT)
			{

				sizeLimitsExceeded = true;
				Printf(TEXTCOLOR_RED "Sprite (%s -> %s) of skin %s is too tall (%dpx, max is %dpx). Downsizing.\n",
					(sprkey == 0 ? "sprite" : sprkey == -1 ? "crouchsprite" : sprites[sprval].name), maxheightSprite.GetChars(),
					skin.name,
					(maxheight * skin.ScaleY[scalekey]) >> FRACBITS,
					maxAllowedHeight >> FRACBITS);
				const fixed_t oldScaleY = skin.ScaleY[scalekey];
				skin.ScaleY[scalekey] = maxAllowedHeight / maxheight;
				// [BB] Preserve the aspect ration of the sprites.
				skin.ScaleX[scalekey] =
				static_cast<fixed_t> (skin.ScaleX[scalekey] * (FIXED2FLOAT(skin.ScaleY[scalekey]) / FIXED2FLOAT(oldScaleY)));
			}

			if (maxwidth * skin.ScaleX[scalekey] > maxAllowedWidth + FRACUNIT)
			{
				sizeLimitsExceeded = true;
				Printf(TEXTCOLOR_RED "Sprite (%s -> %s) of skin %s is too wide (%dpx, max is %dpx). Downsizing.\n",
					(sprkey == 0 ? "sprite" : sprkey == -1 ? "crouchsprite" : sprites[sprval].name), maxwidthSprite.GetChars(),
					skin.name,
					(maxwidth * skin.ScaleX[scalekey]) >> FRACBITS,
					(maxAllowedWidth) >> FRACBITS);
				const fixed_t oldScaleX = skin.ScaleX[scalekey];
				skin.ScaleX[scalekey] = maxAllowedWidth / maxwidth;
				// [BB] Preserve the aspect ration of the sprites.
				skin.ScaleY[scalekey] =
				static_cast<fixed_t> (skin.ScaleY[scalekey] * (FIXED2FLOAT(skin.ScaleX[scalekey]) / FIXED2FLOAT(oldScaleX)));
			}

			// [BB] Don't allow the base skin sprites of the player classes to exceed the limits.
			if (sizeLimitsExceeded && (skinIdx < PlayerClasses.Size()))
			{
				I_FatalError("The base skin sprite of player class %s exceeds the limits!\n", PlayerClasses[skinIdx].Type->TypeName.GetChars());
			}
		}
	}
	// [RH] Sort the skins, but leave base as skin 0
	//qsort (&skins[PlayerClasses.Size ()], skins.Size()-PlayerClasses.Size (), sizeof(FPlayerSkin), skinsorter);

	gl_InitModels();
}

void R_DeinitSpriteData()
{
	// Free skins
	// [BB]
	skins.Clear();
}

// [BC] Allow clients to decide whether or not they want skins enabled.
CUSTOM_CVAR( Int, cl_skins, 1, CVAR_ARCHIVE )
{
	LONG	lSkin;
	ULONG	ulIdx;

	// Loop through all the players and set their sprite according to the value of cl_skins.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].mo == NULL ))
			continue;

		// If cl_skins == 0, then the user wishes to disable all skins.
		if ( self <= 0 )
		{
			lSkin = R_FindSkin( "base", players[ulIdx].CurrentPlayerClass);

			// Make sure the player doesn't change sprites when his state changes.
			players[ulIdx].mo->flags4 |= MF4_NOSKIN;

		}
		// If cl_skins >= 2, then the user wants to disable cheat skins, but allow all others.
		else if ( self >= 2 )
		{
			if ( skins[players[ulIdx].userinfo.GetSkin()].bCheat )
			{
				lSkin = R_FindSkin( "base", players[ulIdx].CurrentPlayerClass);

				// Make sure the player doesn't change sprites when his state changes.
				players[ulIdx].mo->flags4 |= MF4_NOSKIN;
			}
			else
			{
				lSkin = players[ulIdx].userinfo.GetSkin();

				if (( players[ulIdx].mo->GetDefault( )->flags4 & MF4_NOSKIN ) == false )
				{
					players[ulIdx].mo->flags4 &= ~MF4_NOSKIN;
				}
			}
		}
		// If cl_skins == 1, allow all skins to be used.
		else
		{
			lSkin = players[ulIdx].userinfo.GetSkin();

			if (( players[ulIdx].mo->GetDefault( )->flags4 & MF4_NOSKIN ) == false )
				players[ulIdx].mo->flags4 &= ~MF4_NOSKIN;
		}

		// If the skin is valid, set the player's sprite to the skin's sprite.
		if (( lSkin >= 0 ) && ( static_cast<unsigned> (lSkin) < skins.Size() )
		// [BOF] Also add this check for cl_skins to not change to state frames that the skin doesn't have
			&& (players[ulIdx].mo->state->sprite ==
			GetDefaultByType(players[ulIdx].cls)->SpawnState->sprite ||
			skins[players[ulIdx].userinfo.GetSkin()].sprites.CheckKey(*(DWORD*)sprites[players[ulIdx].mo->state->sprite].name))
			&& !(players[ulIdx].mo->flags4 & MF4_NOSKIN))
		{
			players[ulIdx].mo->sprite = 
			skins[lSkin].sprites.CheckKey(*(DWORD*)sprites[players[ulIdx].mo->state->sprite].name) ?
			skins[lSkin].sprites[*(DWORD*)sprites[players[ulIdx].mo->state->sprite].name] :
			skins[lSkin].sprite;
/*
			players[ulIdx].mo->scaleX = skins[lSkin].ScaleX;
			players[ulIdx].mo->scaleY = skins[lSkin].ScaleY;

			// Make sure the player doesn't change sprites when his state changes.
			if ( lSkin == R_FindSkin( "base", players[ulIdx].CurrentPlayerClass ))
				players[ulIdx].mo->flags4 |= MF4_NOSKIN;
			else
			{
				if (( players[ulIdx].mo->GetDefault( )->flags4 & MF4_NOSKIN ) == false )
					players[ulIdx].mo->flags4 &= ~MF4_NOSKIN;
			}
*/
		}
	}
}

