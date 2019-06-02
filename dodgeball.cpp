/*
 * Copyright (C) 2018 Vladimir "allejo" Jimenez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <map>
#include <random>

#include "bzfsAPI.h"
#include "plugin_utils.h"

// Define plugin name
const std::string PLUGIN_NAME = "Dodgeball";

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 1;
const int REV = 2;
const int BUILD = 20;

const int MAX_PLAYER_ID = 256;

class TeamJail : public bz_CustomZoneObject
{
public:
    typedef std::vector<TeamJail> Vector;

    TeamJail() : bz_CustomZoneObject() {}
};

class Dodgeball : public bz_Plugin, public bz_CustomMapObjectHandler
{
public:
    virtual const char* Name();
    virtual void Init(const char* config);
    virtual void Event(bz_EventData *eventData);
    virtual void Cleanup(void);

    virtual bool MapObject(bz_ApiString object, bz_CustomMapObjectInfo *data);

private:
    std::vector<bz_eTeamType> getAvailableTeams();

    bool isEntireTeamInJail(bz_eTeamType _team);
    bool safelyFreePrisoner(int playerID);
    void checkGameOver(void);
    void doGameOver(bz_eTeamType winner, bz_eTeamType loser);

    std::map<bz_eTeamType, TeamJail::Vector> TeamJails;
    std::map<bz_eTeamType, int> lastKill;

    bz_eTeamType TEAM_ONE = eNoTeam;
    bz_eTeamType TEAM_TWO = eNoTeam;

    /// When true, checking for "game over" status is disabled while the plug-in is cleaning itself. The plug-in checks
    /// for a "game over" state each time a player dies and since some players need to be killed to be freed from jail,
    /// this lock prevents the plug-in while triggering another "game over" while players are being freed.
    bool gameOverCheckLocked = false;

    /// A minimum of 2 players per team is required. This boolean keeps track of when there are enough players on each
    /// team to allow for a game of dodgeball to start.
    bool gameModeEnabled = false;

    /// An array to store whether or not a player ID is marked as being in jail.
    bool inJail[MAX_PLAYER_ID] = {false};
};

BZ_PLUGIN(Dodgeball)

const char* Dodgeball::Name(void)
{
    static std::string pluginName;

    if (pluginName.empty())
    {
        pluginName = bz_format("%s %d.%d.%d (%d)", PLUGIN_NAME.c_str(), MAJOR, MINOR, REV, BUILD);
    }

    return pluginName.c_str();
}

void Dodgeball::Init(const char* /*commandLine*/)
{
    if (bz_getGameType() != eCTFGame)
    {
        bz_debugMessagef(0, "ERROR :: %s :: The dodgeball game mode requires a CTF server. This plug-in will **fail** to load.", PLUGIN_NAME.c_str());

        return;
    }

    auto teams = getAvailableTeams();

    if (teams.size() != 2)
    {
        bz_debugMessagef(0, "ERROR :: %s :: The dodgeball game mode only works with two teams. This plug-in will **fail** to load.", PLUGIN_NAME.c_str());
        bz_debugMessagef(0, "ERROR :: %s ::   %d teams have been detected.", PLUGIN_NAME.c_str(), teams.size());

        return;
    }
    else
    {
        TEAM_ONE = teams.at(0);
        TEAM_TWO = teams.at(1);
    }

    bz_registerCustomMapObject("jail", this);

    Register(bz_eGetPlayerSpawnPosEvent);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_eTickEvent);
}

void Dodgeball::Cleanup(void)
{
    Flush();

    bz_removeCustomMapObject("jail");
}

bool Dodgeball::MapObject(bz_ApiString object, bz_CustomMapObjectInfo *data)
{
    if (object != "JAIL" || !data)
    {
        return false;
    }

    bz_eTeamType team = eNoTeam;
    TeamJail newZone;
    newZone.handleDefaultOptions(data);

    for (unsigned int i = 0; i < data->data.size(); i++)
    {
        std::string line = data->data.get(i).c_str();

        bz_APIStringList *nubs = bz_newStringList();
        nubs->tokenize(line.c_str(), " ", 0, true);

        if (nubs->size() > 0)
        {
            std::string key = bz_toupper(nubs->get(0).c_str());

            if (key == "COLOR" && nubs->size() == 2)
            {
                team = (bz_eTeamType)atoi(nubs->get(1).c_str());
            }
        }

        bz_deleteStringList(nubs);
    }

    TeamJails[team].push_back(newZone);

    return true;
}

