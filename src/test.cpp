#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <stdexcept>

// Project headers
#include "AudioTrack.h"
#include "MP3Track.h"
#include "WAVTrack.h"
#include "Playlist.h"
#include "PointerWrapper.h"
#include "LRUCache.h"
#include "CacheSlot.h"
#include "DJLibraryService.h"
#include "DJControllerService.h"
#include "MixingEngineService.h"
#include "DJSession.h"

// ---------- Simple test harness ----------

#define RUN_TEST(fn) do { \
    std::cout << "\n====================================\n"; \
    std::cout << "[ RUN  ] " << #fn << std::endl; \
    std::cout << "------------------------------------\n"; \
    try { \
        fn(); \
        std::cout << "[ PASS ] " << #fn << std::endl; \
    } catch (const std::exception& e) { \
        std::cout << "[ FAIL ] " << #fn << " - exception: " << e.what() << std::endl; \
    } catch (...) { \
        std::cout << "[ FAIL ] " << #fn << " - unknown exception\n"; \
    } \
    std::cout << "====================================\n"; \
} while(0)

static void debug_ptr(const char* name, const void* p) {
    std::cout << "    [DBG ] " << name << " = " << p << std::endl;
}

// ======================================================
//  PHASE 1: PLAYLIST TESTS (OWNERSHIP & LINKED LIST)
// ======================================================

// Basic add/remove + ownership model
void test_playlist_basic_add_remove() {
    std::cout << "[INFO] Creating 2 tracks and a playlist..." << std::endl;
    AudioTrack* mp3 = new MP3Track("P_Test_1", {"ArtistA"}, 180, 120, 320, true);
    AudioTrack* wav = new WAVTrack("P_Test_2", {"ArtistB"}, 200, 128, 44100, 16);

    Playlist* pl = new Playlist("TestPlaylist");

    pl->add_track(mp3);
    pl->add_track(wav);

    std::cout << "[INFO] Playlist after adding 2 tracks:" << std::endl;
    pl->display();

    std::cout << "[INFO] Removing one track (P_Test_2)..." << std::endl;
    pl->remove_track("P_Test_2");
    pl->display();

    std::cout << "[INFO] Deleting playlist (should delete nodes only, not tracks, "
              << "if Playlist is a borrower)." << std::endl;
    delete pl;

    std::cout << "[INFO] Deleting tracks manually. If Playlist also deletes tracks, "
              << "you will see double-delete / crash / valgrind errors here." << std::endl;
    delete mp3;
    delete wav;
}

// Edge cases: removing from empty playlist, removing non-existing track
void test_playlist_edge_cases() {
    Playlist pl("EdgePlaylist");

    std::cout << "[INFO] Removing from empty playlist (should not crash)..." << std::endl;
    pl.remove_track("DoesNotExist");

    AudioTrack* t = new MP3Track("EdgeTrack", {"Artist"}, 100, 100, 128, false);
    pl.add_track(t);

    std::cout << "[INFO] Removing non-existing track from non-empty playlist..." << std::endl;
    pl.remove_track("OtherTrack");

    std::cout << "[INFO] Playlist should still contain EdgeTrack:" << std::endl;
    pl.display();

    // Playlist is on the stack, so no manual delete for it.
    // But Playlist does NOT own tracks in your current implementation,
    // so we must delete the track ourselves to avoid leaks.
    delete t;
}

// ======================================================
//  PHASE 2: AUDIOTRACK RULE OF 5 TESTS
// ======================================================

void test_audiotrack_copy_constructor() {
    MP3Track original("Original", {"Artist1"}, 200, 128, 256, true);

    std::cout << "[INFO] Creating copy using copy constructor..." << std::endl;
    MP3Track copy = original;

    double buf_orig[10] = {0};
    double buf_copy[10] = {0};

    original.get_waveform_copy(buf_orig, 10);
    copy.get_waveform_copy(buf_copy, 10);

    std::cout << "[INFO] Comparing first 10 waveform samples (should be equal)..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        if (buf_orig[i] != buf_copy[i]) {
            std::cout << "[ERROR] Waveform mismatch at index " << i
                      << ": " << buf_orig[i] << " vs " << buf_copy[i] << std::endl;
        }
    }

    std::cout << "[NOTE] If copy constructor is shallow or buggy, expect "
              << "memory issues or identical pointers for waveform_data." << std::endl;
}

void test_audiotrack_copy_assignment_self() {
    MP3Track t("Self", {"S"}, 150, 110, 192, false);

    std::cout << "[INFO] Performing self-assignment t = t..." << std::endl;
    t = t;

    std::cout << "[INFO] Self-assignment completed without crash. "
              << "If you don't guard self-assignment, you might see double delete." << std::endl;
}

