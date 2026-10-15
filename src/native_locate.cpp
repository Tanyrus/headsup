#include "native_locate.h"

#include "bytes.h"
#include "code_patch.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <iterator>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr uint8_t kIconBaseTable[]  = {0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9};
        constexpr uint8_t kIconCountTable[] = {0x00, 0x01, 0x02, 0x00, 0x01, 0x02};
        constexpr uint32_t kIconTableGap    = 8;
        // Both tables are indexed by icon code, and the codes start at 200, so the operands point 200 bytes short.
        constexpr uint32_t kFirstIconCode    = 200;
        constexpr uint32_t kBaseReadOperand  = 0x1F9;
        constexpr uint32_t kCountReadOperand = 0x1FF;

        constexpr uint8_t kPrologue[] = {0x81, 0xEC, 0xC4, 0x06, 0x00, 0x00, 0x53, 0x55, 0x56, 0x57}; // sub esp,0x6C4; push x4
        constexpr uint32_t kHookOffset = 0x1D1;
        constexpr uint32_t kExitOffset = 0x6D7;
        constexpr uint8_t kExitBytes[] = {0x8D, 0x8C, 0x24, 0x84, 0x00, 0x00, 0x00}; // lea ecx,[esp+0x84]
        constexpr uint32_t kRoutineBytes = kExitOffset + sizeof(kExitBytes);

        // Each arrow's pushes up to its call: the target's reads m_AnkX (0xBC), m_AnkY (0xBE) and m_AnkNum (0xBA) and pushes
        // its gray, the candidate's reads m_SubAnkY (0xC2) and m_SubAnkX (0xC0) and pushes edi, the range color.
        constexpr uint8_t kTargetArrowLead[] = {0x66, 0x8B, 0x86, 0xBC, 0x00, 0x00, 0x00, 0x66, 0x85, 0xC0, 0x74, 0x32, 0x0F,
            0xBF, 0x96, 0xBE, 0x00, 0x00, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x33, 0xC9, 0x8A, 0x8E, 0xBA, 0x00, 0x00,
            0x00, 0x68, 0x80, 0x80, 0x80, 0xE0, 0x0F, 0xBF, 0xC0, 0x8B, 0x4C, 0x8E, 0x78, 0x68, 0x00, 0x00, 0x80, 0x3F, 0x68,
            0x00, 0x00, 0x80, 0x3F, 0x52, 0x50, 0xE8};
        // Before them the draw asks for the candidate's color: it pushes yellow, red, the blue in edi, the candidate and
        // you, and calls the routine on the window in esi.
        constexpr uint8_t kRangeColorLead[] = {0x68, 0x40, 0xD0, 0xD0, 0xE0, 0x68, 0x40, 0x40, 0xD0, 0xE0, 0x57, 0x52, 0x50,
            0x8B, 0xCE, 0xE8};
        constexpr uint8_t kPickedArrowLead[] = {0x0F, 0xBF, 0x96, 0xC2, 0x00, 0x00, 0x00, 0x0F, 0xBF, 0x86, 0xC0, 0x00, 0x00,
            0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x33, 0xC9, 0x8A, 0x8E, 0xBA, 0x00, 0x00, 0x00, 0x57, 0x68, 0x00, 0x00,
            0x80, 0x3F, 0x68, 0x00, 0x00, 0x80, 0x3F, 0x8B, 0x4C, 0x8E, 0x78, 0x52, 0x50, 0xE8};

        // The guesses' bytes: -1 for the import table addresses, which depend on where the client was loaded. The call is
        // 4 bytes in, the load 10.
        constexpr int16_t kAnyByte = -1;
        constexpr uint32_t kGuessCall = 4, kGuessLoad = 10;
        constexpr int16_t kMessageGuess[] = {0x8B, 0x48, 0x18, 0x51, 0xFF, 0x15, -1, -1, -1, -1, 0x8B, 0x2D, -1, -1, -1, -1, 0x6A, 0x20,
            0xFF, 0xD5, 0x6A, 0x21};
        constexpr int16_t kWarpGuess[] = {0xD9, 0x5C, 0x24, 0x20, 0xFF, 0x15, -1, -1, -1, -1, 0x8B, 0x35, -1, -1, -1, -1, 0x6A, 0x20,
            0xFF, 0xD6, 0x8B, 0xF8, 0x6A, 0x21};
        constexpr int16_t kEdgeGuess[] = {0x8B, 0x51, 0x18, 0x52, 0xFF, 0x15, -1, -1, -1, -1, 0x8B, 0x1D, -1, -1, -1, -1, 0x6A, 0x20,
            0xFF, 0xD3, 0x6A, 0x21};
        constexpr uint8_t kMovImmediate = 0xB8; // plus the register

        // A mouse move: the controller's pointer moved (call), then the show routine called with 1. The controller's
        // global is at 11 and 24, the show call at 30.
        constexpr int16_t kMoveShow[] = {0x8D, 0x4C, 0x24, 0x14, 0x8D, 0x54, 0x24, 0x24, 0x51, 0x8B, 0x0D, -1, -1, -1, -1, 0x53, 0x52,
            0xE8, -1, -1, -1, -1, 0x8B, 0x0D, -1, -1, -1, -1, 0x6A, 0x01, 0xE8, -1, -1, -1, -1};
        constexpr uint32_t kMoveController = 11, kShowController = 24, kShowCall = 30;
        // The command check's end: mov ecx,[controller]; push 1 (then 0); call mouse use; mov al,[esp+14h]; pop edi; pop
        // esi; pop ecx; ret. The on call is 8 bytes in, the off call 29.
        constexpr int16_t kMouseUseCalls[] = {0x8B, 0x0D, -1, -1, -1, -1, 0x6A, 0x01, 0xE8, -1, -1, -1, -1, 0x8A, 0x44, 0x24, 0x14, 0x5F,
            0x5E, 0x59, 0xC3, 0x8B, 0x0D, -1, -1, -1, -1, 0x6A, 0x00, 0xE8, -1, -1, -1, -1, 0x8A, 0x44, 0x24, 0x14, 0x5F, 0x5E, 0x59, 0xC3};
        constexpr uint32_t kMouseOnCall = 8, kMouseOffCall = 29;
        // The show routine as far as its test of the shown byte: cmp [esi+4Eh], bl. Its globals are wild.
        constexpr int16_t kShowRoutine[] = {0xA1, -1, -1, -1, -1, 0x56, 0x8B, 0xF1, 0x8B, 0x0D, -1, -1, -1, -1, 0x85, 0xC0, 0x74, 0x3B,
            0x53, 0x8B, 0x5C, 0x24, 0x0C, 0x85, 0xC9, 0x74, 0x0C, 0x53, 0xE8, -1, -1, -1, -1, 0x8B, 0x0D, -1, -1, -1, -1, 0x38, 0x5E,
            static_cast<int16_t>(kPointerShownOffset)};
        constexpr uint8_t kModRmRegister = 3, kRegisterBits = 7;

        // The draw loop: mov ecx,esi; call the window draw; mov eax,[esp+0Ch]; test eax,eax; jnz next; inc bl;
        // cmp bl,9; jl the next layer.
        constexpr int16_t kWindowLoop[] = {0x8B, 0xCE, 0xE8, -1, -1, -1, -1, 0x8B, 0x44, 0x24, 0x0C, 0x85, 0xC0, 0x75,
            0xC6, 0xFE, 0xC3, 0x80, 0xFB, 0x09, 0x7C, 0xAD};
        constexpr uint32_t kWindowLoopCall = 2;
        // Before the loop, the list it walks: xor bl,bl; mov ecx,list; call First, within kWindowListReach bytes.
        constexpr int16_t kWindowListMove[] = {0x32, 0xDB, 0xB9, -1, -1, -1, -1, 0xE8};
        constexpr uint32_t kWindowListAddress = 3, kWindowListReach = 0x60;
        // The window draw's first ten bytes: sub esp,24h; push ebx, ebp, esi, edi; mov ebp,ecx; call. hudbegone patches
        // it from entry+14, so nothing past these is checked.
        constexpr uint8_t kWindowDrawEntry[] = {0x83, 0xEC, 0x24, 0x53, 0x55, 0x56, 0x57, 0x8B, 0xE9, 0xE8};
        // MCD::MoveTo's start: sub esp,0Ch; mov ax,[esp+14h]; push ebx; push ebp; mov ebp,[esp+18h]; push esi;
        // mov esi,ecx; push edi; xor edi,edi; lea ebx,[esi+14h]; mov [esi+52h],bp; mov ecx,ebx; mov [esi+54h],ax.
        constexpr int16_t kWindowMoveTo[] = {0x83, 0xEC, 0x0C, 0x66, 0x8B, 0x44, 0x24, 0x14, 0x53, 0x55, 0x8B, 0x6C, 0x24,
            0x18, 0x56, 0x8B, 0xF1, 0x57, 0x33, 0xFF, 0x8D, 0x5E, 0x14, 0x66, 0x89, 0x6E, 0x52, 0x8B, 0xCB, 0x66, 0x89, 0x46,
            0x54};
        // MCD::GetRect's start: sub esp,0Ch; push esi; mov esi,ecx; mov al,[esi+99h]; test al,al; jz.
        // mov edx,[the strip title's owner]; add esp,10h; lea ecx,[esp+4]; push ecx; mov ecx,[edx+8]; call GetRect; then
        // the label drawn at the rectangle's left and its bottom plus 3, in 80808080.
        constexpr int16_t kScanLabel[] = {0x8B, 0x15, -1, -1, -1, -1, 0x83, 0xC4, 0x10, 0x8D, 0x4C, 0x24, 0x04, 0x51,
            0x8B, 0x4A, 0x08, 0xE8, -1, -1, -1, -1, 0x8B, 0x4C, 0x24, 0x0A, 0x8B, 0x54, 0x24, 0x04, 0x8D, 0x44, 0x24, 0x10,
            0x68, 0x80, 0x80, 0x80, 0x80, 0x83, 0xC1, 0x03};
        constexpr uint32_t kScanLabelCall = 17;
        constexpr int16_t kWindowGetRect[] = {0x83, 0xEC, 0x0C, 0x56, 0x8B, 0xF1, 0x8A, 0x86, 0x99, 0x00, 0x00, 0x00,
            0x84, 0xC0, 0x0F, 0x84};
        constexpr char kMenuPrefix[] = "menu    ";
        constexpr size_t kMenuPrefixLength = sizeof(kMenuPrefix) - 1;
        static_assert(kMenuPrefixLength + kWindowKeyLength == kWindowNameLength);
        // The windows players know, by key.
        struct KnownWindow
        {
            const char* key;
            const char* label;
        };
        constexpr KnownWindow kKnownWindows[] = {
            {"equip", "Equipment"}, {"inventor", "Inventory"}, {"iteminfo", "Item Details"},
            {"itemctrl", "Item Options"}, {"itmsortw", "Sort"}, {"itmsort2", "Sort Options"},
            {"statcom", "Status"}, {"statcom2", "Status Details"}, {"money", "Gil"},
            {"moneyctr", "Gil Amount"}, {"menuwind", "Main Menu"}, {"ability", "Abilities"},
            {"abimenu", "Ability Menu"}, {"magic", "Magic"}, {"mgcmenu", "Magic Menu"},
            {"magselec", "Magic List"}, {"mgcsortw", "Magic Sort"}, {"btlskill", "Combat Skills"},
            {"mgcskill", "Magic Skills"}, {"trdskill", "Crafting Skills"}, {"joblevel", "Job Levels"},
            {"jobchang", "Job Change"}, {"levmerit", "Merit Points"}, {"masterle", "Master Levels"},
            {"trade", "Trade"}, {"bazaar", "Bazaar"}, {"friend", "Friends"},
            {"flistmai", "Friend List"}, {"social", "Social"}, {"socialme", "Social Menu"},
            {"partywin", "Party"}, {"prtyjoin", "Party Invite"}, {"link1", "Linkshell"},
            {"map0", "Map"}, {"mapframe", "Map Frame"}, {"maplist", "Map List"},
            {"region", "Region Info"}, {"conquer1", "Conquest"}, {"quest00", "Quests"},
            {"miss00", "Missions"}, {"missionm", "Mission Menu"}, {"storage", "Storage"},
            {"mnstorag", "Mog Storage"}, {"delivery", "Delivery Box"}, {"post1", "Post"},
            {"shop", "Shop"}, {"shopbuy", "Shop Buy"}, {"shopsell", "Shop Sell"},
            {"shopmain", "Shop Menu"}, {"auc1", "Auction House"}, {"auclist", "Auction List"},
            {"auchisto", "Auction History"}, {"guildsho", "Guild Shop"}, {"loot", "Treasure Pool"},
            {"macro", "Macros"}, {"mcrmenu", "Macro Menu"}, {"config", "Config"},
            {"logwindo", "Log"}, {"fulllog", "Full Log"}, {"profile", "Profile"},
            {"blklist", "Blacklist"}, {"emote", "Emotes"}, {"compass", "Compass"},
            {"inspect", "Check"}, {"titlehan", "Titles"}, {"mount", "Mounts"},
            {"gift", "Gifts"}, {"searchjo", "Search"}, {"imperial", "Imperial Standing"},
            {"allied", "Allied Notes"}};
        // Every menu window the client knows (its menu catalog), but the strip's own, a space apart.
        constexpr const char* kAllWindows =
            "ability abimenu abiselec abisortw acoption actiom10 actionm1 actionm2 actionm3 actionm4 actionm5 "
            "actionm6 actionm7 actionm8 actionm9 alarm album allied arealist armsort as_ms_e at_ms_e at_qs_e auc1 "
            "auc2 auc3 auc4 aucammo aucarmor aucfood auchisto aucitem auclist aucmagic aucmater aucmeals aucweapo "
            "automato bank bankmenu bazaar beseige1 beseige2 blklist blkmain bluehelp bluepoin bluequip bluesibo "
            "bluinven blusortw bs_ms_e bs_qs_e btlskill buff camparea campresu campsan casttime cfilter charlnk "
            "chatctrl chdummy chfwin chmkface chmkhair chmkjobs chmkname chmkpass chmkrace chmkserv chmksize "
            "chmktown chocobor chswin cmbhlst cmbhwin cmbmenu cnqframe cnttime colopoin colorank coloresu "
            "comgenre comment commenu compass compassd compassf comwin comwinde comwinfr comyn conf1 conf11l "
            "conf11m conf11s conf12wi conf13wi conf1win conf2win conf3win conf4 conf5m conf5w1 conf5w2 conf5win "
            "conf6win conf7 config configwi conftxtc confyn conq3pts conq6sta conquer1 contents dbnamese dead "
            "delivery dmgnum emote equip eventtim evitem evitem01 faqmain faqsub fep feppalle flistmai flmes "
            "fp_cat fp_cat2 fp_info fr_qs_e frames framesde framesfr framesus friend fulllog fxfilter gaugewin "
            "gift gmtell guide00 guide01 guildsho handover hn1blank hn1exist hn2blank hn2exist hn3blank hn3exist "
            "hnbackwi hncansel hnhead hnnaming hnwarnin hnyesno holdtime hpcol imperial inline inspect installp "
            "installu inventor item10in item11in item12in item4inf item5inf item6inf item7inf item8inf item9inf "
            "itemctrl iteminfo itemx10i itemx11i itemx12i itemx4in itemx5in itemx6in itemx7in itemx8in itemx9in "
            "itemxinf itemyinf itmsort2 itmsortw itmstora iuse jbpcat jn_qs_e jobchang jobcsel jobcselu joblevel "
            "k1assign k2assign key1top key1topu key2top keylayou keypad keypad2 keypad3 keypadus keyselec "
            "keytops3 level levelsyn levmeri2 levmerit link1 link10 link11 link12 link13 link2 link3 link4 link5 "
            "link6 link7 link8 link9 lnowin lobbyps2 lobbywin loby1win loby2win lobycwin lobyhelp logwin2 "
            "logwindo loot lootope lscswin macro magic magico2w magiconw magselec map0 mapframe maplist mapscan "
            "mapv2 mapv3 masterle mcr1edit mcr1edlo mcr1long mcr1pall mcr2edit mcr2edlo mcr2long mcr2pall "
            "mcralpha mcrbedit mcres20 mcresed mcresmn mcrledit mcrmenu mcrmogc mcrselec mcrselop menuwind merit1 "
            "merit2 merit2ca merit3 meritcat meritinf merityn mes1rcv mes2frnd mgcmenu mgcskill mgcsortw miss00 "
            "missionm mnstorag mogcont mogdoor mogext mogpost money moneyctr mount mp_abisc mp_cstm mp_dead "
            "mp_invnt mp_kncst mp_mnabi mp_mncst mp_mnnm mp_mntp2 mp_pmode mp_stat msgline msglist myroom nation1 "
            "nation2 nation3 ncol netbar netmeter netwait numctrl ok olstat oplev ot_qs_e partywin passinpu "
            "persona playermo plnt1 plnt2 plnt3 plnt4 plnt5 plnt6 pname post1 post2 pr_sc_e profile prty1 prty2 "
            "prty3 prty4 prty5 prty5de prty5fr prty5us prty7 prty8 prtyjoin prvdungp prvdunsi ptc10che ptc1prog "
            "ptc2warn ptc3bar1 ptc4bar2 ptc6yesn ptc7vup ptc8lice ptc9dele ptcbgwin ptw0 ptw1 ptw2 ptw3 ptw4 ptw5 "
            "ptw6 pupequip pupsibor query quest00 quest01 race1 race2 race3 race4 race5 race6 race7 race8 "
            "racecard raid1 raid2 ranking region rem4li2 rem4line resyn resynde resynfr resynus rmlo1 rmlo2 rmlo4 "
            "rmpost roomlist roommenu sc_item sc_item_ scanlist scchar sccomite sccomls sccompar scmlev scname "
            "scoption scresult scroll scsibori sd_ms_e sd_qs_e searchjo searchle searchma searchna searchra shop "
            "shopbuy shopmain shopsell social socialme sortyn splmsg2 spoolmsg statcom statcom2 statpup storage "
            "storage2 stringdl subwindo targetwi textcol1 textcol2 textcol3 titlehan trade trddummy trdskill "
            "tskill1 tskill2 ut_menu ut_point wepsort windowps worldsel ws_ms_e ws_qs_e zl_ms_e ";
        constexpr char kHelpWindow[]         = "menu    helpwind";
        constexpr char kTitleWindow[]        = "menu    titlewin";
        constexpr char kNetstatWindow[]      = "menu    netstat "; // the space is part of its name

        // The menu catalog's records: two dwords, the window's name, and kCatalogSlot in, its owner's slot.
        constexpr int16_t kHelpRecord[]  = {0x2B, 0x00, 0x01, 0x00, 0x35, 0x07, 0x00, 0x00, 'm', 'e', 'n', 'u', ' ', ' ',
            ' ', ' ', 'h', 'e', 'l', 'p', 'w', 'i', 'n', 'd'};
        constexpr int16_t kTitleRecord[] = {0x93, 0x80, 0x00, 0x00, 0x05, 0x02, 0x00, 0x00, 'm', 'e', 'n', 'u', ' ', ' ',
            ' ', ' ', 't', 'i', 't', 'l', 'e', 'w', 'i', 'n'};
        constexpr uint32_t kCatalogSlot = 0x28;
        // add ecx,<help text>; jmp SetText; eight nops; add ecx,<help text>; call; mov ecx,[...]
        constexpr int16_t kHelpSetter[] = {0x83, 0xC1, -1, 0xE9, -1, -1, -1, -1, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
            0x90, 0x83, 0xC1, -1, 0xE8, -1, -1, -1, -1, 0x8B, 0x0D};
        constexpr uint32_t kHelpOffsetAt = 2, kHelpOffsetAgainAt = 18, kHelpJumpAt = 3;
        // add ecx,<title text>; jmp SetText; five nops; then the copying setter's entry
        constexpr int16_t kTitleSetter[] = {0x81, 0xC1, -1, -1, -1, -1, 0xE9, -1, -1, -1, -1, 0x90, 0x90, 0x90, 0x90, 0x90,
            0x8B, 0x44, 0x24, 0x04, 0x81, 0xEC, 0xCC, 0x00, 0x00, 0x00};
        constexpr uint32_t kTitleOffsetAt = 2, kTitleJumpAt = 6;
        constexpr int16_t kSetText[] = {0x51, 0x8B, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56, 0x85, 0xC0, 0x57, 0x8B, 0xE9, 0x75,
            0x14};
        // The level picker: ... lea edi,[esi+<level 3's buffer>]
        constexpr int16_t kLevelPicker[] = {0xA1, -1, -1, -1, -1, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x8A, 0x48, 0x18, 0x33,
            0xDB, 0x57, 0x3A, 0xCB, 0x74, 0x4E, 0x8D, 0xBE, -1, -1, -1, -1};
        constexpr uint32_t kTopBufferAt = 22;

        // The connection readout's two loads of its counters: mov eax,[net]; add eax,kNetCounters; push eax; call.
        constexpr int16_t kCounterLoad[] = {0xA1, -1, -1, -1, -1, 0x05, 0x38, 0x46, 0x00, 0x00, 0x50, 0xE8};
        static_assert(static_cast<uint32_t>(kCounterLoad[6]) == (kNetCounters & 0xFF) &&
                      static_cast<uint32_t>(kCounterLoad[7]) == (kNetCounters >> 8 & 0xFF));
        constexpr uint32_t kCounterLoadGlobal = 1;
        constexpr int16_t kWeatherLoad[]      = {0x66, 0xA1, -1, -1, -1, -1, 0x66, 0x3D, 0x04, 0x00, 0x72, -1, 0x66,
            0x3D, 0x14, 0x00};
        constexpr uint32_t kWeatherLoadGlobal = 2;
        constexpr size_t kCounterLoads        = 2;
        // The 3-second update: xor eax,eax; push 3000; clear both byte counts; mov edx,[net]; mov eax,[edx+4190h], the
        // milliseconds since the last server data.
        constexpr int16_t kCounterUpdate[] = {0x33, 0xC0, 0x68, 0xB8, 0x0B, 0x00, 0x00, 0x89, 0x46, 0x0C, 0x89, 0x46,
            0x10, 0x8B, 0x15, -1, -1, -1, -1, 0x8B, 0x82, 0x90, 0x41, 0x00, 0x00};
        constexpr uint32_t kCounterUpdateGlobal = 15;

        // add esp,0x14; ret 0x14: the wrapper entities' names are drawn through hands back as soon as the routine does.
        constexpr uint8_t kEntityCallerTail[] = {0x83, 0xC4, 0x14, 0xC2, 0x14, 0x00};

        bool Matches(const ImageSection& section, uint32_t offset, const uint8_t* bytes, uint32_t count)
        {
            return offset <= section.size && section.size - offset >= count &&
                   std::memcmp(section.bytes + offset, bytes, count) == 0;
        }


        std::vector<uint32_t> IconTables(const ImageSection& rdata)
        {
            std::vector<uint32_t> found;
            for (uint32_t offset = 0; offset + kIconTableGap + sizeof(kIconCountTable) <= rdata.size; ++offset)
                if (Matches(rdata, offset, kIconBaseTable, sizeof(kIconBaseTable)) &&
                    Matches(rdata, offset + kIconTableGap, kIconCountTable, sizeof(kIconCountTable)))
                    found.push_back(rdata.rva + offset);
            return found;
        }

        // Offsets into .text of every routine entry that reads this table pair the way the name routine does.
        void AddTableReaders(uint32_t moduleBase, uint32_t table, const ImageSection& text, std::vector<uint32_t>& entries)
        {
            const uint32_t baseOperand  = moduleBase + table - kFirstIconCode;
            const uint32_t countOperand = baseOperand + kIconTableGap;
            constexpr uint32_t kGap     = kCountReadOperand - kBaseReadOperand;
            for (uint32_t at = kBaseReadOperand; at + kGap + sizeof(uint32_t) <= text.size; ++at)
            {
                if (ReadAt<uint32_t>(text.bytes, at) != baseOperand ||
                    ReadAt<uint32_t>(text.bytes, at + kGap) != countOperand)
                    continue;
                const uint32_t entry = at - kBaseReadOperand;
                if (text.size - entry >= kRoutineBytes) entries.push_back(entry);
            }
        }

        // The RVA of the call each copy of lead ends in.
        std::vector<uint32_t> CallsAfter(const uint8_t* lead, uint32_t bytes, const ImageSection& text)
        {
            std::vector<uint32_t> calls;
            for (uint32_t at = 0; at + bytes + kBranchLength - 1 <= text.size; ++at)
                if (Matches(text, at, lead, bytes)) calls.push_back(text.rva + at + bytes - 1);
            return calls;
        }

        // Offsets into .text where the pattern matches, its -1 bytes matching anything.
        std::vector<uint32_t> Find(const int16_t* pattern, uint32_t bytes, const ImageSection& text)
        {
            std::vector<uint32_t> found;
            for (uint32_t at = 0; at + bytes <= text.size; ++at)
            {
                uint32_t i = 0;
                while (i < bytes && (pattern[i] == kAnyByte || text.bytes[at + i] == pattern[i]))
                    ++i;
                if (i == bytes) found.push_back(at);
            }
            return found;
        }

        uint32_t CallTarget(uint32_t call, const ImageSection& text)
        {
            return call + kBranchLength + ReadAt<uint32_t>(text.bytes, call - text.rva + 1);
        }

        template <size_t N>
        std::vector<uint32_t> FindAll(const int16_t (&pattern)[N], const ImageSection& section)
        {
            return Find(pattern, N, section);
        }

        std::vector<uint32_t> ReturnsFromCallsTo(uint32_t target, const ImageSection& text)
        {
            std::vector<uint32_t> returns;
            for (uint32_t at = 0; at + kBranchLength <= text.size; ++at)
            {
                if (text.bytes[at] != kCallOpcode) continue;
                const uint32_t returnRva = text.rva + at + kBranchLength;
                if (returnRva + ReadAt<uint32_t>(text.bytes, at + 1) == target) returns.push_back(returnRva);
            }
            return returns;
        }
    }

    const char* Describe(LocateProblem problem)
    {
        switch (problem)
        {
        case LocateProblem::None: return "found";
        case LocateProblem::NoIconTables: return "the client's icon tables were not found";
        case LocateProblem::NoTableReads: return "no routine reads the client's icon tables";
        case LocateProblem::AmbiguousTableReads: return "more than one routine reads the client's icon tables";
        case LocateProblem::UnexpectedPrologue: return "the name routine does not start as expected";
        case LocateProblem::PatchSiteChanged: return "the name routine was already changed, likely by another plugin";
        case LocateProblem::UnexpectedExit: return "the name routine does not end as expected";
        case LocateProblem::NoCallers: return "nothing calls the name routine";
        case LocateProblem::NoEntityCaller: return "the call that draws entities' names was not found";
        case LocateProblem::NoArrowDraws: return "the target window's arrow draws were not found";
        case LocateProblem::AmbiguousArrowDraws: return "the target window's arrow draws were found more than once";
        case LocateProblem::ArrowDrawsDiffer: return "the target window's two arrows are not drawn by the same function";
        case LocateProblem::NoRangeColor: return "the routine that colors the arrow over a sub-target was not found";
        case LocateProblem::AmbiguousRangeColor: return "the routine that colors the arrow over a sub-target was found more than once";
        case LocateProblem::NoPointerMapping: return "the game's reading of the mouse was not found";
        case LocateProblem::AmbiguousPointerMapping: return "the game's reading of the mouse was found more than once";
        case LocateProblem::PointerMappingDiffers: return "the game reads the mouse through different functions than expected";
        case LocateProblem::NoPointerShow: return "the game's showing of its pointer was not found";
        case LocateProblem::AmbiguousPointerShow: return "the game's showing of its pointer was found more than once";
        case LocateProblem::PointerShowChanged: return "the game's showing of its pointer is not as expected";
        case LocateProblem::NoKeyHide: return "where the game hides its pointer for a key was not found";
        case LocateProblem::AmbiguousKeyHide: return "where the game hides its pointer for a key was found more than once";
        case LocateProblem::KeyHideDiffers: return "the game hides its pointer for a key through an unexpected function";
        case LocateProblem::NoWindowDraw: return "the game's menu window draw loop was not found";
        case LocateProblem::AmbiguousWindowDraw: return "the game's menu window draw loop was found more than once";
        case LocateProblem::WindowDrawChanged: return "the game's menu window draw is not as HeadsUp expects";
        case LocateProblem::NoWeather: return "the game's weather was not found";
        case LocateProblem::AmbiguousWeather: return "the game's weather was found more than once";
        case LocateProblem::NoWindowMove: return "the game's window move was not found";
        case LocateProblem::AmbiguousWindowMove: return "the game's window move was found more than once";
        case LocateProblem::NoScanLabel: return "where Wide Scan places its grid label was not found";
        case LocateProblem::AmbiguousScanLabel: return "where Wide Scan places its grid label was found more than once";
        case LocateProblem::ScanLabelChanged: return "Wide Scan's grid label is placed through an unexpected function";
        case LocateProblem::NoStripText: return "the game's help bar text was not found";
        case LocateProblem::AmbiguousStripText: return "the game's help bar text was found in more places than one";
        case LocateProblem::StripTextChanged: return "the game's help bar text is not laid out as HeadsUp expects";
        case LocateProblem::NoNetStats: return "the game's connection readout was not found";
        case LocateProblem::AmbiguousNetStats: return "the game's connection readout was found in more places than one";
        case LocateProblem::NetStatsChanged: return "the game's connection counters are not where HeadsUp expects";
        }
        return "unknown problem";
    }

    NameRoutine LocateNameRoutine(uint32_t moduleBase, const ImageSection& text, const ImageSection& rdata)
    {
        NameRoutine result;
        const std::vector<uint32_t> tables = IconTables(rdata);
        if (tables.empty())
        {
            result.problem = LocateProblem::NoIconTables;
            return result;
        }
        std::vector<uint32_t> entries;
        for (const uint32_t table : tables)
            AddTableReaders(moduleBase, table, text, entries);
        if (entries.size() != 1)
        {
            result.problem = entries.empty() ? LocateProblem::NoTableReads : LocateProblem::AmbiguousTableReads;
            return result;
        }
        const uint32_t entry = entries.front();
        if (!Matches(text, entry, kPrologue, sizeof(kPrologue)))
            result.problem = LocateProblem::UnexpectedPrologue;
        else if (!Matches(text, entry + kHookOffset, kNameHookBytes, sizeof(kNameHookBytes)))
            result.problem = LocateProblem::PatchSiteChanged;
        else if (!Matches(text, entry + kExitOffset, kExitBytes, sizeof(kExitBytes)))
            result.problem = LocateProblem::UnexpectedExit;
        if (result.problem != LocateProblem::None) return result;

        result.entry         = text.rva + entry;
        result.hook          = result.entry + kHookOffset;
        result.resume        = result.hook + static_cast<uint32_t>(std::size(kNameHookBytes));
        result.exit          = result.entry + kExitOffset;
        result.callerReturns = ReturnsFromCallsTo(result.entry, text);
        if (result.callerReturns.empty())
        {
            result.problem = LocateProblem::NoCallers;
            return result;
        }
        uint32_t wrappers = 0;
        for (const uint32_t returnRva : result.callerReturns)
            if (Matches(text, returnRva - text.rva, kEntityCallerTail, sizeof(kEntityCallerTail)))
            {
                result.entityCaller = returnRva;
                ++wrappers;
            }
        if (wrappers != 1) result.problem = LocateProblem::NoEntityCaller;
        return result;
    }

    RangeColor LocateRangeColor(const ImageSection& text)
    {
        const std::vector<uint32_t> asks = CallsAfter(kRangeColorLead, sizeof(kRangeColorLead), text);
        if (asks.empty()) return RangeColor{LocateProblem::NoRangeColor};
        if (asks.size() > 1) return RangeColor{LocateProblem::AmbiguousRangeColor};
        return RangeColor{LocateProblem::None, CallTarget(asks.front(), text)};
    }

    ArrowCalls LocateArrowCalls(const ImageSection& text)
    {
        ArrowCalls result;
        const std::vector<uint32_t> target = CallsAfter(kTargetArrowLead, sizeof(kTargetArrowLead), text);
        const std::vector<uint32_t> picked = CallsAfter(kPickedArrowLead, sizeof(kPickedArrowLead), text);
        if (target.empty() || picked.empty()) result.problem = LocateProblem::NoArrowDraws;
        else if (target.size() > 1 || picked.size() > 1) result.problem = LocateProblem::AmbiguousArrowDraws;
        else if (CallTarget(target.front(), text) != CallTarget(picked.front(), text)) result.problem = LocateProblem::ArrowDrawsDiffer;
        else result = ArrowCalls{LocateProblem::None, target.front(), picked.front(), CallTarget(target.front(), text)};
        return result;
    }

    PointerMapping LocatePointerMapping(const ImageSection& text)
    {
        PointerMapping result;
        uint32_t windowRect = 0, metrics = 0;
        for (const auto& [pattern, bytes] : {std::pair{kMessageGuess, sizeof(kMessageGuess) / sizeof(int16_t)},
                 std::pair{kWarpGuess, sizeof(kWarpGuess) / sizeof(int16_t)}, std::pair{kEdgeGuess, sizeof(kEdgeGuess) / sizeof(int16_t)}})
        {
            const std::vector<uint32_t> found = Find(pattern, static_cast<uint32_t>(bytes), text);
            if (found.empty()) return PointerMapping{LocateProblem::NoPointerMapping, {}};
            if (found.size() > 1) return PointerMapping{LocateProblem::AmbiguousPointerMapping, {}};
            const uint32_t at = found.front();
            const uint32_t rect = ReadAt<uint32_t>(text.bytes, at + kGuessCall + 2);
            const uint32_t load = ReadAt<uint32_t>(text.bytes, at + kGuessLoad + 2);
            if (result.guesses.empty()) windowRect = rect, metrics = load;
            if (rect != windowRect || load != metrics) return PointerMapping{LocateProblem::PointerMappingDiffers, {}};
            const auto reg = static_cast<uint8_t>((text.bytes[at + kGuessLoad + 1] >> kModRmRegister) & kRegisterBits);
            result.guesses.push_back(WindowGuess{text.rva + at + kGuessCall, text.rva + at + kGuessLoad,
                static_cast<uint8_t>(kMovImmediate + reg)});
        }
        return result;
    }

    PointerShow LocatePointerShow(const ImageSection& text)
    {
        const std::vector<uint32_t> found = Find(kMoveShow, sizeof(kMoveShow) / sizeof(int16_t), text);
        if (found.empty()) return PointerShow{LocateProblem::NoPointerShow};
        if (found.size() > 1) return PointerShow{LocateProblem::AmbiguousPointerShow};
        const uint32_t at = found.front();
        const uint32_t controller = ReadAt<uint32_t>(text.bytes, at + kMoveController);
        const uint32_t show       = CallTarget(text.rva + at + kShowCall, text);
        const uint32_t showAt     = show - text.rva;
        if (ReadAt<uint32_t>(text.bytes, at + kShowController) != controller || showAt >= text.size ||
            Find(kShowRoutine, sizeof(kShowRoutine) / sizeof(int16_t), ImageSection{text.rva + showAt, text.bytes + showAt,
                                                                                std::min<uint32_t>(text.size - showAt,
                                                                                    sizeof(kShowRoutine) / sizeof(int16_t))})
                .empty())
            return PointerShow{LocateProblem::PointerShowChanged};
        return PointerShow{LocateProblem::None, controller, show};
    }

    KeyHide LocateKeyHide(const ImageSection& text)
    {
        const std::vector<uint32_t> found = Find(kMouseUseCalls, sizeof(kMouseUseCalls) / sizeof(int16_t), text);
        if (found.empty()) return KeyHide{LocateProblem::NoKeyHide};
        if (found.size() > 1) return KeyHide{LocateProblem::AmbiguousKeyHide};
        const uint32_t on = text.rva + found.front() + kMouseOnCall, off = text.rva + found.front() + kMouseOffCall;
        if (CallTarget(on, text) != CallTarget(off, text)) return KeyHide{LocateProblem::KeyHideDiffers};
        return KeyHide{LocateProblem::None, off, CallTarget(off, text)};
    }

    WindowDraw LocateWindowDraw(const ImageSection& text)
    {
        const std::vector<uint32_t> found = Find(kWindowLoop, sizeof(kWindowLoop) / sizeof(int16_t), text);
        if (found.empty()) return WindowDraw{LocateProblem::NoWindowDraw};
        if (found.size() > 1) return WindowDraw{LocateProblem::AmbiguousWindowDraw};
        const uint32_t call = text.rva + found.front() + kWindowLoopCall;
        const uint32_t draw = CallTarget(call, text);
        // Below the section, draw - text.rva wraps past its end, which Matches rejects.
        if (!Matches(text, draw - text.rva, kWindowDrawEntry, sizeof(kWindowDrawEntry)))
            return WindowDraw{LocateProblem::WindowDrawChanged};
        const uint32_t from = found.front() >= kWindowListReach ? found.front() - kWindowListReach : 0;
        const std::vector<uint32_t> moves =
            FindAll(kWindowListMove, ImageSection{text.rva + from, text.bytes + from, found.front() - from});
        const uint32_t list = moves.empty() ? 0 : ReadAt<uint32_t>(text.bytes, from + moves.back() + kWindowListAddress);
        return WindowDraw{LocateProblem::None, call, draw, list};
    }

    NetStats LocateNetStats(const ImageSection& text)
    {
        const std::vector<uint32_t> loads = FindAll(kCounterLoad, text);
        if (loads.empty()) return NetStats{LocateProblem::NoNetStats};
        const uint32_t global = ReadAt<uint32_t>(text.bytes, loads.front() + kCounterLoadGlobal);
        if (loads.size() != kCounterLoads || ReadAt<uint32_t>(text.bytes, loads.back() + kCounterLoadGlobal) != global)
            return NetStats{LocateProblem::AmbiguousNetStats};
        const std::vector<uint32_t> updates = FindAll(kCounterUpdate, text);
        if (updates.size() != 1 || ReadAt<uint32_t>(text.bytes, updates.front() + kCounterUpdateGlobal) != global)
            return NetStats{LocateProblem::NetStatsChanged};
        return NetStats{LocateProblem::None, global};
    }

    WeatherSite LocateWeather(const ImageSection& text)
    {
        const std::vector<uint32_t> found = FindAll(kWeatherLoad, text);
        if (found.empty()) return WeatherSite{LocateProblem::NoWeather};
        if (found.size() > 1) return WeatherSite{LocateProblem::AmbiguousWeather};
        return WeatherSite{LocateProblem::None, ReadAt<uint32_t>(text.bytes, found.front() + kWeatherLoadGlobal)};
    }

    std::optional<NetCounters> CountersFrom(int32_t received, int32_t sent, int32_t health)
    {
        const auto rate = [](int32_t r) { return r >= 0 && r <= kMaxRate; };
        if (!rate(received) || !rate(sent) || health < 0 || health > kFullHealth) return std::nullopt;
        return NetCounters{received, sent, health};
    }

    bool SkipsWindow(const char* name, HiddenWindows hidden)
    {
        if (name == nullptr) return false;
        const auto is = [name](const char* window) { return std::memcmp(name, window, kWindowNameLength) == 0; };
        return (hidden.helpBar && (is(kHelpWindow) || is(kTitleWindow))) || (hidden.netstat && is(kNetstatWindow));
    }

    WindowMoveSite LocateWindowMove(const ImageSection& text)
    {
        const std::vector<uint32_t> found = FindAll(kWindowMoveTo, text);
        if (found.empty()) return WindowMoveSite{LocateProblem::NoWindowMove};
        if (found.size() > 1) return WindowMoveSite{LocateProblem::AmbiguousWindowMove};
        const std::vector<uint32_t> getRect = FindAll(kWindowGetRect, text);
        return WindowMoveSite{LocateProblem::None, text.rva + found.front(),
            getRect.size() == 1 ? text.rva + getRect.front() : 0};
    }

    bool MoveUnderWay(int32_t move) { return move > 0; }

    ScanLabelSite LocateScanLabel(const ImageSection& text, uint32_t getRect)
    {
        const std::vector<uint32_t> found = FindAll(kScanLabel, text);
        if (getRect == 0 || found.empty()) return ScanLabelSite{LocateProblem::NoScanLabel};
        if (found.size() > 1) return ScanLabelSite{LocateProblem::AmbiguousScanLabel};
        const uint32_t call = text.rva + found.front() + kScanLabelCall;
        if (CallTarget(call, text) != getRect) return ScanLabelSite{LocateProblem::ScanLabelChanged};
        return ScanLabelSite{LocateProblem::None, call};
    }

    std::optional<WindowKey> WindowKeyOf(const char* name)
    {
        if (name == nullptr || std::memcmp(name, kMenuPrefix, kMenuPrefixLength) != 0) return std::nullopt;
        WindowKey key;
        std::memcpy(key.data(), name + kMenuPrefixLength, kWindowKeyLength);
        return key;
    }

    std::string ShortName(const WindowKey& key)
    {
        std::string name(key.begin(), key.end());
        name.erase(name.find_last_not_of(' ') + 1);
        return name;
    }

    std::optional<WindowKey> KeyFromShortName(std::string_view name)
    {
        // The game's names are lower-case letters, digits and underscores.
        const auto allowed = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; };
        if (name.empty() || name.size() > kWindowKeyLength || !std::ranges::all_of(name, allowed)) return std::nullopt;
        WindowKey key;
        key.fill(' ');
        std::ranges::copy(name, key.begin());
        return key;
    }

    bool HelpStripWindow(const WindowKey& key)
    {
        for (const char* strip : {kHelpWindow, kTitleWindow})
            if (std::memcmp(key.data(), strip + kMenuPrefixLength, kWindowKeyLength) == 0) return true;
        return false;
    }

    bool TitleWindow(const WindowKey& key)
    {
        return std::memcmp(key.data(), kTitleWindow + kMenuPrefixLength, kWindowKeyLength) == 0;
    }

    bool MovableWindow(const WindowKey& key)
    {
        for (const char* strip : {kHelpWindow, kTitleWindow, kNetstatWindow})
            if (std::memcmp(key.data(), strip + kMenuPrefixLength, kWindowKeyLength) == 0) return false;
        return true;
    }

    std::vector<WindowOffset> WindowOffsetsFrom(std::string_view text)
    {
        std::vector<WindowOffset> offsets;
        while (!text.empty() && offsets.size() < kMaxWindowKeys)
        {
            const size_t end = std::min(text.find(' '), text.size());
            const std::string_view entry = text.substr(0, end);
            text.remove_prefix(std::min(end + 1, text.size()));
            const size_t colon = entry.find(':'), comma = entry.find(',', colon == std::string_view::npos ? 0 : colon);
            if (colon == std::string_view::npos || comma == std::string_view::npos) continue;
            const std::string_view name = entry.substr(0, colon);
            const std::optional<WindowKey> key = KeyFromShortName(name);
            int x = 0, y = 0;
            const auto number = [](std::string_view digits, int& out) {
                const auto [end, problem] = std::from_chars(digits.data(), digits.data() + digits.size(), out);
                return problem == std::errc{} && end == digits.data() + digits.size();
            };
            if (!key || !MovableWindow(*key) || !number(entry.substr(colon + 1, comma - colon - 1), x) ||
                !number(entry.substr(comma + 1), y) || (x == 0 && y == 0) ||
                std::ranges::any_of(offsets, [&](const WindowOffset& o) { return o.name == name; }))
                continue;
            offsets.push_back(WindowOffset{std::string(name), std::clamp(x, -kMaxWindowOffset, kMaxWindowOffset),
                std::clamp(y, -kMaxWindowOffset, kMaxWindowOffset)});
        }
        return offsets;
    }

    std::string WindowOffsetText(const std::vector<WindowOffset>& offsets)
    {
        std::string text;
        for (const WindowOffset& o : offsets)
            text += (text.empty() ? "" : " ") + o.name + ":" + std::to_string(o.x) + "," + std::to_string(o.y);
        return text;
    }

    const std::vector<std::string>& AllWindows()
    {
        // Those HeadsUp names first, in its order, then the rest by their names.
        static const std::vector<std::string> all = [] {
            std::vector<std::string> names;
            for (const KnownWindow& w : kKnownWindows)
                names.emplace_back(w.key);
            for (std::string_view rest = kAllWindows; !rest.empty();)
            {
                const size_t end = std::min(rest.find(' '), rest.size());
                if (const std::string_view name = rest.substr(0, end);
                    !name.empty() && std::ranges::find(names, name) == names.end())
                    names.emplace_back(name);
                rest.remove_prefix(std::min(end + 1, rest.size()));
            }
            return names;
        }();
        return all;
    }

    const char* WindowLabel(std::string_view shortName)
    {
        for (const KnownWindow& w : kKnownWindows)
            if (shortName == w.key) return w.label;
        return nullptr;
    }

    bool WindowKeyList::Has(const WindowKey& key) const
    {
        const auto end = m_Keys.begin() + static_cast<std::ptrdiff_t>(m_Count);
        return std::find(m_Keys.begin(), end, key) != end;
    }

    bool WindowKeyList::Add(const WindowKey& key)
    {
        if (m_Count == m_Keys.size() || Has(key)) return false;
        m_Keys[m_Count++] = key;
        return true;
    }

    void WindowPlacements::SetFrame(uint32_t frame) { m_Frame = frame; }

    std::optional<WindowSpot> WindowPlacements::PlaceFor(uint32_t mcd, const WindowKey& key, WindowSpot now, int width,
        int height, int dx, int dy, int screenRight, int screenBottom)
    {
        Placed* placed = nullptr;
        for (Placed& p : m_Placed)
            if (p.mcd == mcd && p.key == key) placed = &p;
        if (placed == nullptr)
        {
            placed  = &m_Placed[m_Next];
            m_Next  = (m_Next + 1) % m_Placed.size();
            *placed = Placed{mcd, key, now, now};
        }
        else if (now != placed->to)
        {
            if (now == placed->from && placed->from != placed->to)
            {
                placed->bounces = m_Frame - placed->undone <= kBounceFrames ? placed->bounces + 1 : 1;
                placed->undone  = m_Frame;
                if (placed->bounces >= kBounces) m_SelfPlaced.Add(key);
            }
            placed->from = now; // the game put it there
        }
        placed->frame = m_Frame;
        const WindowSpot from = placed->from;
        int left = from.left;
        if (dx != 0) left = std::clamp(from.left + dx, 0, std::max(0, screenRight - width));
        int top = std::min(from.top + dy, screenBottom - height);
        top     = dy >= 0 ? std::max(top, from.top) : std::max(top, 0);
        placed->to = WindowSpot{left, top};
        return placed->to != now ? std::optional<WindowSpot>(placed->to) : std::nullopt;
    }

    WindowSpot PlacedText::Follow(WindowSpot seen, WindowSpot origin, WindowSpot at)
    {
        if (!m_Moved || seen != m_Given) m_Natural = seen;
        m_Given = WindowSpot{m_Natural.left + at.left - origin.left, m_Natural.top + at.top - origin.top};
        m_Moved = true;
        return m_Given;
    }

    WindowSpot PlacedText::Natural(WindowSpot seen) const { return m_Moved && seen == m_Given ? m_Natural : seen; }

    WindowSpot MovedNumber(WindowSpot number, WindowSpot at, int width, int height, WindowSpot to)
    {
        const bool inside = number.left >= at.left && number.left < at.left + width && number.top >= at.top &&
                            number.top < at.top + height;
        if (!inside || number == WindowSpot{}) return number;
        return WindowSpot{number.left + to.left - at.left, number.top + to.top - at.top};
    }

    std::vector<WindowPlacements::Placed> WindowPlacements::MovedSince(uint32_t since) const
    {
        std::vector<Placed> moved;
        for (const Placed& p : m_Placed)
            if (p.mcd != 0 && p.frame >= since && p.from != p.to) moved.push_back(p);
        return moved;
    }

    WindowSpot WindowPlacements::OriginOf(uint32_t mcd, const WindowKey& key, WindowSpot now) const
    {
        for (const Placed& p : m_Placed)
            if (p.mcd == mcd && p.key == key && p.to == now) return p.from;
        return now;
    }

    StripSites LocateStripText(const ImageSection& text, const ImageSection& data)
    {
        const auto help = FindAll(kHelpRecord, data), title = FindAll(kTitleRecord, data);
        const auto helpSet = FindAll(kHelpSetter, text), titleSet = FindAll(kTitleSetter, text);
        const auto setText = FindAll(kSetText, text), picker = FindAll(kLevelPicker, text);
        for (const auto* found : {&help, &title, &helpSet, &titleSet, &setText, &picker})
            if (found->empty()) return StripSites{LocateProblem::NoStripText};
        for (const auto* found : {&help, &title, &helpSet, &titleSet, &setText, &picker})
            if (found->size() != 1) return StripSites{LocateProblem::AmbiguousStripText};
        const uint32_t hs = helpSet.front(), ts = titleSet.front(), shared = text.rva + setText.front();
        const uint8_t helpText = text.bytes[hs + kHelpOffsetAt];
        if (text.bytes[hs + kHelpOffsetAgainAt] != helpText ||
            CallTarget(text.rva + hs + kHelpJumpAt, text) != shared ||
            CallTarget(text.rva + ts + kTitleJumpAt, text) != shared ||
            ReadAt<uint32_t>(text.bytes, picker.front() + kTopBufferAt) != kStripBuffers + (kStripLevels - 1) * kStripStride)
            return StripSites{LocateProblem::StripTextChanged};
        // A record at the very end of the section would hold its slot past it.
        for (const uint32_t record : {help.front(), title.front()})
            if (data.size - record < kCatalogSlot + sizeof(uint32_t))
                return StripSites{LocateProblem::StripTextChanged};
        return StripSites{LocateProblem::None, ReadAt<uint32_t>(data.bytes, help.front() + kCatalogSlot),
            ReadAt<uint32_t>(data.bytes, title.front() + kCatalogSlot), helpText,
            ReadAt<uint32_t>(text.bytes, ts + kTitleOffsetAt)};
    }

    bool IsStripWindow(const char* name, StripWindow window)
    {
        return name != nullptr &&
               std::memcmp(name, window == StripWindow::Help ? kHelpWindow : kTitleWindow, kWindowNameLength) == 0;
    }
}
