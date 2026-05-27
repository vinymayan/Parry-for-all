namespace Tracking
{
    inline std::uint32_t PerfectParry = 0;
    inline std::uint32_t NormalParry = 0; // Nova variável para o Parry comum

    // --- SERIALIZAÇÃO (SKSE Cosave) ---
    namespace Serialization
    {
        constexpr std::uint32_t kSerializationID = 'VIN2'; // Identificador principal do seu Mod
        constexpr std::uint32_t kDataVersion = 1;

        // Sub-IDs para identificar cada tipo de dado internamente
        constexpr std::uint32_t kPerfectParryRecord = 'PDOD';
        constexpr std::uint32_t kNormalParryRecord = 'NDOD';

        inline bool hasLoadedData = false;

        static void SaveCallback(SKSE::SerializationInterface* a_intfc)
        {
            auto player = RE::PlayerCharacter::GetSingleton();
            // Salva a contagem de Perfect Parrys
            if (a_intfc->OpenRecord(kPerfectParryRecord, kDataVersion)) {
                a_intfc->WriteRecordData(PerfectParry);
                player->SetGraphVariableInt("PerfectParriedHitsSTM", static_cast<int>(PerfectParry));
            }
            else {
                SKSE::log::error("Falha ao abrir record de Perfect Parry!");
            }

            // Salva a contagem de Parrys Normais
            if (a_intfc->OpenRecord(kNormalParryRecord, kDataVersion)) {
                a_intfc->WriteRecordData(NormalParry);
                player->SetGraphVariableInt("ParriedHitsSTM", static_cast<int>(NormalParry));
            }
            else {
                SKSE::log::error("Falha ao abrir record de Normal Parry!");
            }
        }

        static void LoadCallback(SKSE::SerializationInterface* a_intfc)
        {
            std::uint32_t type;
            std::uint32_t version;
            std::uint32_t length;

            hasLoadedData = false;
            auto player = RE::PlayerCharacter::GetSingleton();
            // O loop varre os sub-records salvos dentro do ID 'VINY'
            while (a_intfc->GetNextRecordInfo(type, version, length)) {
                switch (type) {
                case kPerfectParryRecord:
                    a_intfc->ReadRecordData(PerfectParry);
                    player->SetGraphVariableInt("PerfectParriedHitsSTM", static_cast<int>(PerfectParry));
                    hasLoadedData = true;
                    break;

                case kNormalParryRecord:
                    a_intfc->ReadRecordData(NormalParry);
                    player->SetGraphVariableInt("ParriedHitsSTM", static_cast<int>(NormalParry));
                    hasLoadedData = true;
                    break;

                default:
                    SKSE::log::warn("Sub-record desconhecido encontrado: {:X}", type);
                    break;
                }
            }
            SKSE::log::info("Dados carregados - Perfect: {} | Normal: {}", PerfectParry, NormalParry);
        }

        static void RevertCallback(SKSE::SerializationInterface*)
        {
            auto player = RE::PlayerCharacter::GetSingleton();
            PerfectParry = 0;
            NormalParry = 0;
            player->SetGraphVariableInt("ParriedHitsSTM", static_cast<int>(NormalParry));
            player->SetGraphVariableInt("PerfectParriedHitsSTM", static_cast<int>(PerfectParry));
            hasLoadedData = false;
        }
    }
}