/*
Dodgeball
    Copyright (C) 2016 Vladimir "allejo" Jimenez

    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <iterator>
#include <random>
#include <sstream>
#include <vector>

#include "bzfsAPI.h"
#include "plugin_utils.h"

static const char* eTeamTypeLiteral(bz_eTeamType _team)
{
    switch (_team)
    {
        case eRogueTeam:
            return "Rogue";

        case eRedTeam:
            return "Red";

        case eGreenTeam:
            return "Green";

        case eBlueTeam:
            return "Blue";

        case ePurpleTeam:
            return "Purple";

        case eRabbitTeam:
            return "Rabbit";

        case eHunterTeam:
            return "Hunter";

        case eObservers:
            return "Observer";

        case eAdministrators:
            return "Administrator";

        default:
            return "No";
    }
}

// Define plugin name
const std::string PLUGIN_NAME = "Dodgeball";

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 0;
const int REV = 0;
const int BUILD = 3;

template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));

    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    return select_randomly(start, end, gen);
}

class TeamJail : public bz_CustomZoneObject_V2
{
public:
    TeamJail() : bz_CustomZoneObject_V2() {}
};

class Dodgeball : public bz_Plugin, bz_CustomMapObjectHandler
{
public:
    virtual const char* Name ();
    virtual void Init (const char* config);
    virtual void Event (bz_EventData *eventData);
    virtual void Cleanup (void);

    virtual bool MapObject (bz_ApiString object, bz_CustomMapObjectInfo *data);

    virtual bool isEntireTeamInJail (bz_eTeamType _team);
    virtual void checkGameOver (void);
    virtual void killAll (void);

    typedef std::vector<TeamJail> ZoneVector;
    std::map<bz_eTeamType, ZoneVector> TeamJails;

    std::vector<bz_eTeamType> availableTeams;

    bool gamemodeEnabled;
    bool inJail[256];
};

BZ_PLUGIN(Dodgeball)

const char* Dodgeball::Name (void)
{
    static std::string pluginBuild = "";

    if (!pluginBuild.size())
    {
        std::ostringstream pluginBuildStream;

        pluginBuildStream << PLUGIN_NAME << " " << MAJOR << "." << MINOR << "." << REV << " (" << BUILD << ")";
        pluginBuild = pluginBuildStream.str();
    }

    return pluginBuild.c_str();
}

void Dodgeball::Init (const char* commandLine)
{
    bz_registerCustomMapObject("JAIL", this);

    for (bz_eTeamType t = eRedTeam; t <= ePurpleTeam; t = (bz_eTeamType) (t + 1))
    {
        if (bz_getTeamPlayerLimit(t) > 0)
        {
            availableTeams.push_back(t);
        }
    }

    // Register our events with Register()
    Register(bz_eAllowCTFCaptureEvent);
    Register(bz_eGetPlayerSpawnPosEvent);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_eTickEvent);
}

void Dodgeball::Cleanup (void)
{
    Flush(); // Clean up all the events
}

bool Dodgeball::MapObject (bz_ApiString object, bz_CustomMapObjectInfo *data)
{
    if (object != "JAIL" || !data)
    {
        return false;
    }

    bz_eTeamType team;
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

void Dodgeball::Event (bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eAllowCTFCaptureEvent: // This event is called each time a flag is about to be captured
        {
            bz_AllowCTFCaptureEventData_V1* allowCtfData = (bz_AllowCTFCaptureEventData_V1*)eventData;

            allowCtfData->allow = false;
        }
        break;

        case bz_eGetPlayerSpawnPosEvent: // This event is called each time the server needs a new spawn postion
        {
            bz_GetPlayerSpawnPosEventData_V1* spawnData = (bz_GetPlayerSpawnPosEventData_V1*)eventData;

            if (gamemodeEnabled && inJail[spawnData->playerID] && !TeamJails[spawnData->team].empty())
            {
                spawnData->handled = true;

                float spawnPos[3];

                TeamJail zone = *select_randomly(TeamJails[spawnData->team].begin(), TeamJails[spawnData->team].end());
                zone.getRandomPoint(spawnPos);

                spawnData->pos[0] = spawnPos[0];
                spawnData->pos[1] = spawnPos[1];
                spawnData->pos[2] = spawnPos[2];
            }
        }
        break;

        case bz_ePlayerDieEvent: // This event is called each time a tank is killed.
        {
            bz_PlayerDieEventData_V1* dieData = (bz_PlayerDieEventData_V1*)eventData;

            if (gamemodeEnabled)
            {
                inJail[dieData->playerID] = true;

                if (inJail[dieData->killerID] && dieData->playerID != dieData->killerID)
                {
                    bz_killPlayer(dieData->killerID, false);
                    bz_incrementPlayerWins(dieData->killerID, 1);
                    inJail[dieData->killerID] = false;
                }

                checkGameOver();
            }
        }
        break;

        case bz_ePlayerJoinEvent: // This event is called each time a player joins the game
        {
            bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            if (!gamemodeEnabled)
            {
                bz_sendTextMessage(BZ_SERVER, joinData->playerID, "Dodgeball has been disabled; a minimum of 2 players per team is needed.");
            }

            inJail[joinData->playerID] = false;
        }
        break;

        case bz_eTickEvent:
        {
            for (auto team : availableTeams)
            {
                if (bz_getTeamCount(team) < 2)
                {
                    if (gamemodeEnabled)
                    {
                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Dodgeball has been disabled; a minimum of 2 players per team is needed.");
                        gamemodeEnabled = false;
                    }

                    return;
                }
            }

            if (!gamemodeEnabled)
            {
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Dodgeball has been enabled!");
                gamemodeEnabled = true;
            }
        }
        break;

        default: break;
    }
}

void Dodgeball::checkGameOver (void)
{
    int teamsFree = 0;
    bz_eTeamType winningTeam = eNoTeam;

    for (auto team : availableTeams)
    {
        if (!isEntireTeamInJail(team))
        {
            winningTeam = team;
            teamsFree++;
        }
    }

    if (teamsFree == 1)
    {
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "The %s team successfully eliminated the other %s!", eTeamTypeLiteral(winningTeam),
            (availableTeams.size() > 2) ? "teams" : "team");

        bz_incrementTeamWins(winningTeam, 1);

        for (auto team : availableTeams)
        {
            if (team != winningTeam)
            {
                bz_incrementTeamLosses(team, 1);
            }
        }

        killAll();
    }
}

bool Dodgeball::isEntireTeamInJail (bz_eTeamType _team)
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

        if (bz_getPlayerTeam(playerID) == _team && !inJail[playerID])
        {
            entireTeamInJail = false;
            break;
        }
    }

    bz_deleteIntList(playerList);

    return entireTeamInJail;
}

void Dodgeball::killAll (void)
{
    bz_APIIntList *playerList = bz_newIntList();
    bz_getPlayerIndexList(playerList);

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        int playerID = playerList->get(i);

        if (inJail[playerID])
        {
            bz_killPlayer(playerID, false);
        }

        inJail[playerID] = false;
    }
}
