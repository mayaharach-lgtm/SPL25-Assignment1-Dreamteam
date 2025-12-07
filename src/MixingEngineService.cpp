#include "MixingEngineService.h"
#include <iostream>
#include <memory>


/**
 * TODO: Implement MixingEngineService constructor
 */
MixingEngineService::MixingEngineService(): decks(),active_deck(1), auto_sync(false),bpm_tolerance(0)
{
    decks[0]=nullptr;
    decks[1]=nullptr;
    std::cout <<"[MixingEngineService] Initialized with 2 empty decks."<<std::endl;
}

/**
 * TODO: Implement MixingEngineService destructor
 */
MixingEngineService::~MixingEngineService() {
    std::cout <<"[MixingEngineService] Cleaning up decks...."<<std::endl;
    if(decks[0]!=nullptr){
        delete decks[0];
        decks[0]=nullptr;
    }
    if(decks[1]!=nullptr){
        delete decks[1];
        decks[1]=nullptr;
    }
}


MixingEngineService::MixingEngineService(const MixingEngineService& other): decks(),
    active_deck(other.active_deck), auto_sync(other.auto_sync), bpm_tolerance(other.bpm_tolerance) 
{
    for (size_t i = 0; i < 2; i++) {
        if (other.decks[i] != nullptr) {
            decks[i] = other.decks[i]->clone().release();
        } else {
            decks[i] = nullptr;
        }
    }
}

MixingEngineService& MixingEngineService::operator=(const MixingEngineService& other) {
    if (this == &other) return *this;
    for (size_t i = 0; i < 2; i++) {
        delete decks[i];
        decks[i] = nullptr;
    }
    active_deck = other.active_deck;
    auto_sync = other.auto_sync;
    bpm_tolerance = other.bpm_tolerance;
    for (size_t i = 0; i < 2; i++) {
        if (other.decks[i] != nullptr) {
            decks[i] = other.decks[i]->clone().release();
        }
    }
    return *this;
}


/**
 * TODO: Implement loadTrackToDeck method
 * @param track: Reference to the track to be loaded
 * @return: Index of the deck where track was loaded, or -1 on failure
 */
int MixingEngineService::loadTrackToDeck(const AudioTrack& track) {
    
        PointerWrapper<AudioTrack> clone = track.clone();
        std::cout << "\n=== Loading Track to Deck ==="<<std::endl;

        if (!clone) {
            std::cout << "[ERROR] Track: " <<track.get_title() <<" failed to clone"<<std::endl;
            return -1;  
        }

        int target_deck=1-active_deck;
        std::cout << "[Deck Switch] Target deck: " <<target_deck<<std::endl;
    
    //Unload target deck if occupied
        if(decks[target_deck]!=nullptr){
            delete decks[target_deck];
            decks[target_deck]=nullptr;
        }
        clone->load();
        clone->analyze_beatgrid();

    //BPM Management
    if(decks[active_deck]!=nullptr && auto_sync){
        if(!can_mix_tracks(clone)){
            sync_bpm(clone);
        }
    }
    else if (decks[active_deck] == nullptr && auto_sync) {
        std::cout << "[Sync BPM] Cannot sync - one of the decks is empty." << std::endl;
    }
    
    decks[target_deck]=clone.release();
    std::cout << "[Load Complete] '" << decks[target_deck]->get_title() << "' is now loaded on deck " << target_deck << std::endl;
    
    //Instant Transition
    // if(decks[active_deck] != nullptr)
    //     std::cout << "[Unload] Unloading previous deck " <<active_deck << " " <<track.get_title()<<std::endl;
    // delete decks[active_deck];
    // decks[active_deck]=nullptr;

    active_deck=target_deck;
    std::cout << "[Active Deck] Switched to deck " <<target_deck <<std::endl;
    return target_deck;
    
}
        

/**
 * @brief Display current deck status
 */
void MixingEngineService::displayDeckStatus() const {
    std::cout << "\n=== Deck Status ===\n";
    for (size_t i = 0; i < 2; ++i) {
        if (decks[i])
            std::cout << "Deck " << i << ": " << decks[i]->get_title() << "\n";
        else
            std::cout << "Deck " << i << ": [EMPTY]\n";
    }
    std::cout << "Active Deck: " << active_deck << "\n";
    std::cout << "===================\n";
}

/**
 * TODO: Implement can_mix_tracks method
 * 
 * Check if two tracks can be mixed based on BPM difference.
 * 
 * @param track: Track to check for mixing compatibility
 * @return: true if BPM difference <= tolerance, false otherwise
 */
bool MixingEngineService::can_mix_tracks(const PointerWrapper<AudioTrack>& track) const {
    // Your implementation here
    bool canmixtracks=false;
    if(decks[active_deck]!=nullptr){
        if(track){
            int newtrackbpm=track.get()->get_bpm();
            int currtrackbpm=decks[active_deck]->get_bpm();
            int sub= std::abs(newtrackbpm-currtrackbpm);
            if(sub<=bpm_tolerance){
                canmixtracks=true;
            }
        }
    }
    return canmixtracks;
}

/**
 * TODO: Implement sync_bpm method
 * @param track: Track to synchronize with active deck
 */
        

void MixingEngineService::sync_bpm(const PointerWrapper<AudioTrack>& track) const {
    if(decks[active_deck]!=nullptr){
        if(track){
            int newtrackbpm=track.get()->get_bpm();
            int currtrackbpm=decks[active_deck]->get_bpm();
            int avgbpm= (newtrackbpm+currtrackbpm)/2;
            track.get()->set_bpm(avgbpm);
            std::cout << "[Sync BPM] Syncing BPM from " << newtrackbpm << " to " << avgbpm << std::endl;
        }
    }
}
