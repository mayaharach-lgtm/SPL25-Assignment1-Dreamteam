#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

// Include your project headers
#include "AudioTrack.h"
#include "MP3Track.h"
#include "WAVTrack.h"
#include "Playlist.h"
#include "PointerWrapper.h"
#include "LRUCache.h"

// --- Testing Framework Macros ---
#define PASS(msg) std::cout << "\033[1;32m[PASS]\033[0m " << msg << std::endl
#define FAIL(msg) do { std::cerr << "\033[1;31m[FAIL]\033[0m " << msg << " (Line " << __LINE__ << ")" << std::endl; std::exit(1); } while(0)
#define ASSERT_TRUE(cond, msg) if(!(cond)) FAIL(msg)
#define ASSERT_THROWS(code, ExceptionType, msg) \
    do { \
        bool caught = false; \
        try { code; } catch(const ExceptionType& e) { caught = true; } \
        if(!caught) FAIL(msg " (Expected exception not thrown)"); \
        else PASS(msg); \
    } while(0)

// Helper to create tracks
PointerWrapper<AudioTrack> make_mp3(const std::string& title) {
    std::vector<std::string> artists = {"Artist"};
    return PointerWrapper<AudioTrack>(new MP3Track(title, artists, 180, 120, 320));
}

// ==========================================
// 1. ADVANCED POINTER WRAPPER TESTS
// ==========================================
void test_pointer_wrapper_edge_cases() {
    std::cout << "\n--- Edge Cases: PointerWrapper ---" << std::endl;

    // Test 1: Dereferencing Null Pointer
    // מצופה שהקוד יזרוק שגיאה ולא יקרוס (SegFault)
    PointerWrapper<int> null_wrapper;
    ASSERT_THROWS(*null_wrapper, std::runtime_error, "Dereferencing null wrapper should throw");

    // Test 2: Self-Assignment via Move
    // בדיקה שהעברה של אובייקט לעצמו לא מוחקת את המידע
    int* val = new int(42);
    PointerWrapper<int> w1(val);
    w1 = std::move(w1); 
    
    if (w1.get() == nullptr) FAIL("Self-move assignment destroyed the object!");
    if (*w1 != 42) FAIL("Self-move assignment corrupted data!");
    PASS("Self-move assignment handled correctly");

    // Test 3: Reset with same pointer
    // בדיקה שקריאה ל-reset עם אותו מצביע לא גורמת ל-Double Free
    w1.reset(val);
    if (*w1 != 42) FAIL("Reset with same pointer corrupted data!");
    PASS("Reset with same pointer is safe");

    // Test 4: Reset with nullptr
    w1.reset(nullptr);
    if (static_cast<bool>(w1)) FAIL("Wrapper should be empty after reset(nullptr)");
    PASS("Reset to nullptr works");
}

// ==========================================
// 2. ADVANCED LRU CACHE TESTS
// ==========================================
void test_lru_logic_complex() {
    std::cout << "\n--- Edge Cases: LRU Cache Logic ---" << std::endl;

    // Scenario: Access Pattern Logic
    // Cache Size: 3
    LRUCache cache(3);
    
    // Insert A, B, C
    cache.put(make_mp3("A")); // Time: 0
    cache.put(make_mp3("B")); // Time: 1
    cache.put(make_mp3("C")); // Time: 2
    // Current State (LRU to MRU): A, B, C

    // Access A -> Should become MRU
    AudioTrack* ptrA = cache.get("A"); // Time: 3
    ASSERT_TRUE(ptrA != nullptr, "Track A should exist");
    // New State (LRU to MRU): B, C, A

    // Insert D -> Should evict B (Logic: B has lowest time 1, C has 2, A has 3)
    bool evicted = cache.put(make_mp3("D")); // Time: 4
    
    ASSERT_TRUE(evicted, "Insertion should cause eviction");
    ASSERT_TRUE(!cache.contains("B"), "Track B (LRU) should be evicted");
    ASSERT_TRUE(cache.contains("A"), "Track A (recently accessed) should remain");
    ASSERT_TRUE(cache.contains("C"), "Track C should remain");
    ASSERT_TRUE(cache.contains("D"), "Track D should exist");
    PASS("LRU Logic: Update on Get() prevents eviction");

    // Scenario: Update Existing Item on Put
    // Insert C again -> Should update C's time and NOT duplicate
    cache.put(make_mp3("C")); // Time: 5
    // State (LRU to MRU): A(3), D(4), C(5)
    
    // Insert E -> Should evict A
    cache.put(make_mp3("E")); // Time: 6
    
    ASSERT_TRUE(!cache.contains("A"), "Track A should be evicted (was LRU)");
    ASSERT_TRUE(cache.contains("C"), "Track C should remain (was updated by Put)");
    PASS("LRU Logic: Put() of existing track updates priority");
}

void test_lru_edge_capacities() {
    std::cout << "\n--- Edge Cases: LRU Capacities ---" << std::endl;

    // Test Capacity 1
    LRUCache tiny_cache(1);
    tiny_cache.put(make_mp3("X"));
    bool evicted = tiny_cache.put(make_mp3("Y"));
    
    ASSERT_TRUE(evicted, "Capacity 1: Must evict on second insert");
    ASSERT_TRUE(!tiny_cache.contains("X"), "X should be gone");
    ASSERT_TRUE(tiny_cache.contains("Y"), "Y should be present");
    PASS("Capacity 1 cache works correctly");
}

// ==========================================
// 3. PLAYLIST & OWNERSHIP TESTS
// ==========================================
void test_playlist_robustness() {
    std::cout << "\n--- Edge Cases: Playlist ---" << std::endl;

    Playlist p("Stress Test");
    
    // Test 1: Removing non-existent track
    // Shouldn't crash
    try {
        p.remove_track("Ghost Track");
        PASS("Removing non-existent track didn't crash");
    } catch (...) {
        FAIL("Removing non-existent track caused exception");
    }

    // Test 2: Deep Copy Verification
    // If Playlist owns tracks (Phase 1/2), copy must be deep!
    std::vector<std::string> artists = {"Me"};
    p.add_track(new MP3Track("Original", artists, 100, 100, 320));
    
    Playlist p_copy = p; // Copy Constructor
    
    // Verify addresses are different (Deep Copy)
    AudioTrack* t1 = p.getTracks()[0];
    AudioTrack* t2 = p_copy.getTracks()[0];
    
    ASSERT_TRUE(t1 != t2, "Playlist Copy failed: Shallow copy detected! (Pointers are same)");
    ASSERT_TRUE(t1->get_title() == t2->get_title(), "Playlist Copy content mismatch");
    
    // Modify copy (if possible via setters) or just ensure independence
    // (Assuming set_bpm exists from previous context)
    t2->set_bpm(999);
    ASSERT_TRUE(t1->get_bpm() != 999, "Modifying copy affected original! (Shared state)");
    
    PASS("Playlist Deep Copy Verified");
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "   RUNNING COMPREHENSIVE EDGE CASE TESTS    " << std::endl;
    std::cout << "============================================" << std::endl;

    try {
        test_pointer_wrapper_edge_cases();
        test_lru_logic_complex();
        test_lru_edge_capacities();
        test_playlist_robustness();
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL: Uncaught exception in test runner: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n\033[1;32mALL COMPREHENSIVE TESTS PASSED!\033[0m" << std::endl;
    std::cout << "Your code is robust and handles edge cases well." << std::endl;
    return 0;
}