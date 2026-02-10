#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cstdint>

namespace zzzPotionHotkeySKSE {
    static float GetStrength(RE::AlchemyItem* a_alch) {
        if (!a_alch) {
            return 0.0f;
        }
        if (a_alch->effects.empty() || !a_alch->effects[0]) {
            return 0.0f;
        }
        return a_alch->effects[0]->effectItem.magnitude;
    }

    static RE::AlchemyItem* FindOptimalPotionImpl(RE::Actor* a_actor, RE::BGSKeyword* a_restoreKW, float a_deficit, bool a_allowOH, bool a_prioritizeOH) {
        if (!a_actor || !a_restoreKW) {
            return nullptr;
        }

        if (a_deficit <= 0.0f) {
            return nullptr;
        }

        const auto invCounts = a_actor->GetInventoryCounts(
            [](RE::TESBoundObject& a_obj) { return a_obj.GetFormType() == RE::FormType::AlchemyItem; });

        RE::AlchemyItem* smallestBigger = nullptr;
        RE::AlchemyItem* biggestSmaller = nullptr;

        float smallestBiggerStrength = 0.0f;
        float biggestSmallerStrength = 0.0f;

        for (const auto& [obj, count] : invCounts) {
            if (!obj || count <= 0) {
                continue;
            }

            auto* alch = obj->As<RE::AlchemyItem>();
            if (!alch) {
                continue;
            }

            if (alch->IsFood()) {
                continue;
            }

            if (alch->effects.size() != 1) {
                continue;
            }

            if (!alch->HasKeyword(a_restoreKW)) {
                continue;
            }

            const float strength = GetStrength(alch);
            if (strength <= 0.0f) {
                continue;
            }

            if (strength >= a_deficit) {
                if (!smallestBigger || strength < smallestBiggerStrength) {
                    smallestBigger = alch;
                    smallestBiggerStrength = strength;
                }
            }

            if (strength <= a_deficit) {
                if (!biggestSmaller || strength > biggestSmallerStrength) {
                    biggestSmaller = alch;
                    biggestSmallerStrength = strength;
                }
            }
        }

        if (smallestBigger && a_prioritizeOH && a_allowOH) {
            return smallestBigger;
        }
        if (biggestSmaller) {
            return biggestSmaller;
        }
        if (smallestBigger && a_allowOH) {
            return smallestBigger;
        }

        return nullptr;
    }

    static RE::AlchemyItem* FindOptimalPotion(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BGSKeyword* a_restoreKW, float a_deficit, bool a_allowOH, bool a_prioritizeOH) {
        return FindOptimalPotionImpl(a_actor, a_restoreKW, a_deficit, a_allowOH, a_prioritizeOH);
    }

    static bool Bind(RE::BSScript::IVirtualMachine* a_vm) 
    { 
        a_vm->RegisterFunction("FindOptimalPotion", "zzzPotionHotkey_SKSE", FindOptimalPotion);
        return true;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);
    SKSE::GetPapyrusInterface()->Register(zzzPotionHotkeySKSE::Bind);
    return true;
}