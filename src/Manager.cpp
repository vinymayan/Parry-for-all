#include "Manager.h"

namespace FormUtil {
    const RE::TESFile* GetMasterFile(RE::TESForm* ref) {
        if (!ref) return nullptr;

        uint32_t formID = ref->GetFormID();
        uint8_t modIndex = static_cast<uint8_t>(formID >> 24);

        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return nullptr;

        if (modIndex == 0xFE) {
            uint16_t eslIndex = (formID >> 12) & 0xFFF;
            return dataHandler->LookupLoadedLightModByIndex(eslIndex);
        }

        return dataHandler->LookupLoadedModByIndex(modIndex);
    }

    std::string NormalizeFormID(RE::TESForm* form) {
        if (!form) return {};

        RE::FormID formID = form->GetFormID();
        uint8_t modIndex = (formID >> 24) & 0xFF;

        if (modIndex == 0xFF) {
            return std::format("{:X}", formID);
        }

        auto file = GetMasterFile(form);
        if (!file) return std::format("{:X}", formID);

        uint32_t localID = formID & 0x00FFFFFF;

        if (modIndex == 0xFE) {
            uint32_t eslID = localID & 0xFFF;
            return std::format("{}|{:X}", file->GetFilename(), eslID);
        }

        return std::format("{}|{:X}", file->GetFilename(), localID);
    }

    // Função auxiliar para reverter string para FormID no load do JSON
    RE::FormID FormIDFromString(const std::string& str) {
        auto pos = str.find('|');
        if (pos != std::string::npos) {
            std::string plugin = str.substr(0, pos);
            std::string idStr = str.substr(pos + 1);
            RE::FormID localId = std::stoul(idStr, nullptr, 16);
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            return dataHandler ? dataHandler->LookupFormID(localId, plugin) : 0;
        }
        return str.empty() ? 0 : std::stoul(str, nullptr, 16);
    }
}

void Manager::PopulateAllLists() {
    if (_isPopulated) return;

    logger::info("Iniciando escaneamento de FormTypes...");

    PopulateList<RE::BGSPerk>("Perk");
    PopulateList<RE::TESGlobal>("Global", [](RE::TESGlobal* glob) -> bool {
        return glob != nullptr;
        });
    _isPopulated = true;
    for (auto cb : _readyCallbacks) {
        if (cb) cb();
    }
    _readyCallbacks.clear();
}

const std::vector<InternalFormInfo>& Manager::GetList(const std::string& typeName) {
    static std::vector<InternalFormInfo> empty;
    auto it = _dataStore.find(typeName);
    if (it != _dataStore.end()) {
        return it->second;
    }
    return empty;
}

void Manager::RegisterReadyCallback(std::function<void()> callback) {
    if (_isPopulated) {
        callback();
    }
    else {
        _readyCallbacks.push_back(callback);
    }
}


std::string Manager::ToUTF8(std::string_view a_str) {
    if (a_str.empty()) return "";

    // 1. Testa se a string já é um UTF-8 válido
    int u8Test = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a_str.data(), static_cast<int>(a_str.size()), nullptr, 0);
    if (u8Test > 0) {
        // É UTF-8 válido (Skyrim SE nativo), retorna sem corromper
        return std::string(a_str);
    }

    // 2. Se falhou, a string é ANSI (Mod antigo ou locale específico do Windows).
    // Precisamos converter de ANSI (CP_ACP) para UTF-16, e depois para UTF-8.
    int wlen = MultiByteToWideChar(CP_ACP, 0, a_str.data(), static_cast<int>(a_str.size()), nullptr, 0);
    if (wlen <= 0) return std::string(a_str);

    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, a_str.data(), static_cast<int>(a_str.size()), &wstr[0], wlen);

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wlen, nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) return std::string(a_str);

    std::string u8str(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wlen, &u8str[0], u8len, nullptr, nullptr);

    return u8str;
}

template <typename T>
void Manager::PopulateList(const std::string& a_typeName, std::function<bool(T*)> a_filter) {
    auto dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;

    auto& list = _dataStore[a_typeName];
    list.clear();

    const auto& forms = dataHandler->GetFormArray<T>();
    list.reserve(forms.size());

    for (const auto& form : forms) {
        if (!form) continue;

        if (form->IsDeleted() || form->IsIgnored()) {
            continue;
        }

        if (a_filter && !a_filter(form)) {
            continue;
        }
        // Variáveis de auxílio para o log de erro caso o catch seja acionado
        RE::FormID currentID = 0;
        std::string currentPlugin = "Unknown";

        try {
            currentID = form->GetFormID();

            // Obtém o nome do plugin de origem antes de qualquer processamento complexo
            if (auto file = form->GetFile(0)) {
                currentPlugin = std::string(file->GetFilename());
            }
            else {
                currentPlugin = "Dynamic";
            }

            InternalFormInfo info;
            info.formID = currentID;
            info.formType = a_typeName;
            info.pluginName = ToUTF8(currentPlugin);

            // EditorID: clib_util pode lançar exceções em contextos raros de memória
            std::string rawEditorID = clib_util::editorID::get_editorID(form);
            info.editorID = ToUTF8(rawEditorID);

            std::string rawName = "";
            if (form->Is(RE::FormType::NPC)) {
                if (auto npc = form->As<RE::TESNPC>()) {
                    rawName = npc->fullName.c_str();
                }
            }
            else if (auto fullName = form->As<RE::TESFullName>()) {
                rawName = fullName->fullName.c_str();
            }

            // A conversão UTF-8 é um ponto comum de falha se a string estiver corrompida
            info.name = ToUTF8(rawName);
            info.UpdateDisplayName();
            list.push_back(info);
        }
        catch (const std::exception& e) {
            // Log detalhado com FormID em Hexadecimal e o erro específico
            logger::error("[PopulateList] Critical error on item {:08X} of plugin '{}' (Type: {}). Error: {}",
                currentID, currentPlugin, a_typeName, e.what());
        }
        catch (...) {
            // Captura erros desconhecidos que não herdam de std::exception
            logger::error("[PopulateList] Uknown error on item {:08X} of plugin '{}' (Type: {})",
                currentID, currentPlugin, a_typeName);
        }
    }
    logger::info("Carregados {} itens do tipo {}", list.size(), a_typeName);
}



void Manager::RegisterAffectedNPC(RE::FormID baseID, const std::string& nifPath) {
    _affectedNPCs[baseID] = nifPath;
}

void Manager::UnregisterAffectedNPC(RE::FormID baseID) {
    _affectedNPCs.erase(baseID);
}

bool Manager::IsNPCAffected(RE::FormID baseID) {
    auto it = _affectedNPCs.find(baseID);
    if (it != _affectedNPCs.end()) {

        return true;
    }
    return false;
}
