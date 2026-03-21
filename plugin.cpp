#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_set>

namespace zzzPotionHotkeySKSE {
    static std::unordered_set<RE::FormID> g_excludedForms;

    static void LoadExclusions() {
        std::ifstream file("Data/SKSE/Plugins/SmartOptimalSalves.ini");
        if (!file.is_open()) {
            return;
        }

        auto* dh = const_cast<RE::TESDataHandler*>(RE::TESDataHandler::GetSingleton());
        if (!dh) {
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
                continue;
            }

            const auto colon = line.find(':');
            if (colon == std::string::npos) {
                continue;
            }

            const std::string plugin = line.substr(0, colon);
            const std::string formStr = line.substr(colon + 1);

            RE::FormID localID = 0;
            try {
                localID = std::stoul(formStr, nullptr, 16);
            } catch (...) {
                continue;
            }

            auto* form = dh->LookupForm(localID, plugin);
            if (!form) {
                continue;
            }

            g_excludedForms.insert(form->GetFormID());
        }
    }

    static float StrengthFromEffect(RE::Effect* a_eff) {
        if (!a_eff) {
            return 0.0f;
        }

        const auto& ei = a_eff->effectItem;
        const float mag = ei.magnitude;
        float dur = static_cast<float>(ei.duration);

        if (mag <= 0.0f) {
            return 0.0f;
        }

        if (dur <= 0.0f) {
            dur = 1.0f;
        }

        return mag * dur;
    }

    static float GetStrengthForKeyword(RE::AlchemyItem* a_alch, RE::BGSKeyword* a_kw) {
        if (!a_alch || !a_kw) {
            return 0.0f;
        }

        float best = 0.0f;

        for (auto* eff : a_alch->effects) {
            if (!eff) {
                continue;
            }

            auto* mgef = eff->baseEffect;
            if (!mgef) {
                continue;
            }

            if (!mgef->HasKeyword(a_kw)) {
                continue;
            }

            const float s = StrengthFromEffect(eff);
            if (s > best) {
                best = s;
            }
        }

        if (best > 0.0f) {
            return best;
        }

        if (a_alch->HasKeyword(a_kw) && !a_alch->effects.empty()) {
            return StrengthFromEffect(a_alch->effects[0]);
        }

        return 0.0f;
    }

    static RE::AlchemyItem* FindOptimalPotionImpl(RE::Actor* a_actor, RE::BGSKeyword* a_restoreKW, float a_deficit,
                                                  bool a_allowOH, bool a_prioritizeOH) {
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

            if (g_excludedForms.count(alch->GetFormID())) {
                continue;
            }

            if (alch->IsFood()) {
                continue;
            }

            if (!alch->HasKeyword(a_restoreKW)) {
                bool found = false;
                for (auto* eff : alch->effects) {
                    if (eff && eff->baseEffect && eff->baseEffect->HasKeyword(a_restoreKW)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    continue;
                }
            }

            const float strength = GetStrengthForKeyword(alch, a_restoreKW);
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

    static RE::AlchemyItem* FindOptimalPotion(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BGSKeyword* a_restoreKW,
                                              float a_deficit, bool a_allowOH, bool a_prioritizeOH) {
        return FindOptimalPotionImpl(a_actor, a_restoreKW, a_deficit, a_allowOH, a_prioritizeOH);
    }

    static bool Bind(RE::BSScript::IVirtualMachine* a_vm) {
        a_vm->RegisterFunction("FindOptimalPotion", "zzzPotionHotkey_SKSE", FindOptimalPotion);
        return true;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            zzzPotionHotkeySKSE::LoadExclusions();
        }
    });

    SKSE::GetPapyrusInterface()->Register(zzzPotionHotkeySKSE::Bind);
    return true;
}