void test_audiotrack_move_constructor() {
    MP3Track src("MoveSrc", {"M"}, 210, 123, 320, true);

    std::cout << "[INFO] Creating dest using move constructor: "
              << "MP3Track dest = std::move(src)..." << std::endl;
    MP3Track dest = std::move(src);

    std::cout << "[NOTE] If move constructor doesn't null out src.waveform_data, "
              << "you may get double free when src is destroyed." << std::endl;
}

void test_audiotrack_move_assignment() {
    MP3Track a("A", {"A"}, 100, 100, 128, false);
    MP3Track b("B", {"B"}, 200, 130, 320, true);

    std::cout << "[INFO] Performing move assignment: a = std::move(b)..." << std::endl;
    a = std::move(b);

    std::cout << "[NOTE] If move assignment doesn't release a's old resource or doesn't null "
              << "b.waveform_data, you will see leaks or double deletes." << std::endl;
}

// ======================================================
//  PHASE 3: POINTERWRAPPER TESTS
// ======================================================

void test_pointerwrapper_basic() {
    std::cout << "[INFO] Creating PointerWrapper<MP3Track> pw(new MP3Track(...))" << std::endl;
    PointerWrapper<MP3Track> pw(new MP3Track("PWTrack", {"PW"}, 120, 123, 320, true));

    std::cout << "[INFO] Testing operator-> and operator*..." << std::endl;
    if (pw) {
        std::cout << "    Title via -> : " << pw->get_title() << std::endl;
        std::cout << "    BPM via *   : " << (*pw).get_bpm() << std::endl;
    } else {
        std::cout << "[ERROR] PointerWrapper is null but expected non-null." << std::endl;
    }

    std::cout << "[INFO] Resetting wrapper to a new object..." << std::endl;
    pw.reset(new MP3Track("PWTrack2", {"PW2"}, 150, 110, 256, false));
    if (pw) {
        std::cout << "    Title after reset: " << pw->get_title() << std::endl;
    }

    std::cout << "[NOTE] If reset() does not delete the old pointer, you'll see leaks. "
              << "If it deletes the new pointer by mistake or double-deletes, you'll see crashes." << std::endl;
}

// This test is specifically designed to catch a buggy move assignment implementation
// such as: delete ptr; ptr = other.ptr; delete other.ptr; other.ptr = nullptr;
// which would double delete the same pointer.
void test_pointerwrapper_move_assignment_sanity() {
    PointerWrapper<MP3Track> p1(new MP3Track("Move1", {"M1"}, 100, 100, 128, false));
    PointerWrapper<MP3Track> p2(new MP3Track("Move2", {"M2"}, 200, 130, 320, true));

    std::cout << "[DBG ] Before move-assignment:" << std::endl;
    debug_ptr("p1.get()", p1 ? p1.operator->() : nullptr);
    debug_ptr("p2.get()", p2 ? p2.operator->() : nullptr);

    std::cout << "[INFO] Performing p2 = std::move(p1)..." << std::endl;
    p2 = std::move(p1);

    std::cout << "[DBG ] After move-assignment:" << std::endl;
    std::cout << "    p1 (bool) = " << (p1 ? "true" : "false") << std::endl;
    if (p2) {
        debug_ptr("p2.get()", p2.operator->());
        std::cout << "    p2 title: " << p2->get_title() << std::endl;
    }

    std::cout << "[WARN] If your move assignment deletes other.ptr after assigning it to ptr, "
              << "you will get a double delete at the end of the program. "
              << "Correct pattern is: delete ptr; ptr = other.ptr; other.ptr = nullptr;" << std::endl;
}

void test_pointerwrapper_release() {
    PointerWrapper<MP3Track> pw(new MP3Track("ReleaseTrack", {"R"}, 220, 125, 320, true));

    std::cout << "[INFO] Calling release() (ownership must be transferred out)..." << std::endl;
    MP3Track* raw = pw.release();
    debug_ptr("released raw", raw);
    std::cout << "    pw (bool) after release: " << (pw ? "true" : "false") << std::endl;

    std::cout << "[INFO] Deleting raw manually..." << std::endl;
    delete raw;

    std::cout << "[NOTE] If release() does not null out the internal pointer, "
              << "wrapper will double-delete at destruction." << std::endl;
}

// ======================================================
//  PHASE 4: LRUCache / CONTROLLER / MIXER TESTS
// ======================================================

