/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTINSPECTOR_H
#define PLAYERBOTS_PLAYERBOTINSPECTOR_H

#include <optional>
#include <string>
#include <vector>

#include "Bot/Telemetry/PlayerbotVerificationState.h"
#include "Define.h"

inline constexpr uint32 PLAYERBOT_INSPECTION_SCHEMA_VERSION = 2;
inline constexpr uint32 PLAYERBOT_VERIFICATION_INSPECTION_SCHEMA_VERSION = 4;

class Player;
class PlayerbotAI;

struct PlayerbotInspectionUnit
{
    bool available = false;
    uint32 entry = 0;
    std::string name;
};

struct PlayerbotInspectionRpgTarget
{
    bool available = false;
    std::string type;
    std::string guid;
    uint32 entry = 0;
    std::string name;
    uint32 npcFlags = 0;
    float distanceYards = 0.0f;
    bool moving = false;
};

struct PlayerbotInspectionTravel
{
    bool available = false;
    std::string status = "unavailable";
    std::string destinationType;
    std::string destinationTitle;
    std::optional<float> distanceYards;
    std::optional<uint32> timeLeftMs;
    uint32 moveRetry = 0;
    uint32 extendRetry = 0;
};

struct PlayerbotInspectionPersonality
{
    bool available = false;
    uint32 version = 0;
    uint32 craftingAffinity = 0;
    uint32 explorationAffinity = 0;
    uint32 sociability = 0;
    std::string voice;
};

struct PlayerbotInspectionEquipment
{
    uint32 slot = 0;
    uint32 itemId = 0;
    std::string name;
    uint32 count = 0;
};

struct PlayerbotInspectionItem
{
    uint32 itemId = 0;
    std::string name;
    uint32 count = 0;
};

struct PlayerbotInspectionSkill
{
    uint32 id = 0;
    std::string name;
    uint32 value = 0;
    uint32 maximum = 0;
};

struct PlayerbotVerificationCompleteness
{
    uint64 totalCount = 0;
    uint64 returnedCount = 0;
    bool truncated = false;
};

struct PlayerbotVerificationMaster
{
    bool available = false;
    std::string guid;
    std::string name;
    bool relationshipValid = false;
};

struct PlayerbotVerificationGroupMember
{
    std::string guid;
    std::string name;
    uint32 subgroup = 0;
    bool leader = false;
};

struct PlayerbotVerificationGroup
{
    bool available = false;
    std::string guid;
    std::string leaderGuid;
    std::vector<PlayerbotVerificationGroupMember> members;
    PlayerbotVerificationCompleteness completeness;
};

struct PlayerbotVerificationPosition
{
    uint32 mapId = 0;
    uint32 zoneId = 0;
    uint32 areaId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float orientation = 0.0f;
    uint32 movementFlags = 0;
    bool moving = false;
    std::string movementState = "stationary";
};

struct PlayerbotVerificationTransport
{
    bool attached = false;
    std::string guid;
    uint32 entry = 0;
};

struct PlayerbotVerificationTravelPoint
{
    bool available = false;
    uint32 mapId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float distanceYards = 0.0f;
};

struct PlayerbotVerificationTravelRoute
{
    uint32 pointCount = 0;
    std::string nextPathType = "none";
    uint32 nextEntry = 0;
    PlayerbotVerificationTravelPoint nextPoint;
};

struct PlayerbotVerificationLastMovement
{
    PlayerbotVerificationTravelPoint point;
    uint32 ageMs = 0;
    uint32 delayMs = 0;
    std::string priority = "normal";
};

struct PlayerbotVerificationTravel
{
    bool available = false;
    std::string status = "unavailable";
    std::string destinationType;
    std::string destinationTitle;
    float distanceYards = 0.0f;
    bool forced = false;
    bool canMove = false;
    PlayerbotVerificationTravelRoute route;
    PlayerbotVerificationLastMovement lastMovement;
};

struct PlayerbotVerificationInspection
{
    std::string guid;
    std::string name;
    uint32 level = 0;
    uint32 raceId = 0;
    uint32 classId = 0;
    PlayerbotVerificationMaster master;
    PlayerbotVerificationGroup group;
    PlayerbotVerificationPosition position;
    PlayerbotVerificationTransport transport;
    PlayerbotVerificationTravel travel;
    PlayerbotInspectionRpgTarget rpgTarget;
    std::string lastExecutedAction;
    PlayerbotVerificationActionHistory actionHistory;
    uint64 snapshotTimestampMs = 0;
    uint64 moneyCopper = 0;
    uint64 freeTradeskillCopper = 0;
    uint64 freeSpellsCopper = 0;
    PlayerbotVerificationCareerPublication career;
    std::vector<uint32> knownRecipeSpellIds;
    PlayerbotVerificationCompleteness knownRecipeCompleteness;
    PlayerbotVerificationEconomyObservation economy;
    PlayerbotVerificationCompleteness equipmentCompleteness;
    std::vector<PlayerbotInspectionEquipment> equipment;
    PlayerbotVerificationCompleteness inventoryCompleteness;
    std::vector<PlayerbotInspectionItem> inventory;
    PlayerbotVerificationCompleteness skillsCompleteness;
    std::vector<PlayerbotInspectionSkill> skills;
    PlayerbotVerificationCompleteness professionsCompleteness;
    std::vector<PlayerbotInspectionSkill> professions;
};

struct PlayerbotInspection
{
    std::string name;
    uint32 level = 0;
    uint32 raceId = 0;
    uint32 classId = 0;
    uint32 xp = 0;
    uint32 nextLevelXp = 0;
    std::string activity;
    std::string state;
    std::string action;
    std::vector<std::string> strategies;
    bool inCombat = false;
    PlayerbotInspectionUnit target;
    std::vector<PlayerbotInspectionUnit> attackers;
    PlayerbotInspectionTravel travel;
    PlayerbotInspectionRpgTarget rpgTarget;
    PlayerbotInspectionPersonality personality;
    std::vector<PlayerbotInspectionEquipment> equipment;
    std::vector<PlayerbotInspectionItem> inventory;
    std::vector<PlayerbotInspectionSkill> skills;
    std::vector<PlayerbotInspectionSkill> professions;
};

class PlayerbotInspector
{
public:
    static std::string Inspect(Player* bot, PlayerbotAI* botAI);
    // Builds the complete, uncapped verification snapshot. Display caps are applied by
    // SerializeVerification, so wait conditions can be evaluated against full state.
    static PlayerbotVerificationInspection BuildVerification(Player* bot, PlayerbotAI* botAI);
    static std::string InspectVerification(Player* bot, PlayerbotAI* botAI);
    static std::string Serialize(PlayerbotInspection const& inspection);
    static std::string SerializeVerification(PlayerbotVerificationInspection const& inspection);
    static std::string BotNotFound();
    static std::string VerificationBotNotFound();
};

#endif
