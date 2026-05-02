#include "serato_reader.h"
#include "../../lib/serato/serato_parser.h"
#include "../../lib/serato/serato_waveform.h"
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <map>
#include "core/logger.h"

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
        strncpy(t.Label, tracks[i].label.c_str(), 255);
        strncpy(t.Key, tracks[i].key.c_str(), 31);
        t.BPM = (float)tracks[i].bpm;
        t.Duration = (uint32_t)tracks[i].duration;
        strncpy(t.FilePath, tracks[i].location.c_str(), 511);
        strncpy(t.Comment, tracks[i].comment.c_str(), 255);
        strncpy(t.Grouping, tracks[i].grouping.c_str(), 255);
        strncpy(t.FileType, tracks[i].filetype.c_str(), 31);
        t.Year = tracks[i].year;
        t.Bitrate = atoi(tracks[i].bitrate.c_str());
        t.SampleRate = atoi(tracks[i].samplerate.c_str());
        t.FileTime = tracks[i].filetime;
        t.DateAdded = tracks[i].datetimeadded;

        trackPathToId[tracks[i].location] = t.ID;
    }

    // Load Crates with Hierarchical support
    std::string crateDir = std::string(rootPath) + "/_Serato_/Subcrates";
    std::vector<SeratoPlaylist> playlists;
    std::map<std::string, uint32_t> folderPathToId;
    uint32_t nextId = 0;

    if (fs::exists(crateDir)) {
        for (const auto& entry : fs::directory_iterator(crateDir)) {
            if (entry.path().extension() == ".crate") {
                std::string filename = entry.path().stem().string();
                
                // Parse Hierarchy (Folder%%Subfolder%%Crate)
                std::vector<std::string> parts;
                size_t start = 0;
                size_t end = filename.find("%%");
                while (end != std::string::npos) {
                    parts.push_back(filename.substr(start, end - start));
                    start = end + 2;
                    end = filename.find("%%", start);
                }
                parts.push_back(filename.substr(start));

                uint32_t parentId = 0;
                for (size_t i = 0; i < parts.size(); ++i) {
                    std::string currentPath;
                    for (size_t j = 0; j <= i; ++j) {
                        currentPath += (j > 0 ? "%%" : "") + parts[j];
                    }

                    if (i < parts.size() - 1) {
                        // It's a folder
                        if (folderPathToId.find(currentPath) == folderPathToId.end()) {
                            SeratoPlaylist folderPl;
                            memset(&folderPl, 0, sizeof(SeratoPlaylist));
                            folderPl.ID = ++nextId;
                            folderPl.ParentID = parentId;
                            folderPl.IsFolder = true;
                            strncpy(folderPl.Name, parts[i].c_str(), 255);
                            playlists.push_back(folderPl);
                            folderPathToId[currentPath] = folderPl.ID;
                        }
                        parentId = folderPathToId[currentPath];
                    } else {
                        // It's the actual crate
                        serato::Crate crate = serato::Parser::parseCrate(entry.path().string());
                        SeratoPlaylist pl;
                        memset(&pl, 0, sizeof(SeratoPlaylist));
                        pl.ID = ++nextId;
                        pl.ParentID = parentId;
                        pl.IsFolder = false;
                        strncpy(pl.Name, parts[i].c_str(), 255);
                        
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
        }
    }

    // Load History Sessions
    std::string historyDir = std::string(rootPath) + "/_Serato_/History/Sessions";
    if (fs::exists(historyDir)) {
        // Create a "History" root folder
        SeratoPlaylist histFolder;
        memset(&histFolder, 0, sizeof(SeratoPlaylist));
        histFolder.ID = ++nextId;
        histFolder.ParentID = 0;
        histFolder.IsFolder = true;
        strcpy(histFolder.Name, "History");
        playlists.push_back(histFolder);
        uint32_t histRootId = histFolder.ID;

        for (const auto& entry : fs::directory_iterator(historyDir)) {
            if (entry.path().extension() == ".session") {
                // History sessions are basically database files
                std::vector<serato::Track> histTracks = serato::Parser::parseDatabase(entry.path().string());
                if (!histTracks.empty()) {
                    SeratoPlaylist pl;
                    memset(&pl, 0, sizeof(SeratoPlaylist));
                    pl.ID = ++nextId;
                    pl.ParentID = histRootId;
                    pl.IsFolder = false;
                    strncpy(pl.Name, entry.path().stem().string().c_str(), 255);
                    
                    std::vector<uint32_t> tids;
                    for (const auto& t : histTracks) {
                        if (trackPathToId.count(t.location)) {
                            tids.push_back(trackPathToId[t.location]);
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

#include "../../lib/serato/serato_tags.h"

extern "C" void Serato_LoadTrackData(SeratoTrack* track, const char* rootPath) {
    if (!track) return;

    UNX_LOG_INFO("[Serato] Loading Track Data: %s (ID: %u)", track->Title, track->ID);

    std::string fullPath;
    if (track->FilePath[0] == '/' || track->FilePath[0] == '\\') {
        fullPath = std::string(rootPath) + track->FilePath;
    } else {
        fullPath = std::string(rootPath) + "/" + track->FilePath;
    }

    auto tags = serato::TagParser::readTags(fullPath);
    
    serato::WaveformParser parser;
    serato::SeratoAnalysis analysis;

    // Load Markers (Cues)
    std::vector<serato::Marker> allMarkers;
    
    if (tags.count("Markers") && parser.parseBase64(tags["Markers"], analysis)) {
        allMarkers.insert(allMarkers.end(), analysis.markers.begin(), analysis.markers.end());
    }
    if (tags.count("Markers2") && parser.parseBase64(tags["Markers2"], analysis)) {
        // Markers2 often has more detail/colors
        for (auto& m2 : analysis.markers) {
            bool found = false;
            for (auto& m1 : allMarkers) {
                if (std::abs((int)m1.time - (int)m2.time) < 5) {
                    m1 = m2; // Favor Markers2
                    found = true;
                    break;
                }
            }
            if (!found) allMarkers.push_back(m2);
        }
    }

    if (!allMarkers.empty()) {
        track->CueCount = (uint32_t)allMarkers.size();
        track->Cues = new RBCue[track->CueCount];
        for (size_t i = 0; i < allMarkers.size(); i++) {
            track->Cues[i].Time = allMarkers[i].time;
            track->Cues[i].ID = (uint16_t)(i + 1); // 1-8
            track->Cues[i].Type = (uint16_t)(allMarkers[i].type == 0 ? 1 : 2); // 1=Mem, 2=Loop
            track->Cues[i].LoopTime = allMarkers[i].endTime;
            track->Cues[i].Status = (allMarkers[i].type == 1 ? 4 : 1); // 4=ActiveLoop for loops
            
            // Extract color
            uint32_t c = allMarkers[i].color;
            track->Cues[i].Color[0] = (c >> 16) & 0xFF;
            track->Cues[i].Color[1] = (c >> 8) & 0xFF;
            track->Cues[i].Color[2] = c & 0xFF;
            strncpy(track->Cues[i].Comment, allMarkers[i].name.c_str(), 63);
        }
    }
}