void Dodgeball::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eGetPlayerSpawnPosEvent:
        {
            bz_GetPlayerSpawnPosEventData_V1* spawnData = (bz_GetPlayerSpawnPosEventData_V1*)eventData;

            if (gameModeEnabled && inJail[spawnData->playerID] && !TeamJails[spawnData->team].empty())
            {
                spawnData->handled = true;

                float spawnPos[3];
                auto randomJailID = rand() % TeamJails[spawnData->team].size();

                TeamJail zone = TeamJails[spawnData->team].at(randomJailID);

                bz_getRandomPoint(&zone, spawnPos);

                spawnData->pos[0] = spawnPos[0];
                spawnData->pos[1] = spawnPos[1];
                spawnData->pos[2] = spawnPos[2];
            }
        }
        break;

        case bz_ePlayerDieEvent:
        {
            bz_PlayerDieEventData_V1* dieData = (bz_PlayerDieEventData_V1*)eventData;

            if (gameModeEnabled)
            {
                inJail[dieData->playerID] = true;
                lastKill[dieData->killerTeam] = dieData->killerID;

                // If the killer was in jail, set them free
                if (inJail[dieData->killerID] && dieData->killerTeam != dieData->team)
                {
                    safelyFreePrisoner(dieData->killerID);
                }

                checkGameOver();
            }
        }
        break;

        case bz_ePlayerJoinEvent:
        {
            bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            if (!gameModeEnabled)
            {
                bz_sendTextMessage(BZ_SERVER, joinData->playerID, "Dodgeball is disabled; a minimum of 2 players per team is needed.");
            }

            inJail[joinData->playerID] = false;
        }
        break;

        case bz_ePlayerPartEvent:
        {
            if (gameModeEnabled)
            {
                checkGameOver();
            }
        }
        break;

        case bz_eTickEvent:
        {
            if (bz_getTeamCount(TEAM_ONE) < 2 || bz_getTeamCount(TEAM_TWO) < 2)
            {
                if (gameModeEnabled)
                {
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Dodgeball has been disabled; a minimum of 2 players per team is needed.");
                    gameModeEnabled = false;
                }

                return;
            }

            if (!gameModeEnabled)
            {
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Dodgeball has been enabled!");
                gameModeEnabled = true;
            }
        }
        break;

        default:
            break;
    }
}

/**
 * Get a list of all the teams available on the current server.
 *
 * @return A vector of teams loaded on the server
 */
std::vector<bz_eTeamType> Dodgeball::getAvailableTeams()
{
    std::vector<bz_eTeamType> teams;

    for (bz_eTeamType t = eRedTeam; t <= ePurpleTeam; t = (bz_eTeamType) (t + 1))
    {
        if (bz_getTeamPlayerLimit(t) > 0)
        {
            teams.push_back(t);
        }
    }

    return teams;
}

/**
 * The win condition for a game dodgeball is to be the last team
 */
void Dodgeball::checkGameOver(void)
{
    if (gameOverCheckLocked)
    {
        return;
    }

    int teamsFree = 0;
    bz_eTeamType winningTeam = eNoTeam;
    bz_eTeamType losingTeam = eNoTeam;

    if (!isEntireTeamInJail(TEAM_ONE))
    {
        winningTeam = TEAM_ONE;
        losingTeam = TEAM_TWO;
        teamsFree++;
    }

    if (!isEntireTeamInJail(TEAM_TWO))
    {
        winningTeam = TEAM_TWO;
        losingTeam = TEAM_ONE;
        teamsFree++;
    }

    if (teamsFree == 1)
    {
        doGameOver(winningTeam, losingTeam);
    }
}

/**
 * A match of dodgeball has finished, let's clear all of the
 */
void Dodgeball::doGameOver(bz_eTeamType winner, bz_eTeamType loser)
{
    gameOverCheckLocked = true;

    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "The %s team has eliminated the %s team!", bzu_GetTeamName(winner), bzu_GetTeamName(loser));

    bz_incrementTeamWins(winner, 1);
    bz_incrementTeamLosses(loser, 1);

    // On a capture, free everyone from prison
    bz_APIIntList *playerList = bz_getPlayerIndexList();

    for (unsigned int i = 0; i < playerList->size(); ++i)
    {
        safelyFreePrisoner(playerList->get(i));
    }

    bz_deleteIntList(playerList);

    gameOverCheckLocked = false;
}

/**
 * Check to see if all the players on a team are in jail.
 *
 * @param _team The team to check
 *
 * @return True if the entire team is in jail
 */
bool Dodgeball::isEntireTeamInJail(bz_eTeamType _team)
{
    if (bz_getTeamCount(_team) < 2)
    {
        return false;
    }

    bool entireTeamInJail = true;

    bz_APIIntList *playerList = bz_newIntList();
    bz_getPlayerIndexList(playerList);

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        int playerID = playerList->get(i);

        if (bz_getPlayerTeam(playerID) != _team)
        {
            continue;
        }

        if (!inJail[playerID] && bz_getIdleTime(playerID) < 10)
        {
            entireTeamInJail = false;
            break;
        }
    }

    bz_deleteIntList(playerList);

    return entireTeamInJail;
}

/**
 * This method will release a player from jail.
 *
 * If the player is alive, it will mark the player as free, kill the player, and offset their score by one if they were
 * killed in the freedom process.
 *
 * This method can safely be given any player ID, valid or invalid.
 *
 * @param playerID The ID of the player currently in jail to set free
 *
 * @return True if a player was in jail and was set free
 */
bool Dodgeball::safelyFreePrisoner(int playerID)
{
    if (playerID > MAX_PLAYER_ID)
    {
        return false;
    }

    if (!inJail[playerID])
    {
        return false;
    }

    if (bz_killPlayer(playerID, false))
    {
        // Only increment a player's score if they were actually killed to offset the point loss for dying
        bz_incrementPlayerWins(playerID, 1);
    }

    inJail[playerID] = false;

    return true;
}
