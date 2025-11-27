#include "DJLibraryService.h"
#include "SessionFileParser.h"
#include "MP3Track.h"
#include "WAVTrack.h"
#include <iostream>
#include <memory>
#include <filesystem>


DJLibraryService::DJLibraryService(const Playlist& playlist) 
    : playlist(playlist) {}


//DELETE DISTRUCTOR? 
//OR IMPLEMENT RULE OF 3? NADAV AND MAYA
DJLibraryService::~DJLibraryService(){
    for(size_t i=0;i<library.size();i++){
        delete library[i];
    }
}

/**
 * @brief Load a playlist from track indices referencing the library
 * @param library_tracks Vector of track info from config
 */
void DJLibraryService::buildLibrary(const std::vector<SessionConfig::TrackInfo>& library_tracks) {
    for(const SessionConfig::TrackInfo& info : library_tracks){
        AudioTrack* track=nullptr;
        if(info.type== "MP3"){
            track = new MP3Track(info.title, info.artists, info.duration_seconds, info.bpm, info.extra_param1, info.extra_param2); 
            std::cout<<"MP3Track created: "<< static_cast<MP3Track*>(track)->get_bitrate() << "kbps";
        }
        //else- WAV track
        else{
            track = new WAVTrack(info.title, info.artists, info.duration_seconds, info.bpm, info.extra_param1, info.extra_param2); 
            std::cout<<" WAVTrack created: "<< static_cast<WAVTrack*>(track)->get_sample_rate() << "Hz/" << static_cast<WAVTrack*>(track)->get_bit_depth() <<"bit";
        }
            playlist.add_track(track);
            
    }
     std::cout<< "[INFO] Track library built:" << library_tracks.size() << "tracks loaded";
}


/**
 * @brief Display the current state of the DJ library playlist
 * 
 */
void DJLibraryService::displayLibrary() const {
    std::cout << "=== DJ Library Playlist: " 
              << playlist.get_name() << " ===" << std::endl;

    if (playlist.is_empty()) {
        std::cout << "[INFO] Playlist is empty.\n";
        return;
    }

    // Let Playlist handle printing all track info
    playlist.display();

    std::cout << "Total duration: " << playlist.get_total_duration() << " seconds" << std::endl;
}

/**
 * @brief Get a reference to the current playlist
 * 
 * @return Playlist& 
 */
Playlist& DJLibraryService::getPlaylist() {
    // Your implementation here
    return playlist;
}

/**
 * TODO: Implement findTrack method
 * 
 * HINT: Leverage Playlist's find_track method
 */
AudioTrack* DJLibraryService::findTrack(const std::string& track_title) {
    return playlist.find_track(track_title);
}

void DJLibraryService::loadPlaylistFromIndices(const std::string& playlist_name, 
                                               const std::vector<int>& track_indices) {
    std::cout<< "[INFO] Loading playlist:" << playlist_name;
    Playlist *newplaylist = new Playlist(playlist_name);
    int counter=0;
    for(size_t index : track_indices){
        if(index<= library.size()){
            AudioTrack* track=library[index-1];
            PointerWrapper <AudioTrack> clone = track->clone();
            if(!clone){
                std:: cout << "[ERROR] Track:" <<track->get_title() << "failed to clone";
                continue;
            }
            clone.get()->load();
            clone.get()->analyze_beatgrid();
            // playlist.add_track(clone.get()); //notice maybe need to replace with row under - double delete
            newplaylist->add_track(clone.release());
            counter++;
            std::cout<< " Added " <<clone.get()->get_title() <<" to playlist " <<playlist_name;
        }
        else{
            std::cout << "[WARNING] Invalid track index: " <<index;
            continue;
        }
    }
    std::cout <<"[INFO] Playlist loaded:" <<playlist_name <<counter <<"tracks)";
}

/**
 * TODO: Implement getTrackTitles method
 * @return Vector of track titles in the playlist
 */
std::vector<std::string> DJLibraryService::getTrackTitles() const {
    std::vector<std::string> titles;
    std::vector<AudioTrack*> playlist_arr= playlist.getTracks();
    for(AudioTrack* track : playlist_arr){
        titles.push_back(track->get_title());
    }
    return titles;
}
