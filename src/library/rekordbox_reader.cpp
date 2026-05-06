#ifdef _WIN32
#ifndef KS_STR_ENCODING_WIN32API
#define KS_STR_ENCODING_WIN32API
#endif
#endif

#include "library/rekordbox_reader.h"
#include "../../lib/rekordbox-metadata/rekordbox_pdb.h"
#include "../../lib/rekordbox-metadata/rekordbox_anlz.h"
#include "kaitai/kaitaistream.h"
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <new>
extern "C" {
#include "core/logger.h"
#include "core/memory_guard.h"
}

// Helper to safely get string from RB device string
static std::string RB_GetString(rekordbox_pdb_t::device_sql_string_t* rbs) {
    if (!rbs || !rbs->body()) return "";
    
    // The library handles different encodings. 
    // For RB PDB, it's usually UTF-16LE or ASCII.
    // Body can be device_sql_long_utf16le_t, device_sql_long_ascii_t, or device_sql_short_ascii_t.
    
    auto body = rbs->body();
    
    // We can try to cast to the specific types or rely on the fact that rekordbox_pdb.cpp
    // already calls bytes_to_str which (if KS_STR_ENCODING_WIN32API is defined)
    // converts everything to UTF-8.
    
    // However, the generated code for device_sql_string_t doesn't have a generic "text()" method.
    // It's in the body.
    
    std::string result = "";
    if (auto b = dynamic_cast<rekordbox_pdb_t::device_sql_long_utf16le_t*>(body)) result = b->text();
    else if (auto b = dynamic_cast<rekordbox_pdb_t::device_sql_long_ascii_t*>(body)) result = b->text();
    else if (auto b = dynamic_cast<rekordbox_pdb_t::device_sql_short_ascii_t*>(body)) result = b->text();
    
    // Cleanup: Remove non-printable chars (whitespace, etc.)
    while (!result.empty() && (unsigned char)result.back() <= 32) result.pop_back();
    
    return result;
}

