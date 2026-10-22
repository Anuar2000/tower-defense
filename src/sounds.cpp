#include "sounds.h"
#include <cstdio>
#include <map>
#include <vector>
#include "miniaudio.h"
#include "game.h"
#include "settings.h"

const std::string FOLDER = std::string(ASSET_DIR) + "/sounds/";
const std::string MUSIC_FILE = FOLDER + "music.mp3";
const double MUSIC_LOOP_END = 58.3425;  // секунд: осы жерден трек басымен сәйкес келеді, соңындағы өшу циклге кірмейді
const int VOICES = 15;  // бір уақытта 15 эффектіге дейін (pygame-дағы 16 арнаның біреуі — музыкаға)

// Дыбыс: файлдар (біреуі кездейсоқ таңдалады), дауыс деңгейі 0..1, қайталау арасындағы ең аз уақыт мс
struct SoundInfo {
    std::vector<std::string> files;
    float volume;
    int gap;
};

const std::map<std::string, SoundInfo> SOUND_LIST = {
    {"shoot", {{"laserSmall_000.ogg", "laserSmall_001.ogg", "laserSmall_002.ogg"}, 0.6f, 60}},
    {"hit", {{"impactGeneric_light_000.ogg", "impactGeneric_light_001.ogg", "impactGeneric_light_002.ogg"}, 0.7f, 50}},
    {"death_walker", {{"explosionCrunch_000.ogg"}, 0.5f, 60}},
    {"death_bat", {{"phaserDown2.ogg"}, 0.8f, 60}},
    {"death_tank", {{"lowFrequency_explosion_000.ogg"}, 1.0f, 0}},
    {"volley", {{"laserLarge_000.ogg"}, 0.8f, 0}},
    {"tower_hit", {{"impactWood_heavy_000.ogg", "impactWood_heavy_001.ogg", "impactWood_heavy_002.ogg"}, 0.9f, 150}},
    {"coin", {{"highUp.ogg"}, 0.25f, 200}},
    {"wave_start", {{"jingles_NES09.ogg"}, 0.7f, 0}},
    {"wave_clear", {{"jingles_NES03.ogg"}, 0.6f, 0}},
    {"win", {{"jingles_NES12.ogg"}, 0.7f, 0}},
    {"lose", {{"jingles_NES11.ogg"}, 0.7f, 0}},
    {"target", {{"select_001.ogg"}, 0.6f, 0}},
    {"click", {{"click_001.ogg"}, 1.0f, 0}},
    {"hover", {{"tick_002.ogg"}, 0.4f, 0}},
    {"menu_open", {{"open_001.ogg"}, 0.4f, 0}},
    {"menu_close", {{"close_001.ogg"}, 0.4f, 0}},
    {"buy", {{"confirmation_001.ogg"}, 0.6f, 0}},
    {"error", {{"error_004.ogg"}, 0.6f, 0}},
    {"freeze", {{"impactGlass_heavy_000.ogg"}, 0.8f, 0}},
    {"shield", {{"forceField_000.ogg"}, 0.7f, 0}},
    {"overdrive", {{"powerUp2.ogg"}, 0.6f, 0}},
    {"plane", {{"thrusterFire_000.ogg"}, 0.35f, 0}},
    {"bomb_fall", {{"phaserDown1.ogg"}, 0.5f, 0}},
    {"explosion", {{"lowFrequency_explosion_000.ogg"}, 0.9f, 0}},
};

ma_engine engine;
bool audio_ready = false;  // дыбыс құрылғысы ашылмаса, ойын үнсіз жүре береді
std::map<std::string, ma_sound_group> groups;  // әр дыбыс атауының өз дауыс деңгейі
std::map<std::string, ma_sound> loaded;  // жадқа жүктелген файлдар (келесі ашу бірден болады)
ma_sound voices[VOICES];  // қазір ойнап жатқан эффектілер
bool voice_used[VOICES] = {};
std::map<std::string, int> last_played;  // әр дыбыс соңғы рет қашан ойналды (мс)
ma_sound music;
bool music_ready = false;


void load_sounds() {
    audio_ready = ma_engine_init(nullptr, &engine) == MA_SUCCESS;
    if (!audio_ready) {
        printf("Дыбыс құрылғысы ашылмады\n");
        return;
    }
    for (const auto& [name, info] : SOUND_LIST) {
        ma_sound_group_init(&engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &groups[name]);
        for (const std::string& file : info.files) {
            if (!loaded.count(file)) {
                // MA_SOUND_FLAG_DECODE: файлды бірден толық ашып, жадта ұстаймыз
                ma_sound_init_from_file(&engine, (FOLDER + file).c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &loaded[file]);
            }
        }
    }
    apply_volumes();
}


// Параметрлердегі сырғытпалар бойынша барлық дыбыстың деңгейін қайта есептейміз
void apply_volumes() {
    if (!audio_ready) {
        return;
    }
    float master = options["sound_on"] ? options["master"] : 0;
    for (auto& [name, group] : groups) {
        ma_sound_group_set_volume(&group, SOUND_LIST.at(name).volume * options["sfx"] * master);
    }
    if (music_ready) {
        ma_sound_set_volume(&music, MUSIC_MAX * options["music"] * master);
    }
}


// Эффектілерді тоқтату (музыкаға тимейміз)
void stop_effects() {
    for (int i = 0; i < VOICES; i++) {
        if (voice_used[i]) {
            ma_sound_stop(&voices[i]);
        }
    }
}


// Дыбысты ойнату: жүктелмесе, өшірулі болса немесе тым жиі болса — үндемейміз
void play(const std::string& name) {
    if (!audio_ready || !options["sound_on"] || !SOUND_LIST.count(name)) {
        return;
    }
    int now = get_ticks();
    if (last_played.count(name) && now - last_played[name] < SOUND_LIST.at(name).gap) {
        return;
    }
    last_played[name] = now;

    // Бос «дауыс» іздейміз: бәрі бос емес болса, дыбыс шықпайды (pygame сияқты)
    for (int i = 0; i < VOICES; i++) {
        if (voice_used[i] && ma_sound_is_playing(&voices[i])) {
            continue;
        }
        if (voice_used[i]) {
            ma_sound_uninit(&voices[i]);
        }
        const std::vector<std::string>& files = SOUND_LIST.at(name).files;
        std::string file = files[random_int(0, (int)files.size() - 1)];
        ma_sound_init_from_file(&engine, (FOLDER + file).c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                &groups[name], nullptr, &voices[i]);
        ma_sound_start(&voices[i]);
        voice_used[i] = true;
        return;
    }
}


// Музыка шексіз қайталанады, бірақ тек цикл соңына дейін
void start_music() {
    if (!audio_ready) {
        return;
    }
    if (ma_sound_init_from_file(&engine, MUSIC_FILE.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                nullptr, nullptr, &music) != MA_SUCCESS) {
        return;
    }
    music_ready = true;
    ma_uint32 rate;  // секундына неше сэмпл
    ma_sound_get_data_format(&music, nullptr, nullptr, &rate, nullptr, 0);
    ma_data_source_set_loop_point_in_pcm_frames(ma_sound_get_data_source(&music), 0, (ma_uint64)(MUSIC_LOOP_END * rate));
    ma_sound_set_looping(&music, MA_TRUE);
    apply_volumes();
    ma_sound_start(&music);
}
