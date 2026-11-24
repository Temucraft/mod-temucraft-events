/*
 * mod-temucraft-events
 *
 * Elemental Invasion: every few days rifts open in Un'goro Crater (fire), Silithus (air), Azshara (earth) and
 * Winterspring (water). Each rift keeps a few invaders around it; killing invaders raises the zone's stage (more
 * invaders per rift), and at the last stage the zone's lord appears: Baron Charr, the Windreaver, Avalanchion or
 * Princess Tempestria. When a lord dies its rifts close. The invasion ends when all four lords are dead, or after
 * the maximum duration.
 *
 * Rift and lord positions come from mod_temucraft_events_invasion_point. Heights are taken from the live map
 * when the rift opens, so points follow the terrain the server actually uses. Progress is kept in world states
 * (saved), so an invasion survives a restart.
 *
 *   .temucraft invasion            status
 *   .temucraft invasion start|stop (GM)
 */

#include "Chat.h"
#include "WorldSessionMgr.h"
#include "TemucraftModuleText.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "WorldScript.h"
#include "WorldState.h"
#include <array>
#include <vector>

using namespace Acore::ChatCommands;

namespace TemucraftEvents
{
    enum : uint32
    {
        WS_ACTIVE = 2060100,      // 0 idle, 1 running
        WS_NEXT = 2060101,        // unix time of the next invasion
        WS_STARTED = 2060102,     // unix time the running invasion started
        WS_STAGE = 2060110,       // + element: 1..stages, stages+1 = lord up, stages+2 = lord slain
        WS_KILLS = 2060120,       // + element: invaders slain in this stage
    };

    struct Element
    {
        char const* name;
        char const* zoneName;
        uint32 zone;
        uint32 rift;
        uint32 invader;
        uint32 lord;
        char const* lordName;
    };

    std::array<Element, 4> const Elements = { {
        { "fire",  "Un'goro Crater", 490,  179666, 14460, 14461, "Baron Charr" },
        { "air",   "Silithus",       1377, 179667, 14455, 14454, "The Windreaver" },
        { "earth", "Azshara",        16,   179664, 14462, 14464, "Avalanchion" },
        { "water", "Winterspring",   618,  179665, 14458, 14457, "Princess Tempestria" },
    } };

    struct Point { uint8 element; bool lord; uint32 map; float x, y, z, o; };

    struct Cfg
    {
        bool enable = true;
        uint32 minDays = 2, maxDays = 4;
        uint32 maxHours = 72;
        uint32 stages = 4;
        uint32 killsPerStage = 50;
        uint32 stageMinutes = 60;      // a stage also rises on its own after this long
        uint32 minPerRift = 3, maxPerRift = 6;
        float spawnMin = 8.0f, spawnMax = 30.0f;
        float wander = 20.0f;
        uint32 invaderLifetime = 2 * HOUR;
        uint32 invaderCorpse = 5 * MINUTE, lordCorpse = 10 * MINUTE;
        bool announce = true;
    } cfg;

    struct RiftState
    {
        ObjectGuid rift;
        std::vector<ObjectGuid> invaders;
        Point point;
    };

    std::vector<Point> sPoints;
    std::array<std::vector<RiftState>, 4> sRifts;
    std::array<ObjectGuid, 4> sLords;
    std::array<uint32, 4> sStageTimer = { };
    uint32 sTick = 0;

    uint32 Now() { return uint32(GameTime::GetGameTime().count()); }
    uint32 WS(uint32 id) { return uint32(sWorldState->getWorldState(id)); }
    void SetWS(uint32 id, uint32 v) { sWorldState->setWorldState(id, v); }

    uint32 Stage(uint8 e) { return std::max<uint32>(1, WS(WS_STAGE + e)); }
    uint32 LordStage() { return cfg.stages + 1; }
    uint32 DoneStage() { return cfg.stages + 2; }