// LRUCache basic put/get + eviction behavior
void test_lru_cache_basic_put_get_and_eviction() {
    std::cout << "[INFO] Creating LRUCache with capacity = 2..." << std::endl;
    LRUCache cache(2);

    PointerWrapper<AudioTrack> t1(new MP3Track("T1", {"A"}, 100, 100, 128, false));
    PointerWrapper<AudioTrack> t2(new MP3Track("T2", {"B"}, 120, 110, 192, true));
    PointerWrapper<AudioTrack> t3(new MP3Track("T3", {"C"}, 140, 120, 320, true));

    std::cout << "[INFO] Putting T1 (should be MISS, no eviction)..." << std::endl;
    bool ev1 = cache.put(std::move(t1));
    std::cout << "    eviction flag: " << (ev1 ? "true" : "false") << std::endl;

    std::cout << "[INFO] Putting T2 (should be MISS, no eviction)..." << std::endl;
    bool ev2 = cache.put(std::move(t2));
    std::cout << "    eviction flag: " << (ev2 ? "true" : "false") << std::endl;

    assert(cache.contains("T1"));
    assert(cache.contains("T2"));
    assert(!cache.contains("T3"));

    // Access T1 to make T2 the least recently used
    std::cout << "[INFO] Accessing T1 to update LRU state..." << std::endl;
    AudioTrack* a1 = cache.get("T1");
    if (!a1) {
        std::cout << "[ERROR] Expected T1 in cache, got nullptr." << std::endl;
    }

    std::cout << "[INFO] Now inserting T3. Cache is full, so LRU (T2) should be evicted." << std::endl;
    bool ev3 = cache.put(std::move(t3));
    std::cout << "    eviction flag: " << (ev3 ? "true" : "false") << std::endl;

    // Expected state after correct LRU:
    // - T1 is still in cache (recently used)
    // - T3 is in cache (just inserted)
    // - T2 was evicted
    bool hasT1 = cache.contains("T1");
    bool hasT2 = cache.contains("T2");
    bool hasT3 = cache.contains("T3");

    std::cout << "[CHECK] contains(T1) = " << (hasT1 ? "true" : "false") << " (expected: true)" << std::endl;
    std::cout << "[CHECK] contains(T2) = " << (hasT2 ? "true" : "false") << " (expected: false)" << std::endl;
    std::cout << "[CHECK] contains(T3) = " << (hasT3 ? "true" : "false") << " (expected: true)" << std::endl;

    if (!hasT1 || hasT2 || !hasT3) {
        std::cout << "[ERROR] LRU eviction behavior is incorrect. "
                  << "Likely a bug in findLRUSlot() or put()." << std::endl;
    }

    std::cout << "[INFO] Current cache status:" << std::endl;
    cache.displayStatus();
}

// Test that LRUCache::put throws on null PointerWrapper (according to your implementation)
void test_lru_cache_null_put_throws() {
    std::cout << "[INFO] Testing LRUCache::put with empty PointerWrapper (should throw std::runtime_error)..." << std::endl;
    LRUCache cache(1);
    PointerWrapper<AudioTrack> empty; // ptr == nullptr

    try {
        cache.put(std::move(empty));
        std::cout << "[ERROR] Expected exception but none was thrown." << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "[PASS ] Caught expected std::runtime_error: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "[ERROR] Caught unexpected exception type." << std::endl;
    }
}

void test_djcontroller_loadTrackToCache() {
    std::cout << "[INFO] Testing DJControllerService::loadTrackToCache..." << std::endl;
    DJControllerService controller(2);

    MP3Track track("CTRL_T1", {"C1"}, 180, 125, 320, true);

    int res1 = controller.loadTrackToCache(track);
    std::cout << "    First load result (expected MISS 0 or MISS-with-eviction -1, but since cache is empty, probably 0): "
              << res1 << std::endl;

    int res2 = controller.loadTrackToCache(track);
    std::cout << "    Second load result (expected HIT 1): " << res2 << std::endl;

    std::cout << "[INFO] Getting track back from cache..." << std::endl;
    AudioTrack* cached = controller.getTrackFromCache("CTRL_T1");
    if (!cached) {
        std::cout << "[ERROR] getTrackFromCache returned nullptr for CTRL_T1" << std::endl;
    } else {
        std::cout << "    Cached title: " << cached->get_title() << std::endl;
    }
}

