#include "DJControllerService.h"
#include "MP3Track.h"
#include "WAVTrack.h"
#include <iostream>
#include <memory>

DJControllerService::DJControllerService(size_t cache_size)
    : cache(cache_size) {}
/**
 * TODO: Implement loadTrackToCache method
 */
int DJControllerService::loadTrackToCache(AudioTrack& track) {
    //HIT
    if(cache.contains(track.get_title())){
        cache.get(track.get_title());
        return 1;
    }

    //MISS
    PointerWrapper<AudioTrack> clone = track.clone();
    //Clone failure 
    if (!clone) {
        std::cout << "[ERROR] Track: " <<track.get_title() <<" failed to clone";
        return 0;  
    }

    AudioTrack* clone_unwrapped=clone.release();
    if(clone_unwrapped==nullptr){
        throw std::runtime_error("Null pointer!");
    }
    clone_unwrapped->load();
    clone_unwrapped->analyze_beatgrid();
    PointerWrapper <AudioTrack> clone2(clone_unwrapped);
    bool is_evicted=cache.put(std::move(clone2));
    if(is_evicted){
        return -1;
    }
    return 0;
}

void DJControllerService::set_cache_size(size_t new_size) {
    cache.set_capacity(new_size);
}
//implemented
void DJControllerService::displayCacheStatus() const {
    std::cout << "\n=== Cache Status ===\n";
    cache.displayStatus();
    std::cout << "====================\n";
}

/**
 * TODO: Implement getTrackFromCache method
 */
AudioTrack* DJControllerService::getTrackFromCache(const std::string& track_title) {
    return cache.get(track_title);
}
