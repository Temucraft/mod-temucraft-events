/*
 * mod-temucraft-events: Wild Turkeys (Pilgrim's Bounty)
 *
 * The Turkey Tracker kill counter (spell 62014) also makes you faster: each stack gives
 * Temucraft.Events.Turkey.SpeedPerStack percent run speed, up to Temucraft.Events.Turkey.MaxSpeed. When the counter
 * runs out (30 seconds without a kill) it isn't lost at once: it comes back with half the stacks, and halves again
 * each time it runs out, until it is gone. The speed follows the stacks.
 * Respawn time and the critter flag of the turkeys are set in data/sql/db-world/base/mod_temucraft_events_turkeys.sql.
 */

#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "WorldScript.h"
#include <algorithm>

namespace TemucraftTurkeys
{
    enum
    {
        SPELL_TURKEY_TRACKER = 62014,
        SPELL_SPEED          = 2379    // "Speed": run speed aura; the amount is set when it is cast
    };

    struct Settings
    {
        bool enable = true;
        int32 speedPerStack = 1;
        int32 maxSpeed = 30;
        bool halve = true;
    };
    Settings cfg;

    // Our speed aura has no cast item; a Swiftness Potion's has one and is left alone.
    void RemoveSpeed(Unit* target)
    {
        if (Aura* speed = target->GetAura(SPELL_SPEED, target->GetGUID()))
            if (!speed->GetCastItemGUID())
                target->RemoveAura(speed);
    }

    void UpdateSpeed(Unit* target, uint8 stacks, int32 durationMs)
    {
        RemoveSpeed(target);
        if (!cfg.enable || !stacks || durationMs <= 0)
            return;
        int32 const percent = std::min<int32>(cfg.maxSpeed, cfg.speedPerStack * stacks);
        if (percent <= 0)
            return;
        int32 const bp = percent - 1;   // the spell adds 1 (its die roll) to the amount
        target->CastCustomSpell(target, SPELL_SPEED, &bp, nullptr, nullptr, true);
        if (Aura* speed = target->GetAura(SPELL_SPEED, target->GetGUID()))
            if (!speed->GetCastItemGUID())
            {
                speed->SetMaxDuration(durationMs);
                speed->SetDuration(durationMs);
            }
    }

    class spell_temucraft_turkey_tracker : public AuraScript
    {
        PrepareAuraScript(spell_temucraft_turkey_tracker);

        // first kill and every stack change (a stack change reapplies the effect)
        void AfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            UpdateSpeed(GetTarget(), GetStackAmount(), GetDuration());
        }

        void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes mode)
        {
            if (mode & AURA_EFFECT_HANDLE_REAPPLY)
                return;   // a stack change: AfterApply follows

            Unit* target = GetTarget();
            RemoveSpeed(target);

            // only when it ran out; at 40 kills the event script ends it on purpose, and death ends it too
            if (!cfg.enable || !cfg.halve || GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
                return;
            uint8 const half = GetStackAmount() / 2;
            if (!half)
                return;
            // added again a moment later, after this aura is gone
            target->m_Events.AddEventAtOffset([target, half]()
            {
                if (!target->IsInWorld() || !target->IsAlive() || target->HasAura(SPELL_TURKEY_TRACKER))
                    return;
                if (Aura* aura = target->AddAura(SPELL_TURKEY_TRACKER, target))
                    aura->SetStackAmount(half);
            }, 1ms);
        }

        void Register() override
        {
            AfterEffectApply += AuraEffectApplyFn(spell_temucraft_turkey_tracker::AfterApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
            AfterEffectRemove += AuraEffectRemoveFn(spell_temucraft_turkey_tracker::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        }
    };

    class TemucraftTurkeysWorld : public WorldScript
    {
    public:
        TemucraftTurkeysWorld() : WorldScript("TemucraftTurkeysWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            cfg.enable = sConfigMgr->GetOption<bool>("Temucraft.Events.Turkey.Enable", true);
            cfg.speedPerStack = sConfigMgr->GetOption<int32>("Temucraft.Events.Turkey.SpeedPerStack", 1);
            cfg.maxSpeed = sConfigMgr->GetOption<int32>("Temucraft.Events.Turkey.MaxSpeed", 30);
            cfg.halve = sConfigMgr->GetOption<bool>("Temucraft.Events.Turkey.Halve", true);
        }
    };
}

void AddTemucraftTurkeysScripts()
{
    new TemucraftTurkeys::TemucraftTurkeysWorld();
    RegisterSpellScriptWithArgs(TemucraftTurkeys::spell_temucraft_turkey_tracker, "spell_temucraft_turkey_tracker");
}
