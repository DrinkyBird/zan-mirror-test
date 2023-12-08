//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: medal.cpp
//
// Description: Contains medal routines and globals
//
//-----------------------------------------------------------------------------

#include "a_sharedglobal.h"
#include "announcer.h"
#include "chat.h"
#include "cl_demo.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "duel.h"
#include "gi.h"
#include "gamemode.h"
#include "g_level.h"
#include "info.h"
#include "medal.h"
#include "p_local.h"
#include "d_player.h"
#include "network.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "sv_commands.h"
#include "team.h"
#include "templates.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_acs.h"
#include "st_hud.h"
#include "c_console.h"
#include "voicechat.h"

//*****************************************************************************
//	VARIABLES

// A list of all defined medals.
static	MEDAL_t	g_Medals[NUM_MEDALS];

// Any medals that players have recently earned that need to be displayed.
static	MEDALQUEUE_t	medalQueue[MAXPLAYERS];

// Has the first frag medal been awarded this round?
static	bool			g_bFirstFragAwarded;

// [Dusk] Need this from p_interaction.cpp for spawn telefrag checking
extern FName MeansOfDeath;

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_medals, true, CVAR_ARCHIVE )
CVAR( Bool, cl_icons, true, CVAR_ARCHIVE )

//*****************************************************************************
//	PROTOTYPES

ULONG	medal_GetDesiredIcon( player_t *pPlayer, AInventory *&pTeamItem );
void	medal_TriggerMedal( ULONG ulPlayer );
void	medal_SelectIcon( player_t *player );
void	medal_CheckForFirstFrag( ULONG ulPlayer );
void	medal_CheckForDomination( ULONG ulPlayer );
void	medal_CheckForFistingOrSpam( ULONG ulPlayer, int dmgflags );
void	medal_CheckForExcellent( ULONG ulPlayer );
void	medal_CheckForTermination( ULONG ulDeadPlayer, ULONG ulPlayer );
void	medal_CheckForLlama( ULONG ulDeadPlayer, ULONG ulPlayer );
void	medal_CheckForYouFailIt( ULONG ulPlayer );
bool	medal_PlayerHasCarrierIcon( player_t *player );

//*****************************************************************************
//	FUNCTIONS

