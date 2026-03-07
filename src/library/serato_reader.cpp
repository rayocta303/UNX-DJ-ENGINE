#include "serato_reader.h"
#include "../../lib/serato/serato_parser.h"
#include "../../lib/serato/serato_waveform.h"
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

extern "C" SeratoDatabase* Serato_LoadDatabase(const char* rootPath) {
    std::string dbPath = std::string(rootPath) + "/_Serato_/database V2";
    if (!fs::exists(dbPath)) {
        std::cerr << "[Serato] Database not found at " << dbPath << std::endl;
        return nullptr;
    }

    std::vector<serato::Track> tracks = serato::Parser::parseDatabase(dbPath);
    
    SeratoDatabase* db = new SeratoDatabase();
    db->TrackCount = (uint32_t)tracks.size();
    db->Tracks = new SeratoTrack[db->TrackCount];
    
    std::map<std::string, uint32_t> trackPathToId;

    for (size_t i = 0; i < tracks.size(); i++) {
        SeratoTrack& t = db->Tracks[i];
        memset(&t, 0, sizeof(SeratoTrack));
        t.ID = (uint32_t)i;
        
        strncpy(t.Title, tracks[i].title.c_str(), 255);
        strncpy(t.Artist, tracks[i].artist.c_str(), 255);
        strncpy(t.Album, tracks[i].album.c_str(), 255);
        strncpy(t.Genre, tracks[i].genre.c_str(), 255);
        strncpy(t.Key, tracks[i].key.c_str(), 31);
        t.BPM = (float)tracks[i].bpm;
        t.Duration = (uint32_t)tracks[i].duration;
        strncpy(t.FilePath, tracks[i].location.c_str(), 511);
        strncpy(t.Comment, tracks[i].comment.c_str(), 255);

        trackPathToId[tracks[i].location] = t.ID;
    }

    // Load Crates
    std::string crateDir = std::string(rootPath) + "/_Serato_/Subcrates";
    std::vector<SeratoPlaylist> playlists;
    if (fs::exists(crateDir)) {
        for (const auto& entry : fs::directory_iterator(crateDir)) {
            if (entry.path().extension() == ".crate") {
                serato::Crate crate = serato::Parser::parseCrate(entry.path().string());
                SeratoPlaylist pl;
                memset(&pl, 0, sizeof(SeratoPlaylist));
                pl.ID = (uint32_t)playlists.size();
                strncpy(pl.Name, crate.name.c_str(), 255);
                
                std::vector<uint32_t> tids;
                for (const auto& path : crate.trackPaths) {
                    if (trackPathToId.count(path)) {
                        tids.push_back(trackPathToId[path]);
                    }
                }
                
                pl.TrackCount = (uint32_t)tids.size();
                if (pl.TrackCount > 0) {
                    pl.TrackIDs = new uint32_t[pl.TrackCount];
                    for (size_t j = 0; j < tids.size(); j++) pl.TrackIDs[j] = tids[j];
                }
                playlists.push_back(pl);
            }
        }
    }

    db->PlaylistCount = (uint32_t)playlists.size();
    db->Playlists = new SeratoPlaylist[db->PlaylistCount];
    for (size_t i = 0; i < playlists.size(); i++) db->Playlists[i] = playlists[i];

    return db;
}

extern "C" void Serato_FreeDatabase(SeratoDatabase* db) {
    if (!db) return;
    if (db->Tracks) delete[] db->Tracks;
    if (db->Playlists) {
        for (uint32_t i = 0; i < db->PlaylistCount; i++) {
            if (db->Playlists[i].TrackIDs) delete[] db->Playlists[i].TrackIDs;
        }
        delete[] db->Playlists;
    }
    delete db;
}

extern "C" SeratoWaveform* Serato_LoadWaveformFromBase64(const char* base64Data) {
    if (!base64Data) return nullptr;
    
    serato::WaveformParser parser;
    serato::SeratoAnalysis analysis;
    if (!parser.parseBase64(base64Data, analysis)) {
        return nullptr;
    }
    
    SeratoWaveform* wf = new SeratoWaveform();
    
    // Use overview (onvg) if available
    if (!analysis.overview.empty()) {
        wf->SampleCount = (uint32_t)analysis.overview.size();
        wf->Samples = new SeratoWaveformSample[wf->SampleCount];
        
        for (uint32_t i = 0; i < wf->SampleCount; i++) {
            uint8_t val = analysis.overview[i];
            // Simple grayscale mapping for now
            wf->Samples[i].R = val;
            wf->Samples[i].G = val;
            wf->Samples[i].B = val;
            wf->Samples[i].A = 255;
        }
    } else {
        delete wf;
        return nullptr;
    }
    
    return wf;
}

extern "C" void Serato_FreeWaveform(SeratoWaveform* wf) {
    if (!wf) return;
    if (wf->Samples) delete[] wf->Samples;
    delete wf;
}