    void LoadConfig()
    {
        cfg.enable = sConfigMgr->GetOption<bool>("Temucraft.Events.Invasion.Enable", true);
        cfg.minDays = sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.MinDaysBetween", 2);
        cfg.maxDays = std::max(cfg.minDays, sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.MaxDaysBetween", 4));
        cfg.maxHours = sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.MaxHours", 72);
        cfg.stages = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.Stages", 4));
        cfg.killsPerStage = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.KillsPerStage", 50));
        cfg.stageMinutes = sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.StageMinutes", 60);
        cfg.minPerRift = sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.MinInvadersPerRift", 3);
        cfg.maxPerRift = std::max(cfg.minPerRift, sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.MaxInvadersPerRift", 6));
        cfg.spawnMin = sConfigMgr->GetOption<float>("Temucraft.Events.Invasion.SpawnMinDistance", 8.0f);
        cfg.spawnMax = sConfigMgr->GetOption<float>("Temucraft.Events.Invasion.SpawnMaxDistance", 30.0f);
        cfg.wander = sConfigMgr->GetOption<float>("Temucraft.Events.Invasion.WanderDistance", 20.0f);
        cfg.invaderLifetime = sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.InvaderLifetime", 120) * MINUTE;
        cfg.invaderCorpse = sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.InvaderCorpseSeconds", 300);
        cfg.lordCorpse = sConfigMgr->GetOption<uint32>("Temucraft.Events.Invasion.LordCorpseSeconds", 600);
        cfg.announce = sConfigMgr->GetOption<bool>("Temucraft.Events.Invasion.Announce", true);
    }

    void LoadPoints()
    {
        sPoints.clear();
        if (QueryResult r = WorldDatabase.Query("SELECT `Element`, `Lord`, `Map`, `X`, `Y`, `Z`, `O` FROM `mod_temucraft_events_invasion_point`"))
            do
            {
                Field* f = r->Fetch();
                Point p { f[0].Get<uint8>(), f[1].Get<bool>(), f[2].Get<uint32>(), f[3].Get<float>(), f[4].Get<float>(), f[5].Get<float>(), f[6].Get<float>() };
                if (p.element < Elements.size())
                    sPoints.push_back(p);
            } while (r->NextRow());
        LOG_INFO("module", "mod-temucraft-events: {} invasion points", sPoints.size());
    }

    void Announce(std::string const& text)
    {
        if (cfg.announce)
        {
            // ChatHandler(nullptr) has no session to send to (it asserted and took the server down): build the
            // system message once and send it to everyone online.
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, nullptr, nullptr, "|cffff8000[Elemental Invasion]|r " + text);
            sWorldSessionMgr->SendGlobalMessage(&data);
        }
    }

    Map* MapFor(Point const& p, bool load)
    {
        Map* map = sMapMgr->CreateBaseMap(p.map);
        if (map && load)
            map->LoadGrid(p.x, p.y);
        return map;
    }

    // ground height at a point on the server's terrain; keeps the stored z if nothing better is found
    float Ground(Map* map, Point const& p)
    {
        float z = map->GetHeight(PHASEMASK_NORMAL, p.x, p.y, p.z + 40.0f, true, 120.0f);
        if (z <= INVALID_HEIGHT)
            return p.z;
        float water = map->GetWaterLevel(p.x, p.y);
        if (water > z && water > INVALID_HEIGHT)
            z = water;
        return z;
    }

    // ---------------------------------------------------------------- rifts and invaders
    void OpenRifts(uint8 e)
    {
        sRifts[e].clear();
        for (Point const& p : sPoints)
        {
            if (p.element != e || p.lord)
                continue;
            Map* map = MapFor(p, true);
            if (!map)
                continue;
            float z = Ground(map, p);
            if (GameObject* go = map->SummonGameObject(Elements[e].rift, p.x, p.y, z, p.o, 0, 0, 0, 0, 0))
            {
                go->setActive(true);          // keeps the grid (and the invaders) loaded
                RiftState s;
                s.rift = go->GetGUID();
                s.point = p;
                s.point.z = z;
                sRifts[e].push_back(s);
            }
        }
    }

    void CloseRifts(uint8 e)
    {
        for (RiftState& s : sRifts[e])
        {
            Map* map = sMapMgr->FindBaseNonInstanceMap(s.point.map);
            if (!map)
                continue;
            if (GameObject* go = map->GetGameObject(s.rift))
            {
                go->SetRespawnTime(0);
                go->Delete();
            }
            for (ObjectGuid const& g : s.invaders)
                if (Creature* c = map->GetCreature(g))
                    if (c->IsAlive() && !c->IsInCombat())
                        c->DespawnOrUnsummon();
        }
        sRifts[e].clear();
    }

    void FillRifts(uint8 e)
    {
        uint32 stage = std::min(Stage(e), cfg.stages);
        uint32 want = std::min(cfg.maxPerRift, cfg.minPerRift + stage - 1);
        for (RiftState& s : sRifts[e])
        {
            Map* map = sMapMgr->FindBaseNonInstanceMap(s.point.map);
            if (!map)
                continue;
            s.invaders.erase(std::remove_if(s.invaders.begin(), s.invaders.end(), [map](ObjectGuid const& g)
            {
                Creature* c = map->GetCreature(g);
                return !c || !c->IsAlive();
            }), s.invaders.end());

            while (s.invaders.size() < want)
            {
                float angle = frand(0.0f, 2.0f * float(M_PI));
                float dist = frand(cfg.spawnMin, std::max(cfg.spawnMin, cfg.spawnMax));
                Point p = s.point;
                p.x += dist * std::cos(angle);
                p.y += dist * std::sin(angle);
                float z = Ground(map, p);
                if (std::fabs(z - s.point.z) > 15.0f)
                    z = s.point.z;
                Position pos(p.x, p.y, z + 0.5f, angle);
                TempSummon* c = map->SummonCreature(Elements[e].invader, pos, nullptr, cfg.invaderLifetime * IN_MILLISECONDS);
                if (!c)
                    break;
                c->SetTempSummonType(TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN);
                c->SetHomePosition(pos);
                c->SetWanderDistance(cfg.wander);
                c->SetDefaultMovementType(RANDOM_MOTION_TYPE);
                c->GetMotionMaster()->MoveRandom(cfg.wander);
                c->SetCorpseDelay(cfg.invaderCorpse);
                s.invaders.push_back(c->GetGUID());
            }
        }
    }

    void SpawnLord(uint8 e)
    {
        for (Point const& p : sPoints)
        {
            if (p.element != e || !p.lord)
                continue;
            Map* map = MapFor(p, true);
            if (!map)
                return;
            Position pos(p.x, p.y, Ground(map, p) + 0.5f, p.o);
            if (TempSummon* lord = map->SummonCreature(Elements[e].lord, pos, nullptr, uint32(cfg.maxHours) * HOUR * IN_MILLISECONDS))
            {
                lord->SetTempSummonType(TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN);
                lord->setActive(true);
                lord->SetCorpseDelay(cfg.lordCorpse);
                sLords[e] = lord->GetGUID();
                Announce(std::string(Elements[e].lordName) + " has come through the rifts in " + Elements[e].zoneName + "!");
            }
            return;
        }
        // no lord point: count the zone as finished
        SetWS(WS_STAGE + e, DoneStage());
    }

    // ---------------------------------------------------------------- invasion
    void ScheduleNext()
    {
        SetWS(WS_NEXT, Now() + urand(cfg.minDays * DAY, cfg.maxDays * DAY));
    }

    void Start()
    {
        SetWS(WS_ACTIVE, 1);
        SetWS(WS_STARTED, Now());
        for (uint8 e = 0; e < Elements.size(); ++e)
        {
            SetWS(WS_STAGE + e, 1);
            SetWS(WS_KILLS + e, 0);
            sStageTimer[e] = 0;
            OpenRifts(e);
        }
        Announce("Elemental rifts have torn open in Un'goro Crater, Silithus, Azshara and Winterspring!");
    }

    // after a restart: reopen what was open
    void Resume()
    {
        for (uint8 e = 0; e < Elements.size(); ++e)
        {
            uint32 stage = Stage(e);
            if (stage < DoneStage())
                OpenRifts(e);
            if (stage == LordStage())
                SpawnLord(e);
        }
    }

    void Stop(bool scheduleNext)
    {
        for (uint8 e = 0; e < Elements.size(); ++e)
        {
            CloseRifts(e);
            if (!sLords[e].IsEmpty())
            {
                for (Point const& p : sPoints)
                    if (p.element == e && p.lord)
                        if (Map* map = sMapMgr->FindBaseNonInstanceMap(p.map))
                            if (Creature* c = map->GetCreature(sLords[e]))
                                if (c->IsAlive())
                                    c->DespawnOrUnsummon();
                sLords[e].Clear();
            }
            SetWS(WS_STAGE + e, 0);
            SetWS(WS_KILLS + e, 0);
        }
        SetWS(WS_ACTIVE, 0);
        if (scheduleNext)
            ScheduleNext();
    }

    void RaiseStage(uint8 e)
    {
        uint32 stage = Stage(e) + 1;
        SetWS(WS_STAGE + e, stage);
        SetWS(WS_KILLS + e, 0);
        sStageTimer[e] = 0;
        if (stage == LordStage())
            SpawnLord(e);
        else if (stage <= cfg.stages)
            Announce(std::string("The rifts in ") + Elements[e].zoneName + " grow stronger (stage " + std::to_string(stage) + " of " + std::to_string(cfg.stages) + ").");
    }

    void Update(uint32 diff)
    {
        sTick += diff;
        if (sTick < 10 * IN_MILLISECONDS)
            return;
        uint32 elapsed = sTick;
        sTick = 0;
        if (!cfg.enable)
            return;

        if (!WS(WS_ACTIVE))
        {
            if (!WS(WS_NEXT))
                ScheduleNext();
            else if (Now() >= WS(WS_NEXT))
                Start();
            return;
        }

        bool allDone = true;
        for (uint8 e = 0; e < Elements.size(); ++e)
        {
            uint32 stage = Stage(e);
            if (stage < DoneStage())
                allDone = false;
            if (stage <= cfg.stages)
            {
                sStageTimer[e] += elapsed;
                if (cfg.stageMinutes && sStageTimer[e] >= cfg.stageMinutes * MINUTE * IN_MILLISECONDS)
                    RaiseStage(e);
                else
                    FillRifts(e);
            }
            else if (stage == LordStage() && sLords[e].IsEmpty())
                SpawnLord(e);
        }

        if (allDone)
        {
            Announce("The elemental lords are vanquished. The rifts are closed, for now.");
            Stop(true);
        }
        else if (cfg.maxHours && Now() > WS(WS_STARTED) + cfg.maxHours * HOUR)
        {
            Announce("The rifts collapse on their own as the elemental tide recedes.");
            Stop(true);
        }
    }

    void OnKill(Creature* killed)
    {
        if (!WS(WS_ACTIVE))
            return;
        for (uint8 e = 0; e < Elements.size(); ++e)
        {
            Element const& el = Elements[e];
            if (killed->GetEntry() == el.invader && Stage(e) <= cfg.stages)
            {
                uint32 kills = WS(WS_KILLS + e) + 1;
                SetWS(WS_KILLS + e, kills);
                if (kills >= cfg.killsPerStage)
                    RaiseStage(e);
                return;
            }
            if (killed->GetEntry() == el.lord && killed->GetGUID() == sLords[e])
            {
                SetWS(WS_STAGE + e, DoneStage());
                sLords[e].Clear();
                CloseRifts(e);
                Announce(std::string(el.lordName) + " has been slain! The rifts in " + el.zoneName + " fall silent.");
                return;
            }
        }
    }

    // ---------------------------------------------------------------- scripts
    class TemucraftEventsWorld : public WorldScript
    {
    public:
        TemucraftEventsWorld() : WorldScript("TemucraftEventsWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE }) { }

        void OnAfterConfigLoad(bool /*reload*/) override { LoadConfig(); }

        void OnStartup() override
        {
            LoadPoints();
            if (cfg.enable && WS(WS_ACTIVE))
                Resume();
        }

        void OnUpdate(uint32 diff) override { Update(diff); }
    };

    class TemucraftEventsPlayer : public PlayerScript
    {
    public:
        TemucraftEventsPlayer() : PlayerScript("TemucraftEventsPlayer", { PLAYERHOOK_ON_CREATURE_KILL, PLAYERHOOK_ON_CREATURE_KILLED_BY_PET }) { }

        void OnPlayerCreatureKill(Player* /*killer*/, Creature* killed) override { if (killed) OnKill(killed); }
        void OnPlayerCreatureKilledByPet(Player* /*owner*/, Creature* killed) override { if (killed) OnKill(killed); }
    };

    class TemucraftEventsCommands : public CommandScript
    {
    public:
        TemucraftEventsCommands() : CommandScript("TemucraftEventsCommands") { }

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable invasion =
            {
                { "",      HandleStatus, 100450,     Console::Yes },
                { "start", HandleStart,  100401 /* SEC_GAMEMASTER -> RBAC */, Console::Yes },
                { "stop",  HandleStop,   100402 /* SEC_GAMEMASTER -> RBAC */, Console::Yes },
            };
            static ChatCommandTable root = { { "temucraft invasion", invasion } };
            return root;
        }

        static bool HandleStatus(ChatHandler* handler)
        {
            if (!WS(WS_ACTIVE))
            {
                uint32 next = WS(WS_NEXT);
                uint32 now = Now();
                if (next > now)
                    handler->PSendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 1, "No elemental invasion right now. The next one is expected in about {} hours."), (next - now) / HOUR + 1);
                else
                    handler->SendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 2, "No elemental invasion right now."));
                return true;
            }
            handler->SendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 3, "Elemental Invasion:"));
            for (uint8 e = 0; e < Elements.size(); ++e)
            {
                uint32 stage = Stage(e);
                if (stage <= cfg.stages)
                    handler->PSendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 4, "  {}: stage {} of {}, {}/{} invaders slain"), Elements[e].zoneName, stage, cfg.stages, WS(WS_KILLS + e), cfg.killsPerStage);
                else if (stage == LordStage())
                    handler->PSendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 5, "  {}: {} is loose!"), Elements[e].zoneName, Elements[e].lordName);
                else
                    handler->PSendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 6, "  {}: {} slain, rifts closed"), Elements[e].zoneName, Elements[e].lordName);
            }
            return true;
        }

        static bool HandleStart(ChatHandler* handler)
        {
            if (WS(WS_ACTIVE))
                Stop(false);
            Start();
            handler->SendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 7, "Elemental invasion started."));
            return true;
        }

        static bool HandleStop(ChatHandler* handler)
        {
            Stop(true);
            handler->SendSysMessage(TemucraftModuleText("mod-temucraft-events", handler, 8, "Elemental invasion stopped; the next one is scheduled."));
            return true;
        }
    };
}

void AddTemucraftTurkeysScripts();

void AddTemucraftEventsScripts()
{
    AddTemucraftTurkeysScripts();
    new TemucraftEvents::TemucraftEventsWorld();
    new TemucraftEvents::TemucraftEventsPlayer();
    new TemucraftEvents::TemucraftEventsCommands();
}