extern "C" RBDatabase* RB_LoadDatabase(const char* rootPath) {
    auto startTime = std::chrono::steady_clock::now();
    std::string pdbPath = std::string(rootPath) + "/PIONEER/rekordbox/export.pdb";
    std::ifstream is(pdbPath, std::ios::binary);
    if (!is.is_open()) {
        UNX_LOG_ERR("[RB] Failed to open %s", pdbPath.c_str());
        return nullptr;
    }

    try {
        kaitai::kstream ks(&is);
        rekordbox_pdb_t pdb(&ks);

        std::map<uint32_t, std::string> artists;
        std::map<uint32_t, std::string> albums;
        std::map<uint32_t, std::string> genres;
        std::map<uint32_t, std::string> keys;
        std::map<uint32_t, std::string> artworks;
        std::map<uint32_t, std::string> labels;
        std::map<uint32_t, std::string> colors;

        std::vector<RBTrack> rbTracks;
        std::vector<RBPlaylist> rbPlaylists;
        std::vector<RBPlaylist> rbHistory;
        std::map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> playlistTracks; // entry_index, track_id
        std::map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> historyTracks;  // entry_index, track_id

        // PASS 1: Metadata Tables
        for (auto table : *pdb.tables()) {
            auto page_ref = table->first_page();
            while (page_ref) {
                auto page = page_ref->body();
                if (page->is_data_page() && page->type() == table->type()) {
                    for (auto group : *page->row_groups()) {
                        for (auto row : *group->rows()) {
                            if (!row->present()) continue;
                            auto body = row->body();
                            switch (table->type()) {
                                case rekordbox_pdb_t::PAGE_TYPE_ARTISTS: {
                                    auto r = static_cast<rekordbox_pdb_t::artist_row_t*>(body);
                                    artists[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_ALBUMS: {
                                    auto r = static_cast<rekordbox_pdb_t::album_row_t*>(body);
                                    albums[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_GENRES: {
                                    auto r = static_cast<rekordbox_pdb_t::genre_row_t*>(body);
                                    genres[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_KEYS: {
                                    auto r = static_cast<rekordbox_pdb_t::key_row_t*>(body);
                                    keys[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_ARTWORK: {
                                    auto r = static_cast<rekordbox_pdb_t::artwork_row_t*>(body);
                                    artworks[r->id()] = RB_GetString(r->path());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_LABELS: {
                                    auto r = static_cast<rekordbox_pdb_t::label_row_t*>(body);
                                    labels[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_COLORS: {
                                    auto r = static_cast<rekordbox_pdb_t::color_row_t*>(body);
                                    colors[r->id()] = RB_GetString(r->name());
                                    break;
                                }
                                default: break;
                            }
                        }
                    }
                }
                if (page_ref->index() == table->last_page()->index()) break;
                page_ref = page->next_page();
            }
        }
        
        UNX_LOG_INFO("[RB) Metadata Loaded: Artists:%zu, Albums:%zu, Genres:%zu, Keys:%zu, Artworks:%zu, Labels:%zu",
               artists.size(), albums.size(), genres.size(), keys.size(), artworks.size(), labels.size());

        // PASS 2: Tracks and Playlists
        for (auto table : *pdb.tables()) {
            auto page_ref = table->first_page();
            while (page_ref) {
                auto page = page_ref->body();
                if (page->is_data_page() && page->type() == table->type()) {
                    for (auto group : *page->row_groups()) {
                        for (auto row : *group->rows()) {
                            if (!row->present()) continue;
                            auto body = row->body();

                            switch (table->type()) {
                                case rekordbox_pdb_t::PAGE_TYPE_TRACKS: {
                                    auto r = static_cast<rekordbox_pdb_t::track_row_t*>(body);
                                    RBTrack t;
                                    memset(&t, 0, sizeof(RBTrack));
                                    t.ID = r->id();
                                    strncpy(t.Title, RB_GetString(r->title()).c_str(), 255);
                                    strncpy(t.FilePath, RB_GetString(r->file_path()).c_str(), 511);
                                    t.BPM = (float)r->tempo() / 100.0f;
                                    t.Duration = r->duration();
                                    
                                    if (artists.count(r->artist_id())) strncpy(t.Artist, artists[r->artist_id()].c_str(), 255);
                                    if (albums.count(r->album_id())) strncpy(t.Album, albums[r->album_id()].c_str(), 255);
                                    if (genres.count(r->genre_id())) strncpy(t.Genre, genres[r->genre_id()].c_str(), 255);
                                    if (keys.count(r->key_id())) strncpy(t.Key, keys[r->key_id()].c_str(), 31);
                                    if (artworks.count(r->artwork_id())) strncpy(t.ArtworkPath, artworks[r->artwork_id()].c_str(), 511);
                                    if (labels.count(r->label_id())) strncpy(t.Label, labels[r->label_id()].c_str(), 127);
                                    
                                    if (artists.count(r->remixer_id())) strncpy(t.Remixer, artists[r->remixer_id()].c_str(), 127);
                                    if (artists.count(r->composer_id())) strncpy(t.Composer, artists[r->composer_id()].c_str(), 127);
                                    
                                    strncpy(t.MixName, RB_GetString(r->mix_name()).c_str(), 127);
                                    strncpy(t.Comment, RB_GetString(r->comment()).c_str(), 255);
                                    strncpy(t.DateAdded, RB_GetString(r->date_added()).c_str(), 31);
                                    strncpy(t.ReleaseDate, RB_GetString(r->release_date()).c_str(), 31);
                                    strncpy(t.ISRC, RB_GetString(r->isrc()).c_str(), 31);
                                    
                                    t.Rating = r->rating();
                                    t.Year = r->year();
                                    t.Bitrate = r->bitrate();
                                    t.SampleRate = r->sample_rate();
                                    t.PlayCount = r->play_count();
                                    t.ColorID = r->color_id();
                                    t.TrackNumber = r->track_number();
                                    t.DiscNumber = r->disc_number();
                                    
                                    strncpy(t.AnalyzePath, RB_GetString(r->analyze_path()).c_str(), 511);

                                    rbTracks.push_back(t);
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_TREE: {
                                    auto r = static_cast<rekordbox_pdb_t::playlist_tree_row_t*>(body);
                                    RBPlaylist pl;
                                    memset(&pl, 0, sizeof(RBPlaylist));
                                    pl.ID = r->id();
                                    pl.ParentID = r->parent_id();
                                    pl.IsFolder = (r->raw_is_folder() != 0);
                                    strncpy(pl.Name, RB_GetString(r->name()).c_str(), 255);
                                    rbPlaylists.push_back(pl);
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_ENTRIES: {
                                    auto r = static_cast<rekordbox_pdb_t::playlist_entry_row_t*>(body);
                                    playlistTracks[r->playlist_id()].push_back({r->entry_index(), r->track_id()});
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_HISTORY_PLAYLISTS: {
                                    auto r = static_cast<rekordbox_pdb_t::history_playlist_row_t*>(body);
                                    RBPlaylist pl;
                                    memset(&pl, 0, sizeof(RBPlaylist));
                                    pl.ID = r->id();
                                    pl.IsFolder = false;
                                    strncpy(pl.Name, RB_GetString(r->name()).c_str(), 255);
                                    rbHistory.push_back(pl);
                                    break;
                                }
                                case rekordbox_pdb_t::PAGE_TYPE_HISTORY_ENTRIES: {
                                    auto r = static_cast<rekordbox_pdb_t::history_entry_row_t*>(body);
                                    historyTracks[r->playlist_id()].push_back({r->entry_index(), r->track_id()});
                                    break;
                                }
                                default: break;
                            }
                        }
                    }
                }
                if (page_ref->index() == table->last_page()->index()) break;
                page_ref = page->next_page();
            }
        }

        RBDatabase* db = (RBDatabase*)malloc(sizeof(RBDatabase));
        memset(db, 0, sizeof(RBDatabase));
        db->TrackCount = (uint32_t)rbTracks.size();
        db->Tracks = (RBTrack*)malloc(sizeof(RBTrack) * db->TrackCount);
        memset(db->Tracks, 0, sizeof(RBTrack) * db->TrackCount);
        for (size_t i = 0; i < rbTracks.size(); i++) db->Tracks[i] = rbTracks[i];
        
        db->PlaylistCount = (uint32_t)rbPlaylists.size();
        db->Playlists = (RBPlaylist*)malloc(sizeof(RBPlaylist) * db->PlaylistCount);
        memset(db->Playlists, 0, sizeof(RBPlaylist) * db->PlaylistCount);
        for (size_t i = 0; i < rbPlaylists.size(); i++) {
            db->Playlists[i] = rbPlaylists[i];
            auto& tids = playlistTracks[db->Playlists[i].ID];
            db->Playlists[i].TrackCount = (uint32_t)tids.size();
            if (db->Playlists[i].TrackCount > 0) {
                // Sort by entry_index
                std::sort(tids.begin(), tids.end(), [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
                    return a.first < b.first;
                });
                db->Playlists[i].TrackIDs = (uint32_t*)malloc(sizeof(uint32_t) * db->Playlists[i].TrackCount);
                for (size_t j = 0; j < tids.size(); j++) db->Playlists[i].TrackIDs[j] = tids[j].second;
            } else {
                db->Playlists[i].TrackIDs = nullptr;
            }
        }

        db->HistoryCount = (uint32_t)rbHistory.size();
        db->History = (RBPlaylist*)malloc(sizeof(RBPlaylist) * db->HistoryCount);
        memset(db->History, 0, sizeof(RBPlaylist) * db->HistoryCount);
        for (size_t i = 0; i < rbHistory.size(); i++) {
            db->History[i] = rbHistory[i];
            auto& tids = historyTracks[db->History[i].ID];
            db->History[i].TrackCount = (uint32_t)tids.size();
            if (db->History[i].TrackCount > 0) {
                std::sort(tids.begin(), tids.end(), [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
                    return a.first < b.first;
                });
                db->History[i].TrackIDs = (uint32_t*)malloc(sizeof(uint32_t) * db->History[i].TrackCount);
                for (size_t j = 0; j < tids.size(); j++) db->History[i].TrackIDs[j] = tids[j].second;
            } else {
                db->History[i].TrackIDs = nullptr;
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        double duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        UNX_LOG_INFO("[PERF] [DB] Database Loaded: %u tracks in %.2f ms", db->TrackCount, duration);

        return db;

    } catch (const std::exception& e) {
        UNX_LOG_ERR("[RB] Error parsing database: %s", e.what());
        return nullptr;
    }
}

extern "C" void RB_FreeDatabase(RBDatabase* db) {
    if (!db) return;
    if (db->Tracks) {
        for (uint32_t i = 0; i < db->TrackCount; i++) {
            if (db->Tracks[i].Analysis.Cues) free(db->Tracks[i].Analysis.Cues);
            if (db->Tracks[i].Analysis.Phrases) free(db->Tracks[i].Analysis.Phrases);
            if (db->Tracks[i].Analysis.DynamicWaveform) free(db->Tracks[i].Analysis.DynamicWaveform);
            if (db->Tracks[i].Analysis.BeatGrid) free(db->Tracks[i].Analysis.BeatGrid);
        }
        free(db->Tracks);
    }
    if (db->Playlists) {
        for (uint32_t i = 0; i < db->PlaylistCount; i++) {
            if (db->Playlists[i].TrackIDs) free(db->Playlists[i].TrackIDs);
        }
        free(db->Playlists);
    }
    if (db->History) {
        for (uint32_t i = 0; i < db->HistoryCount; i++) {
            if (db->History[i].TrackIDs) free(db->History[i].TrackIDs);
        }
        free(db->History);
    }
    free(db);
}

static void RB_ParseAnlz(const std::string& path, RBAnalysis* analysis) {
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) return;

    try {
        kaitai::kstream ks(&is);
        rekordbox_anlz_t anlz(&ks);
        int currentStaticPrio = 0;

        for (auto& section : *anlz.sections()) {
            auto tag = section->fourcc();
            if (tag == rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID) {
                auto bg = static_cast<rekordbox_anlz_t::beat_grid_tag_t*>(section->body());
                analysis->BeatGridCount = bg->num_beats();

                if (analysis->BeatGrid) free(analysis->BeatGrid);
                analysis->BeatGrid = (RBBeat*)malloc(analysis->BeatGridCount * sizeof(RBBeat));
                if (!analysis->BeatGrid) {
                    UNX_LOG_ERR("[ANLZ] OOM: Failed to allocate BeatGrid (%d entries)", analysis->BeatGridCount);
                    analysis->BeatGridCount = 0;
                    return; // Abort further parsing if critical OOM
                }
                UNX_LOG_INFO("[ANLZ] Allocated BeatGrid: %d entries (%.2f KB)", analysis->BeatGridCount, (analysis->BeatGridCount * sizeof(RBBeat)) / 1024.0f);
                for (uint32_t i = 0; i < bg->num_beats(); i++) {
                    auto b = (*bg->beats())[i].get();
                    analysis->BeatGrid[i].Time = b->time();
                    analysis->BeatGrid[i].BPM = b->tempo();
                    analysis->BeatGrid[i].BeatNumber = b->beat_number();
                }
            } else if (tag == rekordbox_anlz_t::SECTION_TAGS_CUES || tag == rekordbox_anlz_t::SECTION_TAGS_CUES_2) {
                std::vector<RBCue> found;
                if (tag == rekordbox_anlz_t::SECTION_TAGS_CUES) {
                    auto ct = static_cast<rekordbox_anlz_t::cue_tag_t*>(section->body());
                    bool isMemorySection = (ct->type() == 0);
                    for (auto& entry : *ct->cues()) {
                        RBCue rc;
                        memset(&rc, 0, sizeof(RBCue));
                        rc.Time = entry->time();
                        rc.ID = isMemorySection ? 0 : (uint16_t)entry->hot_cue();
                        rc.Type = (uint16_t)entry->type();
                        rc.Status = (uint16_t)entry->status();
                        rc.LoopTime = entry->loop_time();
                        found.push_back(rc);
                    }
                } else {
                    auto ct = static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(section->body());
                    bool isMemorySection = (ct->type() == 0);
                    for (auto& entry : *ct->cues()) {
                        RBCue rc;
                        memset(&rc, 0, sizeof(RBCue));
                        rc.Time = entry->time();
                        rc.ID = isMemorySection ? 0 : (uint16_t)entry->hot_cue();
                        rc.Type = (uint16_t)entry->type();
                        rc.LoopTime = entry->loop_time();
                        std::string comment = entry->comment();
                        strncpy(rc.Comment, comment.c_str(), 63);
                        
                        if (!entry->_is_null_color_red()) {
                            rc.Color[0] = entry->color_red();
                            rc.Color[1] = entry->color_green();
                            rc.Color[2] = entry->color_blue();
                        }
                        
                        found.push_back(rc);
                    }
                }

                for (auto& f : found) {
                    bool exists = false;
                    for (uint32_t i=0; i<analysis->CueCount; i++) {
                        if (analysis->Cues[i].Time == f.Time && analysis->Cues[i].ID == f.ID) {
                            exists = true; 
                            break;
                        }
                    }
                    if (!exists) {
                        RBCue* next = (RBCue*)malloc(sizeof(RBCue) * (analysis->CueCount + 1));
                        if (!next) {
                            UNX_LOG_ERR("[ANLZ] OOM: Failed to allocate Cues (%d)", analysis->CueCount + 1);
                            break;
                        }
                        if (analysis->Cues) {
                            memcpy(next, analysis->Cues, sizeof(RBCue) * analysis->CueCount);
                            free(analysis->Cues);
                        }
                        next[analysis->CueCount] = f;
                        analysis->Cues = next;
                        analysis->CueCount++;
                    }
                }
                UNX_LOG_INFO("[ANLZ] Allocated Cues: %d entries (%.2f KB)", analysis->CueCount, (analysis->CueCount * sizeof(RBCue)) / 1024.0f);
            } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_TINY || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_PREVIEW || 
                       tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_PREVIEW || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_PREVIEW) {
                std::string data;
                int prio = 0; // 1=blue, 2=color, 3=3band
                if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_PREVIEW) {
                    auto w3 = static_cast<rekordbox_anlz_t::wave_3band_preview_tag_t*>(section->body());
                    data = w3->entries();
                    prio = 3;
                } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_PREVIEW) {
                    auto wc = static_cast<rekordbox_anlz_t::wave_color_preview_tag_t*>(section->body());
                    data = wc->entries();
                    prio = 2;
                } else {
                    auto wp = static_cast<rekordbox_anlz_t::wave_preview_tag_t*>(section->body());
                    data = wp->data();
                    prio = 1;
                }

                if (prio > currentStaticPrio) {
                    analysis->StaticWaveformLen = data.length() > 8192 ? 8192 : data.length();
                    analysis->StaticWaveformType = prio;
                    memcpy(analysis->StaticWaveform, data.data(), analysis->StaticWaveformLen);
                    currentStaticPrio = prio;
                }
            } else if ((tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_SCROLL)) {
                // ECO MODE: Skip dynamic waveform if memory is very low
                if (MemoryGuard_GetLevel() >= MEM_MODE_LITE) {
                    static bool warned = false;
                    if (!warned) {
                        UNX_LOG_WARN("[ANLZ] LITE MODE: Skipping dynamic waveform parsing to save RAM.");
                        warned = true;
                    }
                    continue;
                }
                
                std::string data;
                int type = 0;
                if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL) {
                    auto ws = static_cast<rekordbox_anlz_t::wave_scroll_tag_t*>(section->body());
                    data = ws->entries();
                    type = 1;
                } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL) {
                    auto wc = static_cast<rekordbox_anlz_t::wave_color_scroll_tag_t*>(section->body());
                    data = wc->entries();
                    type = 2;
                } else {
                    auto w3 = static_cast<rekordbox_anlz_t::wave_3band_scroll_tag_t*>(section->body());
                    data = w3->entries();
                    type = 3;
                }
                
                if (analysis->DynamicWaveform) free(analysis->DynamicWaveform);
                analysis->DynamicWaveformLen = data.length();
                analysis->WaveformType = type;
                if (analysis->DynamicWaveformLen > 0) {
                    analysis->DynamicWaveform = (unsigned char*)malloc(analysis->DynamicWaveformLen);
                    if (!analysis->DynamicWaveform) {
                        UNX_LOG_ERR("[ANLZ] OOM: Failed to allocate DynamicWaveform (%d bytes)", (int)analysis->DynamicWaveformLen);
                        analysis->DynamicWaveformLen = 0;
                    } else {
                        memcpy(analysis->DynamicWaveform, data.data(), analysis->DynamicWaveformLen);
                        UNX_LOG_INFO("[ANLZ] Allocated DynamicWaveform: %d bytes (%.2f MB)", (int)analysis->DynamicWaveformLen, analysis->DynamicWaveformLen / (1024.0f * 1024.0f));
                    }
                }
            } else if (tag == rekordbox_anlz_t::SECTION_TAGS_SONG_STRUCTURE) {
                auto ss = static_cast<rekordbox_anlz_t::song_structure_tag_t*>(section->body());
                if (ss && ss->body() && ss->body()->entries()) {
                    auto entries = ss->body()->entries();
                    if (!analysis->Phrases) {
                        analysis->PhraseCount = entries->size();
                        analysis->Phrases = (RBPhrase*)malloc(analysis->PhraseCount * sizeof(RBPhrase));
                        if (!analysis->Phrases) {
                            UNX_LOG_ERR("[ANLZ] OOM: Failed to allocate Phrases (%d)", analysis->PhraseCount);
                            analysis->PhraseCount = 0;
                        } else {
                            UNX_LOG_INFO("[ANLZ] Allocated Phrases: %d entries (%.2f KB)", analysis->PhraseCount, (analysis->PhraseCount * sizeof(RBPhrase)) / 1024.0f);
                            for (size_t i = 0; i < analysis->PhraseCount; i++) {
                            auto entry = (*entries)[i].get();
                            analysis->Phrases[i].Index = entry->index();
                            analysis->Phrases[i].Beat = entry->beat();
                            analysis->Phrases[i].KindID = 0;
                            strcpy(analysis->Phrases[i].Kind, "Unknown");
                            
                            auto kind = entry->kind();
                            if (auto kh = dynamic_cast<rekordbox_anlz_t::phrase_high_t*>(kind)) {
                                analysis->Phrases[i].KindID = kh->id();
                                switch (kh->id()) {
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_INTRO: strcpy(analysis->Phrases[i].Kind, "Intro"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_UP: strcpy(analysis->Phrases[i].Kind, "Up"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_DOWN: strcpy(analysis->Phrases[i].Kind, "Down"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_CHORUS: strcpy(analysis->Phrases[i].Kind, "Chorus"); break;
                                    case rekordbox_anlz_t::MOOD_HIGH_PHRASE_OUTRO: strcpy(analysis->Phrases[i].Kind, "Outro"); break;
                                }
                            } else if (auto km = dynamic_cast<rekordbox_anlz_t::phrase_mid_t*>(kind)) {
                                analysis->Phrases[i].KindID = km->id();
                                switch (km->id()) {
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_INTRO: strcpy(analysis->Phrases[i].Kind, "Intro"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_1: strcpy(analysis->Phrases[i].Kind, "Verse 1"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_2: strcpy(analysis->Phrases[i].Kind, "Verse 2"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_3: strcpy(analysis->Phrases[i].Kind, "Verse 3"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_4: strcpy(analysis->Phrases[i].Kind, "Verse 4"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_5: strcpy(analysis->Phrases[i].Kind, "Verse 5"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_VERSE_6: strcpy(analysis->Phrases[i].Kind, "Verse 6"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_BRIDGE: strcpy(analysis->Phrases[i].Kind, "Bridge"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_CHORUS: strcpy(analysis->Phrases[i].Kind, "Chorus"); break;
                                    case rekordbox_anlz_t::MOOD_MID_PHRASE_OUTRO: strcpy(analysis->Phrases[i].Kind, "Outro"); break;
                                }
                            } else if (auto kl = dynamic_cast<rekordbox_anlz_t::phrase_low_t*>(kind)) {
                                analysis->Phrases[i].KindID = kl->id();
                                switch(kl->id()) {
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_INTRO: strcpy(analysis->Phrases[i].Kind, "Intro"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_1: strcpy(analysis->Phrases[i].Kind, "Verse 1"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_1B: strcpy(analysis->Phrases[i].Kind, "Verse 1B"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_1C: strcpy(analysis->Phrases[i].Kind, "Verse 1C"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_2: strcpy(analysis->Phrases[i].Kind, "Verse 2"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_2B: strcpy(analysis->Phrases[i].Kind, "Verse 2B"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_VERSE_2C: strcpy(analysis->Phrases[i].Kind, "Verse 2C"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_BRIDGE: strcpy(analysis->Phrases[i].Kind, "Bridge"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_CHORUS: strcpy(analysis->Phrases[i].Kind, "Chorus"); break;
                                    case rekordbox_anlz_t::MOOD_LOW_PHRASE_OUTRO: strcpy(analysis->Phrases[i].Kind, "Outro"); break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    } catch (...) {}
}

extern "C" void RB_LoadTrackData(RBAnalysis* analysis, const char* analyzePath, const char* title, uint32_t id, const char* rootPath) {
    if (!analysis || !analyzePath || analyzePath[0] == '\0') return;

    UNX_LOG_INFO("[RB] Loading Track Data: %s (ID: %u)", title, id);

    std::string datPath = std::string(rootPath) + "/" + analyzePath;
    
    FILE* f = fopen(datPath.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        UNX_LOG_INFO("[RB] Analysis File Size: %.2f KB", size / 1024.0f);
    }

    RB_ParseAnlz(datPath, analysis);

    // Also look for .EXT
    std::string extPath = datPath;
    if (extPath.size() > 4) {
        extPath.replace(extPath.size() - 3, 3, "EXT");
        RB_ParseAnlz(extPath, analysis);
    }
}

extern "C" void RB_ReloadWaveform(const char* path, unsigned char** outData, int* outLen, int* outType) {
    if (!path || !outData || !outLen || !outType) return;
    
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) return;

    try {
        kaitai::kstream ks(&is);
        rekordbox_anlz_t anlz(&ks);

        for (auto& section : *anlz.sections()) {
            auto tag = section->fourcc();
            if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL || tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_SCROLL) {
                std::string data;
                int type = 0;
                if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL) {
                    auto ws = static_cast<rekordbox_anlz_t::wave_scroll_tag_t*>(section->body());
                    data = ws->entries();
                    type = 1;
                } else if (tag == rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL) {
                    auto wc = static_cast<rekordbox_anlz_t::wave_color_scroll_tag_t*>(section->body());
                    data = wc->entries();
                    type = 2;
                } else {
                    auto w3 = static_cast<rekordbox_anlz_t::wave_3band_scroll_tag_t*>(section->body());
                    data = w3->entries();
                    type = 3;
                }
                
                *outLen = data.length();
                *outType = type;
                if (*outLen > 0) {
                    *outData = (unsigned char*)malloc(*outLen);
                    if (*outData) {
                        memcpy(*outData, data.data(), *outLen);
                    } else {
                        *outLen = 0;
                    }
                }
                return; // Found it
            }
        }
    } catch (...) {}
}