void test_mixingengine_basic_decks() {
    std::cout << "[INFO] Testing MixingEngineService basic deck loading and switching..." << std::endl;
    MixingEngineService mixer;

    MP3Track t1("DeckOne", {"D1"}, 200, 120, 320, true);
    MP3Track t2("DeckTwo", {"D2"}, 210, 122, 256, false);

    std::cout << "[INFO] Loading t1 to mixer..." << std::endl;
    int d0 = mixer.loadTrackToDeck(t1);
    std::cout << "    Returned deck index for t1: " << d0 << std::endl;
    mixer.displayDeckStatus();

    std::cout << "[INFO] Loading t2 to mixer (should go to the other deck, then unload previous deck)..." << std::endl;
    int d1 = mixer.loadTrackToDeck(t2);
    std::cout << "    Returned deck index for t2: " << d1 << std::endl;
    mixer.displayDeckStatus();

    std::cout << "[NOTE] If loadTrackToDeck clones incorrectly, forgets to release, "
              << "or deletes the wrong deck, expect crashes or leaks here / under valgrind." << std::endl;
}

// ======================================================
//  DJSession TESTS (AUTO + INTERACTIVE SIMULATION)
// ======================================================

// This is mostly a smoke test: full correctness is checked by the program logic itself,
// but we want to ensure simulate_dj_performance() runs end-to-end without hanging/crashing.

void test_djsession_auto_simulation() {
    std::cout << "[INFO] Starting DJSession AUTO simulation test (play_all = true)..." << std::endl;
    std::cout << "[INFO] This uses bin/dj_config.txt from the project. "
              << "If the config file is missing or broken, you will see [ERROR] messages." << std::endl;

    DJSession session("AutoSimTest", /*play_all=*/true);
    session.simulate_dj_performance();

    std::cout << "[INFO] Reached end of simulate_dj_performance() in AUTO mode." << std::endl;
    std::cout << "[HINT] To check for memory leaks during this full run, execute under valgrind "
              << "or use make test-leaks." << std::endl;
}

// We fake stdin so the interactive mode won't block on user input.
// NOTE: This assumes display_playlist_menu_from_config() uses std::cin / getline.
void test_djsession_interactive_simulation_with_fake_input() {
    std::cout << "[INFO] Starting DJSession INTERACTIVE simulation test (play_all = false)..." << std::endl;
    std::cout << "[INFO] Redirecting std::cin to a fake input stream (\"q\\n\")." << std::endl;
    std::cout << "[INFO] The idea is to simulate a user who immediately quits the menu." << std::endl;

    DJSession session("InteractiveSimTest", /*play_all=*/false);

    std::stringstream fakeInput;
    fakeInput << "q\n";  // or any input your menu treats as "quit" / invalid -> eventually empty

    std::streambuf* originalCinBuf = std::cin.rdbuf(fakeInput.rdbuf());

    session.simulate_dj_performance();

    // Restore std::cin buffer
    std::cin.rdbuf(originalCinBuf);

    std::cout << "[INFO] Reached end of simulate_dj_performance() in INTERACTIVE mode with fake input." << std::endl;
    std::cout << "[NOTE] If the interactive path never exits when cin hits EOF or 'q', "
              << "you will see an infinite loop / hang instead of this message." << std::endl;
}

// ======================================================
//  MAIN
// ======================================================

int main() {
    std::cout << "=====================================\n";
    std::cout << "   DJ Assignment - Custom Test Suite \n";
    std::cout << "=====================================\n";

    // Phase 1
    RUN_TEST(test_playlist_basic_add_remove);
    RUN_TEST(test_playlist_edge_cases);

    // Phase 2
    RUN_TEST(test_audiotrack_copy_constructor);
    RUN_TEST(test_audiotrack_copy_assignment_self);
    RUN_TEST(test_audiotrack_move_constructor);
    RUN_TEST(test_audiotrack_move_assignment);

    // Phase 3
    RUN_TEST(test_pointerwrapper_basic);
    RUN_TEST(test_pointerwrapper_move_assignment_sanity);
    RUN_TEST(test_pointerwrapper_release);

    // Phase 4 core components (including hard LRU cache tests)
    RUN_TEST(test_lru_cache_basic_put_get_and_eviction);
    RUN_TEST(test_lru_cache_null_put_throws);
    RUN_TEST(test_djcontroller_loadTrackToCache);
    RUN_TEST(test_mixingengine_basic_decks);

    // DJSession simulations (AUTO + INTERACTIVE)
    RUN_TEST(test_djsession_auto_simulation);
    RUN_TEST(test_djsession_interactive_simulation_with_fake_input);

    std::cout << "\n[INFO] All tests finished running. See PASS/FAIL above." << std::endl;
    std::cout << "[INFO] For memory issues (leaks, double frees, dangling pointers), "
              << "run this binary under valgrind or use: make test-leaks." << std::endl;

    return 0;
}