void MEDAL_Construct( void )
{
	FActorInfo *const floatyIconInfo = RUNTIME_CLASS( AFloatyIcon )->ActorInfo;

	// Excellent
	g_Medals[MEDAL_EXCELLENT].icon = TexMan.CheckForTexture( "EXCLA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_EXCELLENT].iconState = floatyIconInfo->FindStateByString( "Excellent", true );
	g_Medals[MEDAL_EXCELLENT].text = "Excellent!";
	g_Medals[MEDAL_EXCELLENT].textColor = CR_GREY;
	g_Medals[MEDAL_EXCELLENT].announcerEntry = "Excellent";
	g_Medals[MEDAL_EXCELLENT].lowerMedal = nullptr;

	// Incredible
	g_Medals[MEDAL_INCREDIBLE].icon = TexMan.CheckForTexture( "INCRA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_INCREDIBLE].iconState = floatyIconInfo->FindStateByString( "Incredible", true );
	g_Medals[MEDAL_INCREDIBLE].text = "Incredible!";
	g_Medals[MEDAL_INCREDIBLE].textColor = CR_RED;
	g_Medals[MEDAL_INCREDIBLE].announcerEntry = "Incredible";
	g_Medals[MEDAL_INCREDIBLE].lowerMedal = &g_Medals[MEDAL_EXCELLENT];

	// Impressive
	g_Medals[MEDAL_IMPRESSIVE].icon = TexMan.CheckForTexture( "IMPRA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_IMPRESSIVE].iconState = floatyIconInfo->FindStateByString( "Impressive", true );
	g_Medals[MEDAL_IMPRESSIVE].text = "Impressive!";
	g_Medals[MEDAL_IMPRESSIVE].textColor = CR_GREY;
	g_Medals[MEDAL_IMPRESSIVE].announcerEntry = "Impressive";
	g_Medals[MEDAL_IMPRESSIVE].lowerMedal = nullptr;

	// Most impressive
	g_Medals[MEDAL_MOSTIMPRESSIVE].icon = TexMan.CheckForTexture( "MIMPA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_MOSTIMPRESSIVE].iconState = floatyIconInfo->FindStateByString( "Most_Impressive", true );
	g_Medals[MEDAL_MOSTIMPRESSIVE].text = "Most impressive!";
	g_Medals[MEDAL_MOSTIMPRESSIVE].textColor = CR_RED;
	g_Medals[MEDAL_MOSTIMPRESSIVE].announcerEntry = "MostImpressive";
	g_Medals[MEDAL_MOSTIMPRESSIVE].lowerMedal = &g_Medals[MEDAL_IMPRESSIVE];

	// Domination
	g_Medals[MEDAL_DOMINATION].icon = TexMan.CheckForTexture( "DOMNA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_DOMINATION].iconState = floatyIconInfo->FindStateByString( "Domination", true );
	g_Medals[MEDAL_DOMINATION].text = "Domination!";
	g_Medals[MEDAL_DOMINATION].textColor = CR_GREY;
	g_Medals[MEDAL_DOMINATION].announcerEntry = "Domination";
	g_Medals[MEDAL_DOMINATION].lowerMedal = nullptr;

	// Total domination
	g_Medals[MEDAL_TOTALDOMINATION].icon = TexMan.CheckForTexture( "TDOMA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_TOTALDOMINATION].iconState = floatyIconInfo->FindStateByString( "Total_Domination", true );
	g_Medals[MEDAL_TOTALDOMINATION].text = "Total domination!";
	g_Medals[MEDAL_TOTALDOMINATION].textColor = CR_RED;
	g_Medals[MEDAL_TOTALDOMINATION].announcerEntry = "TotalDomination";
	g_Medals[MEDAL_TOTALDOMINATION].lowerMedal = &g_Medals[MEDAL_DOMINATION];

	// Accuracy
	g_Medals[MEDAL_ACCURACY].icon = TexMan.CheckForTexture( "ACCUA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_ACCURACY].iconState = floatyIconInfo->FindStateByString( "Accuracy", true );
	g_Medals[MEDAL_ACCURACY].text = "Accuracy!";
	g_Medals[MEDAL_ACCURACY].textColor = CR_GREY;
	g_Medals[MEDAL_ACCURACY].announcerEntry = "Accuracy";
	g_Medals[MEDAL_ACCURACY].lowerMedal = nullptr;

	// Precision
	g_Medals[MEDAL_PRECISION].icon = TexMan.CheckForTexture( "PRECA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_PRECISION].iconState = floatyIconInfo->FindStateByString( "Precision", true );
	g_Medals[MEDAL_PRECISION].text = "Precision!";
	g_Medals[MEDAL_PRECISION].textColor = CR_RED;
	g_Medals[MEDAL_PRECISION].announcerEntry = "Precision";
	g_Medals[MEDAL_PRECISION].lowerMedal = &g_Medals[MEDAL_ACCURACY];

	// You fail it
	g_Medals[MEDAL_YOUFAILIT].icon = TexMan.CheckForTexture( "FAILA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_YOUFAILIT].iconState = floatyIconInfo->FindStateByString( "YouFailIt", true );
	g_Medals[MEDAL_YOUFAILIT].text = "You fail it!";
	g_Medals[MEDAL_YOUFAILIT].textColor = CR_GREEN;
	g_Medals[MEDAL_YOUFAILIT].announcerEntry = "YouFailIt";
	g_Medals[MEDAL_YOUFAILIT].lowerMedal = nullptr;

	// Your skill is not enough
	g_Medals[MEDAL_YOURSKILLISNOTENOUGH].icon = TexMan.CheckForTexture( "SKILA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_YOURSKILLISNOTENOUGH].iconState = floatyIconInfo->FindStateByString( "YourSkillIsNotEnough", true );
	g_Medals[MEDAL_YOURSKILLISNOTENOUGH].text = "Your skill is not enough!";
	g_Medals[MEDAL_YOURSKILLISNOTENOUGH].textColor = CR_ORANGE;
	g_Medals[MEDAL_YOURSKILLISNOTENOUGH].announcerEntry = "YourSkillIsNotEnough";
	g_Medals[MEDAL_YOURSKILLISNOTENOUGH].lowerMedal = &g_Medals[MEDAL_YOUFAILIT];

	// Llama
	g_Medals[MEDAL_LLAMA].icon = TexMan.CheckForTexture( "LLAMA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_LLAMA].iconState = floatyIconInfo->FindStateByString( "Llama", true );
	g_Medals[MEDAL_LLAMA].text = "Llama!";
	g_Medals[MEDAL_LLAMA].textColor = CR_GREEN;
	g_Medals[MEDAL_LLAMA].announcerEntry = "Llama";
	g_Medals[MEDAL_LLAMA].lowerMedal = nullptr;
	g_Medals[MEDAL_LLAMA].sound = "misc/llama";

	// Spam
	g_Medals[MEDAL_SPAM].icon = TexMan.CheckForTexture( "SPAMA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_SPAM].iconState = floatyIconInfo->FindStateByString( "Spam", true );
	g_Medals[MEDAL_SPAM].text = "Spam!";
	g_Medals[MEDAL_SPAM].textColor = CR_GREEN;
	g_Medals[MEDAL_SPAM].announcerEntry = "Spam";
	g_Medals[MEDAL_SPAM].lowerMedal = &g_Medals[MEDAL_LLAMA];
	g_Medals[MEDAL_SPAM].sound = "misc/spam";

	// Victory
	g_Medals[MEDAL_VICTORY].icon = TexMan.CheckForTexture( "VICTA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_VICTORY].iconState = floatyIconInfo->FindStateByString( "Victory", true );
	g_Medals[MEDAL_VICTORY].text = "Victory!";
	g_Medals[MEDAL_VICTORY].textColor = CR_GREY;
	g_Medals[MEDAL_VICTORY].announcerEntry = "Victory";
	g_Medals[MEDAL_VICTORY].lowerMedal = nullptr;

	// Perfect
	g_Medals[MEDAL_PERFECT].icon = TexMan.CheckForTexture( "PFCTA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_PERFECT].iconState = floatyIconInfo->FindStateByString( "Perfect", true );
	g_Medals[MEDAL_PERFECT].text = "Perfect!";
	g_Medals[MEDAL_PERFECT].textColor = CR_RED;
	g_Medals[MEDAL_PERFECT].announcerEntry = "Perfect";
	g_Medals[MEDAL_PERFECT].lowerMedal = &g_Medals[MEDAL_VICTORY];

	// Termination
	g_Medals[MEDAL_TERMINATION].icon = TexMan.CheckForTexture( "TRMAA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_TERMINATION].iconState = floatyIconInfo->FindStateByString( "Termination", true );
	g_Medals[MEDAL_TERMINATION].text = "Termination!";
	g_Medals[MEDAL_TERMINATION].textColor = CR_GREY;
	g_Medals[MEDAL_TERMINATION].announcerEntry = "Termination";
	g_Medals[MEDAL_TERMINATION].lowerMedal = nullptr;

	// First frag
	g_Medals[MEDAL_FIRSTFRAG].icon = TexMan.CheckForTexture( "FFRGA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_FIRSTFRAG].iconState = floatyIconInfo->FindStateByString( "FirstFrag", true );
	g_Medals[MEDAL_FIRSTFRAG].text = "First frag!";
	g_Medals[MEDAL_FIRSTFRAG].textColor = CR_GREY;
	g_Medals[MEDAL_FIRSTFRAG].announcerEntry = "FirstFrag";
	g_Medals[MEDAL_FIRSTFRAG].lowerMedal = nullptr;

	// Capture
	g_Medals[MEDAL_CAPTURE].icon = TexMan.CheckForTexture( "CAPTA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_CAPTURE].iconState = floatyIconInfo->FindStateByString( "Capture", true );
	g_Medals[MEDAL_CAPTURE].text = "Capture!";
	g_Medals[MEDAL_CAPTURE].textColor = CR_GREY;
	g_Medals[MEDAL_CAPTURE].announcerEntry = "Capture";
	g_Medals[MEDAL_CAPTURE].lowerMedal = nullptr;

	// Tag
	g_Medals[MEDAL_TAG].icon = TexMan.CheckForTexture( "STAGA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_TAG].iconState = floatyIconInfo->FindStateByString( "Tag", true );
	g_Medals[MEDAL_TAG].text = "Tag!";
	g_Medals[MEDAL_TAG].textColor = CR_GREY;
	g_Medals[MEDAL_TAG].announcerEntry = "Tag";
	g_Medals[MEDAL_TAG].lowerMedal = nullptr;

	// Assist
	g_Medals[MEDAL_ASSIST].icon = TexMan.CheckForTexture( "ASSTA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_ASSIST].iconState = floatyIconInfo->FindStateByString( "Assist", true );
	g_Medals[MEDAL_ASSIST].text = "Assist!";
	g_Medals[MEDAL_ASSIST].textColor = CR_GREY;
	g_Medals[MEDAL_ASSIST].announcerEntry = "Assist";
	g_Medals[MEDAL_ASSIST].lowerMedal = nullptr;

	// Defense
	g_Medals[MEDAL_DEFENSE].icon = TexMan.CheckForTexture( "DFNSA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_DEFENSE].iconState = floatyIconInfo->FindStateByString( "Defense", true );
	g_Medals[MEDAL_DEFENSE].text = "Defense!";
	g_Medals[MEDAL_DEFENSE].textColor = CR_GREY;
	g_Medals[MEDAL_DEFENSE].announcerEntry = "Defense";
	g_Medals[MEDAL_DEFENSE].lowerMedal = nullptr;

	// Fisting
	g_Medals[MEDAL_FISTING].icon = TexMan.CheckForTexture( "FISTA0", FTexture::TEX_MiscPatch );
	g_Medals[MEDAL_FISTING].iconState = floatyIconInfo->FindStateByString( "Fisting", true );
	g_Medals[MEDAL_FISTING].text = "Fisting!";
	g_Medals[MEDAL_FISTING].textColor = CR_GREY;
	g_Medals[MEDAL_FISTING].announcerEntry = "Fisting";
	g_Medals[MEDAL_FISTING].lowerMedal = nullptr;

	g_bFirstFragAwarded = false;
}

//*****************************************************************************
//
void MEDAL_Tick( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// No need to do anything.
		if ( playeringame[ulIdx] == false )
			continue;

		// Tick down the duration of the medal on the top of the queue. If time
		// has expired on this medal, pop it and potentially trigger a new one.
		if (( medalQueue[ulIdx].medals.empty( ) == false ) && ( medalQueue[ulIdx].ticks ) && ( --medalQueue[ulIdx].ticks == 0 ))
		{
			medalQueue[ulIdx].medals.erase( medalQueue[ulIdx].medals.begin( ));

			// If a new medal is now at the top of the queue, trigger it.
			if ( medalQueue[ulIdx].medals.empty( ) == false )
			{
				medalQueue[ulIdx].ticks = MEDAL_ICON_DURATION;
				medal_TriggerMedal( ulIdx );
			}
			// If there isn't, just delete the medal that has been displaying.
			else if ( players[ulIdx].pIcon != nullptr )
			{
				players[ulIdx].pIcon->Destroy( );
				players[ulIdx].pIcon = nullptr;
			}
		}

		// [BB] We don't need to know what medal_GetDesiredIcon puts into pTeamItem, but we still need to supply it as argument.
		AInventory *pTeamItem;
		const ULONG ulDesiredSprite = medal_GetDesiredIcon( &players[ulIdx], pTeamItem );

		// If we're not currently displaying a medal for the player, potentially display
		// some other type of icon.
		// [BB] Also let carrier icons override medals.
		if (( medalQueue[ulIdx].medals.empty( )) || (( ulDesiredSprite >= SPRITE_WHITEFLAG ) && ( ulDesiredSprite <= SPRITE_TEAMITEM )))
			medal_SelectIcon( &players[ulIdx] );

		// [BB] If the player is being awarded a medal at the moment but has no icon, restore the medal.
		// This happens when the player respawns while being awarded a medal.
		if (( medalQueue[ulIdx].medals.empty( ) == false ) && ( players[ulIdx].pIcon == nullptr ))
			medal_TriggerMedal( ulIdx );

		// [BB] Remove any old carrier icons.
		medal_PlayerHasCarrierIcon( &players[ulIdx] );

		// Don't render icons floating above our own heads.
		if ( players[ulIdx].pIcon )
		{
			if (( players[ulIdx].mo->CheckLocalView( consoleplayer )) && (( players[ulIdx].cheats & CF_CHASECAM ) == false ))
				players[ulIdx].pIcon->renderflags |= RF_INVISIBLE;
			else
				players[ulIdx].pIcon->renderflags &= ~RF_INVISIBLE;
		}
	}
}

//*****************************************************************************
//
void MEDAL_Render( void )
{
	if ( players[consoleplayer].camera == NULL )
		return;

	player_t *pPlayer = players[consoleplayer].camera->player;
	if ( pPlayer == NULL )
		return;

	ULONG ulPlayer = pPlayer - players;

	// [TP] Sanity check
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// If the player doesn't have a medal to be drawn, don't do anything.
	if ( medalQueue[ulPlayer].medals.empty( ))
		return;

	const MEDAL_t *medal = medalQueue[ulPlayer].medals[0];
	const LONG lAlpha = medalQueue[ulPlayer].ticks > TICRATE ? OPAQUE : static_cast<LONG>( OPAQUE * ( static_cast<float>( medalQueue[ulPlayer].ticks ) / TICRATE ));

	// Get the graphic and text name from the global array.
	FTexture *icon = TexMan[medal->icon];
	FString string = medal->text.GetChars( );

	ULONG ulCurXPos = SCREENWIDTH / 2;
	ULONG ulCurYPos = ( viewheight <= ST_Y ? ST_Y : SCREENHEIGHT ) - 11 * CleanYfac;

	// Determine how much actual screen space it will take to render the amount of
	// medals the player has received up until this point.
	ULONG ulLength = medal->awardedCount[ulPlayer] * icon->GetWidth( );

	// If that length is greater then the screen width, display the medals as "<icon> <name> X <num>"
	if ( ulLength >= 320 )
	{
		const char *szSecondColor = medal->textColor == CR_RED ? TEXTCOLOR_GRAY : TEXTCOLOR_RED;

		string.AppendFormat( "%s X %u", szSecondColor, medal->awardedCount[ulPlayer] );
		screen->DrawTexture( icon, ulCurXPos, ulCurYPos, DTA_CleanNoMove, true, DTA_Alpha, lAlpha, TAG_DONE );

		ulCurXPos -= CleanXfac * ( SmallFont->StringWidth( string ) / 2 );
		screen->DrawText( SmallFont, medal->textColor, ulCurXPos, ulCurYPos, string, DTA_CleanNoMove, true, DTA_Alpha, lAlpha, TAG_DONE );
	}
	// Display the medal icon <usNumMedals> times centered on the screen.
	else
	{
		ulCurXPos -= ( CleanXfac * ulLength ) / 2;

		for ( ULONG ulMedal = 0; ulMedal < medal->awardedCount[ulPlayer]; ulMedal++ )
		{
			screen->DrawTexture( icon, ulCurXPos + CleanXfac * ( icon->GetWidth( ) / 2 ), ulCurYPos, DTA_CleanNoMove, true, DTA_Alpha, lAlpha, TAG_DONE );
			ulCurXPos += CleanXfac * icon->GetWidth( );
		}

		ulCurXPos = ( SCREENWIDTH - CleanXfac * SmallFont->StringWidth( string )) / 2;
		screen->DrawText( SmallFont, medal->textColor, ulCurXPos, ulCurYPos, string, DTA_CleanNoMove, true, DTA_Alpha, lAlpha, TAG_DONE );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void MEDAL_GiveMedal( const ULONG player, const ULONG medalIndex )
{
	// [CK] Do not award if it's a countdown sequence
	// [AK] Or if we're playing a cooperative game mode.
	if (( GAMEMODE_IsGameInCountdown( )) || (( deathmatch || teamgame ) == false ))
		return;

	// [AK] Make sure that the player and medal are valid.
	if (( player >= MAXPLAYERS ) || ( players[player].mo == nullptr ) || ( medalIndex >= NUM_MEDALS ))
		return;

	// [AK] Make sure that medals are allowed.
	if ((( NETWORK_GetState( ) != NETSTATE_SERVER ) && ( cl_medals == false )) || ( zadmflags & ZADF_NO_MEDALS ))
		return;

	MEDAL_t *const medal = &g_Medals[medalIndex];

	// [CK] Trigger events if a medal is received
	// [AK] If the event returns 0, then the player doesn't receive the medal.
	if ( GAMEMODE_HandleEvent( GAMEEVENT_MEDALS, players[player].mo, ACS_PushAndReturnDynamicString( medal->announcerEntry ), 0, true ) == 0 )
		return;

	// Increase the player's count of this type of medal.
	medal->awardedCount[player]++;

	// [AK] Check if the medal being give is already in this player's queue.
	std::vector<MEDAL_t *> &queue = medalQueue[player].medals;
	auto iterator = std::find( queue.begin( ), queue.end( ), medal );

	// [AK] If not, then check if a suboordinate of the new medal is already in
	// the list. If so, then the lower medal will be replaced. Otherwise, the new
	// medal gets added to the end of the queue.
	if ( iterator == queue.end( ))
	{
		iterator = std::find( queue.begin( ), queue.end( ), medal->lowerMedal );

		if ( iterator != queue.end( ))
		{
			*iterator = medal;
		}
		else
		{
			queue.push_back( medal );

			// [AK] In case the queue was empty before (there's only one element
			// now, which is what just got added), set the iterator to the start
			// so the timer gets reset properly.
			if ( queue.size( ) == 1 )
				iterator = queue.begin( );
		}
	}

	// [AK] If the new medal is at the start. reset the timer and trigger it.
	if ( iterator == queue.begin( ))
	{
		medalQueue[player].ticks = MEDAL_ICON_DURATION;
		medal_TriggerMedal( player );
	}

	// If this player is a bot, tell it that it received a medal.
	if ( players[player].pSkullBot )
	{
		players[player].pSkullBot->m_ulLastMedalReceived = medalIndex;
		players[player].pSkullBot->PostEvent( BOTEVENT_RECEIVEDMEDAL );
	}

	// [AK] If we're the server, tell clients that this player earned a medal.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_GivePlayerMedal( player, medalIndex );
}

//*****************************************************************************
//
void MEDAL_RenderAllMedals( LONG lYOffset )
{
	ULONG ulCurXPos;
	FTexture *icon;

	if (( players[consoleplayer].camera == nullptr ) || ( players[consoleplayer].camera->player == nullptr ))
		return;

	const unsigned int player = players[consoleplayer].camera->player - players;

	int y0 = ( viewheight <= ST_Y ? ST_Y : SCREENHEIGHT );
	ULONG ulCurYPos = static_cast<ULONG>(( y0 - 11 * CleanYfac + lYOffset ) / CleanYfac );

	// Determine length of all medals strung together.
	ULONG ulLength = 0;
	for ( ULONG ulMedal = 0; ulMedal < NUM_MEDALS; ulMedal++ )
	{
		if ( g_Medals[ulMedal].awardedCount[player] > 0 )
			ulLength += TexMan[g_Medals[ulMedal].icon]->GetWidth( ) * g_Medals[ulMedal].awardedCount[player];
	}

	// Can't fit all the medals on the screen.
	if ( ulLength >= 320 )
	{
		FString	string;

		// Recalculate the length of all the medals strung together.
		ulLength = 0;
		for ( ULONG ulMedal = 0; ulMedal < NUM_MEDALS; ulMedal++ )
		{
			if ( g_Medals[ulMedal].awardedCount[player] > 0 )
				ulLength += TexMan[g_Medals[ulMedal].icon]->GetWidth( );
		}

		// If the length of all our medals goes beyond 320, we cannot scale them.
		bool bScale = ( ulLength >= 320 );

		if ( bScale )
			ulCurYPos = static_cast<ULONG>( ulCurYPos * CleanYfac );

		ulCurXPos = (( bScale ? 320 : SCREENWIDTH ) - ulLength ) / 2;
		for ( ULONG ulMedal = 0; ulMedal < NUM_MEDALS; ulMedal++ )
		{
			if ( g_Medals[ulMedal].awardedCount[player] == 0 )
				continue;

			icon = TexMan[g_Medals[ulMedal].icon];
			screen->DrawTexture( icon, ulCurXPos + icon->GetWidth( ) / 2, ulCurYPos, DTA_Clean, bScale, TAG_DONE );

			ULONG ulXOffset = ( SmallFont->StringWidth( string ) + icon->GetWidth( )) / 2;
			string.Format( "%u", g_Medals[ulMedal].awardedCount[player] );
			screen->DrawText( SmallFont, CR_RED, ulCurXPos - ulXOffset, ulCurYPos, string, DTA_Clean, bScale, TAG_DONE );

			ulCurXPos += icon->GetWidth( );
		}
	}
	else
	{
		ulCurXPos = 160 - ulLength / 2;
		for ( ULONG ulMedal = 0; ulMedal < NUM_MEDALS; ulMedal++ )
		{
			icon = TexMan[g_Medals[ulMedal].icon];

			for ( ULONG ulMedalIdx = 0; ulMedalIdx < g_Medals[ulMedal].awardedCount[player]; ulMedalIdx++ )
			{
				screen->DrawTexture( icon, ulCurXPos + icon->GetWidth( ) / 2, ulCurYPos, DTA_Clean, true, TAG_DONE );
				ulCurXPos += icon->GetWidth( );
			}
		}
	}
}

//*****************************************************************************
//
void MEDAL_RenderAllMedalsFullscreen( player_t *pPlayer )
{
	ULONG ulCurXPos;
	ULONG ulCurYPos = 4;
	FString string;

	if ( pPlayer == nullptr )
		return;

	const unsigned int playerIndex = pPlayer - players;

	// Start by drawing "MEDALS" 4 pixels from the top.
	HUD_DrawTextCentered( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED, ulCurYPos, "MEDALS", g_bScale );
	ulCurYPos += BigFont->GetHeight( ) + 30;

	ULONG ulNumMedal = 0;
	ULONG ulMaxMedalHeight = 0;
	ULONG ulLastHeight = 0;

	for ( ULONG ulMedal = 0; ulMedal < NUM_MEDALS; ulMedal++ )
	{
		if ( g_Medals[ulMedal].awardedCount[playerIndex] == 0 )
			continue;

		ULONG ulHeight = TexMan[g_Medals[ulMedal].icon]->GetHeight( );

		if (( ulNumMedal % 2 ) == 0 )
		{
			ulCurXPos = static_cast<ULONG>( 40.0f * ( g_bScale ? g_fXScale : CleanXfac ));
			ulLastHeight = ulHeight;
		}
		else
		{
			ulCurXPos += HUD_GetWidth( ) / 2;
			ulMaxMedalHeight = MAX( ulHeight, ulLastHeight );
		}

		HUD_DrawTexture( TexMan[g_Medals[ulMedal].icon], ulCurXPos + TexMan[g_Medals[ulMedal].icon]->GetWidth( ) / 2, ulCurYPos + ulHeight, g_bScale );
		HUD_DrawText( SmallFont, CR_RED, ulCurXPos + 48, ulCurYPos + ( ulHeight - SmallFont->GetHeight( )) / 2, "X" );

		string.Format( "%u", g_Medals[ulMedal].awardedCount[playerIndex] );
		HUD_DrawText( BigFont, CR_RED, ulCurXPos + 64, ulCurYPos + ( ulHeight - BigFont->GetHeight( )) / 2, string );

		if ( ulNumMedal % 2 )
			ulCurYPos += ulMaxMedalHeight;

		ulNumMedal++;
	}

	// [CK] Update the names as well
	if ( pPlayer - &players[consoleplayer] == 0 )
		string = "You have";
	else
		string.Format( "%s has", pPlayer->userinfo.GetName( ));

	// The player has not earned any medals, so nothing was drawn.
	if ( ulNumMedal == 0 )
		string += " not yet earned any medals.";
	else
		string += " earned the following medals:";

	HUD_DrawTextCentered( SmallFont, CR_WHITE, BigFont->GetHeight( ) + 14, string, g_bScale );
}

//*****************************************************************************
//
MEDAL_t *MEDAL_GetDisplayedMedal( const ULONG player )
{
	if (( player < MAXPLAYERS ) && ( medalQueue[player].medals.empty( ) == false ))
		return medalQueue[player].medals[0];

	return nullptr;
}

//*****************************************************************************
//
void MEDAL_ResetPlayerMedals( const ULONG player )
{
	if ( player >= MAXPLAYERS )
		return;

	// Reset the number of medals this player has.
	for ( unsigned int i = 0; i < NUM_MEDALS; i++ )
		g_Medals[i].awardedCount[player] = 0;

	medalQueue[player].medals.clear( );
	medalQueue[player].ticks = 0;
}

//*****************************************************************************
//
void MEDAL_PlayerDied( ULONG ulPlayer, ULONG ulSourcePlayer, int dmgflags )
{
	if ( PLAYER_IsValidPlayerWithMo ( ulPlayer ) == false )
		return;

	// Check for domination and first frag medals.
	if ( PLAYER_IsValidPlayerWithMo ( ulSourcePlayer ) &&
		( players[ulSourcePlayer].mo->IsTeammate( players[ulPlayer].mo ) == false ) &&
		// [Dusk] As players do not get frags for spawn telefrags, they shouldn't get medals for that either
		( MeansOfDeath != NAME_SpawnTelefrag ))
	{
		players[ulSourcePlayer].ulFragsWithoutDeath++;
		players[ulSourcePlayer].ulDeathsWithoutFrag = 0;

		medal_CheckForFirstFrag( ulSourcePlayer );
		medal_CheckForDomination( ulSourcePlayer );
		medal_CheckForFistingOrSpam( ulSourcePlayer, dmgflags );
		medal_CheckForExcellent( ulSourcePlayer );
		medal_CheckForTermination( ulPlayer, ulSourcePlayer );
		medal_CheckForLlama( ulPlayer, ulSourcePlayer );

		players[ulSourcePlayer].ulLastFragTick = level.time;
	}

	players[ulPlayer].ulFragsWithoutDeath = 0;

	// [BB] Don't punish being killed by a teammate (except if a player kills himself).
	if ( ( ulPlayer == ulSourcePlayer )
		|| ( PLAYER_IsValidPlayerWithMo ( ulSourcePlayer ) == false )
		|| ( players[ulSourcePlayer].mo->IsTeammate( players[ulPlayer].mo ) == false ) )
	{
		players[ulPlayer].ulDeathsWithoutFrag++;
		medal_CheckForYouFailIt( ulPlayer );
	}
}

//*****************************************************************************
//
void MEDAL_ResetFirstFragAwarded( void )
{
	g_bFirstFragAwarded = false;
}

//*****************************************************************************
//*****************************************************************************
//

//*****************************************************************************
// [BB, RC] Returns whether the player wears a carrier icon (flag/skull/hellstone/etc) and removes any invalid ones.
//
bool medal_PlayerHasCarrierIcon( player_t *player )
{
	bool invalid = false;
	bool hasIcon = true;

	// [BB] If the player has no icon, he obviously doesn't have a carrier icon.
	if (( player == nullptr ) || ( player->pIcon == nullptr ))
		return false;

	// Verify that our current icon is valid.
	if ( player->pIcon->bTeamItemFloatyIcon == false )
	{
		switch ( player->pIcon->currentSprite )
		{
			// White flag icon. Delete it if the player no longer has it.
			case SPRITE_WHITEFLAG:
			{
				// Delete the icon if teamgame has been turned off, or if the player
				// is not on a team.
				if (( teamgame == false ) || ( player->bOnTeam == false ))
					invalid = true;
				// Delete the white flag if the player no longer has it.
				else if ( player->mo->FindInventory( PClass::FindClass( "WhiteFlag" ), true ) == nullptr )
					invalid = true;

				break;
			}

			// Terminator artifact icon. Delete it if the player no longer has it.
			case SPRITE_TERMINATORARTIFACT:
			{
				if (( terminator == false ) || (( player->cheats2 & CF2_TERMINATORARTIFACT ) == false ))
					invalid = true;

				break;
			}

			// Possession artifact icon. Delete it if the player no longer has it.
			case SPRITE_POSSESSIONARTIFACT:
			{
				if ((( possession == false ) && ( teampossession == false )) || (( player->cheats2 & CF2_POSSESSIONARTIFACT ) == false ))
					invalid = true;

				break;
			}

			default:
				hasIcon = false;
				break;
		}
	}
	else if ( GAMEMODE_GetCurrentFlags( ) & GMF_USETEAMITEM )
	{
		invalid = ( TEAM_FindOpposingTeamsItemInPlayersInventory( player ) == nullptr );
	}

	// Remove it.
	if (( invalid ) && ( hasIcon ))
	{
		player->pIcon->Destroy( );
		player->pIcon = NULL;

		medal_TriggerMedal( player - players );
	}

	return ( hasIcon && !invalid );
}

//*****************************************************************************
//
void medal_TriggerMedal( ULONG ulPlayer )
{
	player_t	*pPlayer;

	pPlayer = &players[ulPlayer];

	// Servers don't actually spawn medals.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	// Make sure this player is valid and they have a medal in their queue.
	if (( pPlayer->mo == NULL ) || ( medalQueue[ulPlayer].medals.empty( )))
		return;

	const MEDAL_t *const medal = medalQueue[ulPlayer].medals[0];

	// Medals don't override carrier symbols.
	if ( !medal_PlayerHasCarrierIcon( pPlayer ))
	{
		if ( pPlayer->pIcon )
			pPlayer->pIcon->Destroy( );

		// Spawn the medal as an icon above the player and set its properties.
		pPlayer->pIcon = Spawn<AFloatyIcon>( pPlayer->mo->x, pPlayer->mo->y, pPlayer->mo->z, NO_REPLACE );
		if ( pPlayer->pIcon )
		{
			pPlayer->pIcon->SetState( medal->iconState );
			// [BB] Instead of MEDAL_ICON_DURATION only use the remaining ticks of the medal as ticks for the icon.
			// It is possible that the medal is just restored because the player respawned or that the medal was
			// suppressed by a carrier icon.
			pPlayer->pIcon->lTick = medalQueue[ulPlayer].ticks;
			pPlayer->pIcon->SetTracer( pPlayer->mo );
		}
	}

	// [BB] Only announce the medal when it reaches the top of the queue. Otherwise it could be
	// announced multiple times (for instance when a carrier dies).
	if ( medalQueue[ulPlayer].ticks == MEDAL_ICON_DURATION )
	{
		// Also, locally play the announcer sound associated with this medal.
		// [Dusk] Check coop spy too
		if ( pPlayer->mo->CheckLocalView( consoleplayer ) )
		{
			if ( medal->announcerEntry.IsNotEmpty( ))
				ANNOUNCER_PlayEntry( cl_announcer, medal->announcerEntry.GetChars( ));
		}
		// If a player besides the console player got the medal, play the remote sound.
		else
		{
			// Play the sound effect associated with this medal type.
			if ( medal->sound > 0 )
				S_Sound( pPlayer->mo, CHAN_AUTO, medal->sound, 1.0f, ATTN_NORM );
		}
	}
}

//*****************************************************************************
//
ULONG medal_GetDesiredIcon( player_t *pPlayer, AInventory *&pTeamItem )
{
	ULONG ulDesiredSprite = NUM_SPRITES;

	// [BB] Invalid players certainly don't need any icon.
	if ( ( pPlayer == NULL ) || ( pPlayer->mo == NULL ) )
		return NUM_SPRITES;

	// Draw an ally icon if this person is on our team. Would this be useful for co-op, too?
	// [BB] In free spectate mode, we don't have allies (and SCOREBOARD_GetViewPlayer doesn't return a useful value).
	if ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) && ( CLIENTDEMO_IsInFreeSpectateMode() == false ) )
	{
		// [BB] Dead spectators shall see the icon for their teammates.
		if ( pPlayer->mo->IsTeammate( players[HUD_GetViewPlayer( )].mo ) && !PLAYER_IsTrueSpectator( &players[HUD_GetViewPlayer( )] ) )
			ulDesiredSprite = SPRITE_ALLY;
	}

	// Draw a chat icon over the player if they're typing.
	if ( pPlayer->statuses & PLAYERSTATUS_CHATTING )
		ulDesiredSprite = SPRITE_CHAT;

	// Draw a console icon over the player if they're in the console.
	if ( pPlayer->statuses & PLAYERSTATUS_INCONSOLE )
		ulDesiredSprite = SPRITE_INCONSOLE;

	// Draw a menu icon over the player if they're in the Menu.
	if ( pPlayer->statuses & PLAYERSTATUS_INMENU )
		ulDesiredSprite = SPRITE_INMENU;

	// Draw a speaker icon over the player if they're talking.
	if ( pPlayer->statuses & PLAYERSTATUS_TALKING )
		ulDesiredSprite = SPRITE_VOICECHAT;

	// Draw a lag icon over their head if they're lagging.
	if ( pPlayer->statuses & PLAYERSTATUS_LAGGING )
		ulDesiredSprite = SPRITE_LAG;

	// Draw a flag/skull above this player if he's carrying one.
	if ( GAMEMODE_GetCurrentFlags() & GMF_USETEAMITEM )
	{
		if ( pPlayer->bOnTeam )
		{
			if ( oneflagctf )
			{
				AInventory *pInventory = pPlayer->mo->FindInventory( PClass::FindClass( "WhiteFlag" ), true );
				if ( pInventory )
					ulDesiredSprite = SPRITE_WHITEFLAG;
			}

			else
			{
				pTeamItem = TEAM_FindOpposingTeamsItemInPlayersInventory ( pPlayer );
				if ( pTeamItem )
					ulDesiredSprite = SPRITE_TEAMITEM;
			}
		}
	}

	// Draw the terminator artifact over the terminator.
	if ( terminator && ( pPlayer->cheats2 & CF2_TERMINATORARTIFACT ))
		ulDesiredSprite = SPRITE_TERMINATORARTIFACT;

	// Draw the possession artifact over the player.
	if (( possession || teampossession ) && ( pPlayer->cheats2 & CF2_POSSESSIONARTIFACT ))
		ulDesiredSprite = SPRITE_POSSESSIONARTIFACT;

	return ulDesiredSprite;
}

//*****************************************************************************
//
void medal_SelectIcon( player_t *player )
{
	// [BB] If player carries a TeamItem, e.g. flag or skull, we store a pointer
	// to it in teamItem and set the floaty icon to the carry (or spawn) state of
	// the TeamItem. We also need to copy the Translation of the TeamItem to the
	// FloatyIcon.
	AInventory	*teamItem = nullptr;

	if (( player == nullptr ) || ( player->mo == nullptr ))
		return;

	// Allow the user to disable icons.
	if (( cl_icons == false ) || ( NETWORK_GetState( ) == NETSTATE_SERVER ) || ( player->bSpectating ))
	{
		if ( player->pIcon )
		{
			player->pIcon->Destroy( );
			player->pIcon = nullptr;
		}

		return;
	}

	// Verify that our current icon is valid. (i.e. We may have had a chat bubble, then
	// stopped talking, so we need to delete it).
	if ( player->pIcon )
	{
		bool deleteIcon = false;

		if ( player->pIcon->bTeamItemFloatyIcon == false )
		{
			switch ( player->pIcon->currentSprite )
			{
				// Chat icon. Delete it if the player is no longer talking.
				case SPRITE_CHAT:
				{
					if (( player->statuses & PLAYERSTATUS_CHATTING ) == false )
						deleteIcon = true;

					break;
				}

				// Voice chat icon. Delete it if the player is no longer talking.
				case SPRITE_VOICECHAT:
				{
					if (( player->statuses & PLAYERSTATUS_TALKING ) == false )
						deleteIcon = true;

					break;
				}

				// In console icon. Delete it if the player is no longer in the console.
				case SPRITE_INCONSOLE:
				{
					if (( player->statuses & PLAYERSTATUS_INCONSOLE ) == false )
						deleteIcon = true;

					break;
				}

				// In menu icon . Delete it if the player is no longer in the menu.
				case SPRITE_INMENU:
				{
					if (( player->statuses & PLAYERSTATUS_INMENU ) == false )
						deleteIcon = true;

					break;
				}

				// Ally icon. Delete it if the player is now our enemy or if we're spectating.
				// [BB] Dead spectators shall keep the icon for their teammates.
				case SPRITE_ALLY:
				{
					player_t *viewedPlayer = &players[HUD_GetViewPlayer( )];

					if (( PLAYER_IsTrueSpectator( viewedPlayer )) || ( !player->mo->IsTeammate( viewedPlayer->mo )))
						deleteIcon = true;

					break;
				}

				// Lag icon. Delete it if the player is no longer lagging.
				case SPRITE_LAG:
				{
					if (( NETWORK_InClientMode( ) == false ) || (( player->statuses & PLAYERSTATUS_LAGGING ) == false ))
						deleteIcon = true;

					break;
				}


				// White flag icon. Delete it if the player no longer has it.
				case SPRITE_WHITEFLAG:
				{
					// Delete the icon if teamgame has been turned off, or if the player
					// is not on a team.
					if (( teamgame == false ) || ( player->bOnTeam == false ))
						deleteIcon = true;
					// Delete the white flag if the player no longer has it.
					else if (( oneflagctf ) && ( player->mo->FindInventory( PClass::FindClass( "WhiteFlag" ), true ) == nullptr ))
						deleteIcon = true;

					break;
				}

				// Terminator artifact icon. Delete it if the player no longer has it.
				case SPRITE_TERMINATORARTIFACT:
				{
					if (( terminator == false ) || (( player->cheats2 & CF2_TERMINATORARTIFACT ) == false ))
						deleteIcon = true;

					break;
				}

				// Possession artifact icon. Delete it if the player no longer has it.
				case SPRITE_POSSESSIONARTIFACT:
				{
					if ((( possession == false ) && ( teampossession == false )) || (( player->cheats2 & CF2_POSSESSIONARTIFACT ) == false ))
						deleteIcon = true;

					break;
				}

				default:
					break;
			}
		}
		else
		{
			// [AK] Team item icon. Delete it if the player no longer has one.
			if ((( GAMEMODE_GetCurrentFlags( ) & GMF_USETEAMITEM ) == false ) || ( player->bOnTeam == false ) ||
				( TEAM_FindOpposingTeamsItemInPlayersInventory( player ) == nullptr ))
			{
				deleteIcon = true;
			}
		}

		// We wish to delete the icon, so do that now.
		if ( deleteIcon )
		{
			player->pIcon->Destroy( );
			player->pIcon = nullptr;
		}
	}

	// Check if we need to have an icon above us, or change the current icon.
	const ULONG desiredSprite = medal_GetDesiredIcon( player, teamItem );
	const FActorInfo *floatyIconInfo = RUNTIME_CLASS( AFloatyIcon )->ActorInfo;
	FState *desiredState = nullptr;

	// [BB] Determine the frame based on the desired sprite.
	switch ( desiredSprite )
	{
		case SPRITE_CHAT:
			desiredState = floatyIconInfo->FindState( "Chat" );
			break;

		case SPRITE_VOICECHAT:
			desiredState = floatyIconInfo->FindState( "VoiceChat" );
			break;

		case SPRITE_INCONSOLE:
			desiredState = floatyIconInfo->FindState( "InConsole" );
			break;

		case SPRITE_INMENU:
			desiredState = floatyIconInfo->FindState( "InMenu" );
			break;

		case SPRITE_ALLY:
			desiredState = floatyIconInfo->FindState( "Ally" );
			break;

		case SPRITE_LAG:
			desiredState = floatyIconInfo->FindState( "Lag" );
			break;

		case SPRITE_WHITEFLAG:
			desiredState = floatyIconInfo->FindState( "WhiteFlag" );
			break;

		case SPRITE_TERMINATORARTIFACT:
			desiredState = floatyIconInfo->FindState( "TerminatorArtifact" );
			break;

		case SPRITE_POSSESSIONARTIFACT:
			desiredState = floatyIconInfo->FindState( "PossessionArtifact" );
			break;

		case SPRITE_TEAMITEM:
		{
			if ( teamItem )
			{
				FState *carryState = teamItem->FindState( "Carry" );

				// [BB] If the TeamItem has a Carry state (like the built in flags), use it.
				// Otherwise use the spawn state (the built in skulls don't have a carry state).
				desiredState = carryState ? carryState : teamItem->SpawnState;
			}

			break;
		}

		default:
			break;
	}

	// We have an icon that needs to be spawned.
	if ((( desiredState != nullptr ) && ( desiredSprite != NUM_SPRITES )))
	{
		// [BB] If a TeamItem icon replaces an existing non-team icon, we have to delete the old icon first.
		if (( player->pIcon ) && ( player->pIcon->bTeamItemFloatyIcon == false ) && ( teamItem ))
		{
			player->pIcon->Destroy( );
			player->pIcon = nullptr;
		}

		if (( player->pIcon == nullptr ) || ( desiredSprite != player->pIcon->currentSprite ))
		{
			if ( player->pIcon == NULL )
			{
				player->pIcon = Spawn<AFloatyIcon>( player->mo->x, player->mo->y, player->mo->z + player->mo->height + ( 4 * FRACUNIT ), NO_REPLACE );

				if ( teamItem )
				{
					player->pIcon->bTeamItemFloatyIcon = true;
					player->pIcon->Translation = teamItem->Translation;
				}
				else
				{
					player->pIcon->bTeamItemFloatyIcon = false;
				}
			}

			if ( player->pIcon )
			{
				// [BB] Potentially the new icon overrides an existing medal, so make sure that it doesn't fade out.
				player->pIcon->lTick = 0;
				player->pIcon->currentSprite = desiredSprite;
				player->pIcon->SetTracer( player->mo );
				player->pIcon->SetState( desiredState );
			}
		}
	}
}

//*****************************************************************************
//
void medal_CheckForFirstFrag( ULONG ulPlayer )
{
	// Only award it once.
	if ( g_bFirstFragAwarded )
		return;

	if (( deathmatch ) &&
		( lastmanstanding == false ) &&
		( teamlms == false ) &&
		( possession == false ) &&
		( teampossession == false ) &&
		(( duel == false ) || ( DUEL_GetState( ) == DS_INDUEL )))
	{
		MEDAL_GiveMedal( ulPlayer, MEDAL_FIRSTFRAG );

		// It's been given.
		g_bFirstFragAwarded = true;
	}
}

//*****************************************************************************
//
void medal_CheckForDomination( ULONG ulPlayer )
{
	// If the player has gotten 5 straight frags without dying, award a medal.
	// Award a "Total Domination" medal if they get 10+ straight frags without dying. Otherwise, award a "Domination" medal.
	if (( players[ulPlayer].ulFragsWithoutDeath % 5 ) == 0 )
		MEDAL_GiveMedal( ulPlayer, players[ulPlayer].ulFragsWithoutDeath >= 10 ? MEDAL_TOTALDOMINATION : MEDAL_DOMINATION );
}

//*****************************************************************************
//
void medal_CheckForFistingOrSpam( ULONG ulPlayer, int dmgflags )
{
	// [AK] Check if we should award the player with a "fisting" medal.
	if ( dmgflags & DMG_GIVE_FISTING_MEDAL_ON_FRAG )
		MEDAL_GiveMedal( ulPlayer, MEDAL_FISTING );

	// [AK] Check if we should award the player with a "spam" medal if this is the second
	// frag this player has gotten THIS TICK with a projectile or puff that awards one.
	if ( dmgflags & DMG_GIVE_SPAM_MEDAL_ON_FRAG )
	{
		if ( players[ulPlayer].ulLastSpamTick == static_cast<unsigned> (level.time) )
		{
			// Award the medal.
			MEDAL_GiveMedal( ulPlayer, MEDAL_SPAM );

			// Also, cancel out the possibility of getting an Excellent/Incredible medal.
			players[ulPlayer].ulLastExcellentTick = 0;
			players[ulPlayer].ulLastFragTick = 0;
		}
		else
			players[ulPlayer].ulLastSpamTick = level.time;
	}
}

//*****************************************************************************
//
void medal_CheckForExcellent( ULONG ulPlayer )
{
	// If the player has gotten two Excelents within two seconds, award an "Incredible" medal.
	// [BB] Check that the player actually got an excellent medal.
	if ( ( ( players[ulPlayer].ulLastExcellentTick + ( 2 * TICRATE )) > (ULONG)level.time ) && players[ulPlayer].ulLastExcellentTick )
	{
		// Award the incredible.
		MEDAL_GiveMedal( ulPlayer, MEDAL_INCREDIBLE );

		players[ulPlayer].ulLastExcellentTick = level.time;
		players[ulPlayer].ulLastFragTick = level.time;
	}
	// If this player has gotten two frags within two seconds, award an "Excellent" medal.
	// [BB] Check that the player actually got a frag.
	else if ( ( ( players[ulPlayer].ulLastFragTick + ( 2 * TICRATE )) > (ULONG)level.time ) && players[ulPlayer].ulLastFragTick )
	{
		// Award the excellent.
		MEDAL_GiveMedal( ulPlayer, MEDAL_EXCELLENT );

		players[ulPlayer].ulLastExcellentTick = level.time;
		players[ulPlayer].ulLastFragTick = level.time;
	}
}

//*****************************************************************************
//
void medal_CheckForTermination( ULONG ulDeadPlayer, ULONG ulPlayer )
{
	// If the target player is the terminatior, award a "termination" medal.
	if ( players[ulDeadPlayer].cheats2 & CF2_TERMINATORARTIFACT )
		MEDAL_GiveMedal( ulPlayer, MEDAL_TERMINATION );
}

//*****************************************************************************
//
void medal_CheckForLlama( ULONG ulDeadPlayer, ULONG ulPlayer )
{
	// Award a "llama" medal if the victim had been typing, lagging, or in the console.
	if ( players[ulDeadPlayer].statuses & ( PLAYERSTATUS_CHATTING | PLAYERSTATUS_INCONSOLE | PLAYERSTATUS_INMENU | PLAYERSTATUS_LAGGING ))
		MEDAL_GiveMedal( ulPlayer, MEDAL_LLAMA );
}

//*****************************************************************************
//
void medal_CheckForYouFailIt( ULONG ulPlayer )
{
	// If the player dies TEN times without getting a frag, award a "Your skill is not enough" medal.
	if (( players[ulPlayer].ulDeathsWithoutFrag % 10 ) == 0 )
		MEDAL_GiveMedal( ulPlayer, MEDAL_YOURSKILLISNOTENOUGH );
	// If the player dies five times without getting a frag, award a "You fail it" medal.
	else if (( players[ulPlayer].ulDeathsWithoutFrag % 5 ) == 0 )
		MEDAL_GiveMedal( ulPlayer, MEDAL_YOUFAILIT );
}

#ifdef	_DEBUG
#include "c_dispatch.h"
CCMD( testgivemedal )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < NUM_MEDALS; ulIdx++ )
		MEDAL_GiveMedal( consoleplayer, ulIdx );
}
#endif	// _DEBUG
