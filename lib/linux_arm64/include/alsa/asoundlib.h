#ifndef ALSA_ASOUNDLIB_H
#define ALSA_ASOUNDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

typedef struct _snd_seq snd_seq_t;

#define SND_SEQ_OPEN_INPUT 1
#define SND_SEQ_OPEN_OUTPUT 2
#define SND_SEQ_OPEN_DUPLEX 3
#define SND_SEQ_OPEN_READ 1

#define SND_SEQ_PORT_CAP_READ        (1<<0)
#define SND_SEQ_PORT_CAP_WRITE       (1<<1)
#define SND_SEQ_PORT_CAP_SUBS_READ   (1<<5)
#define SND_SEQ_PORT_CAP_SUBS_WRITE  (1<<6)

#define SND_SEQ_PORT_TYPE_SPECIFIC   (1<<0)
#define SND_SEQ_PORT_TYPE_MIDI_GENERIC (1<<1)
#define SND_SEQ_PORT_TYPE_APPLICATION (1<<20)

#define SND_SEQ_CLIENT_SYSTEM 0

#define SND_SEQ_EVENT_NOTEON 6
#define SND_SEQ_EVENT_NOTEOFF 7
#define SND_SEQ_EVENT_CONTROLLER 10
#define SND_SEQ_EVENT_PITCHBEND 13

typedef struct snd_seq_addr {
    uint8_t client;
    uint8_t port;
} snd_seq_addr_t;

typedef struct snd_seq_ev_note {
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t off_velocity;
    uint32_t duration;
} snd_seq_ev_note_t;

typedef struct snd_seq_ev_ctrl {
    uint8_t channel;
    uint8_t unused[3];
    uint32_t param;
    int32_t value;
} snd_seq_ev_ctrl_t;

typedef struct snd_seq_ev_ext {
    uint32_t len;
    void *ptr;
} snd_seq_ev_ext_t;

typedef struct snd_seq_event {
    uint8_t type;
    uint8_t flags;
    uint8_t tag;
    uint8_t queue;
    struct {
        uint32_t tv_sec;
        uint32_t tv_nsec;
    } time;
    snd_seq_addr_t source;
    snd_seq_addr_t dest;
    union {
        snd_seq_ev_note_t note;
        snd_seq_ev_ctrl_t control;
        snd_seq_ev_ext_t ext;
    } data;
} snd_seq_event_t;

typedef struct _snd_seq_client_info snd_seq_client_info_t;
typedef struct _snd_seq_port_info snd_seq_port_info_t;

int snd_seq_open(snd_seq_t **handle, const char *name, int streams, int mode);
int snd_seq_close(snd_seq_t *handle);
int snd_seq_client_id(snd_seq_t *handle);
int snd_seq_set_client_name(snd_seq_t *handle, const char *name);
int snd_seq_create_simple_port(snd_seq_t *seq, const char *name, unsigned int caps, unsigned int type);
int snd_seq_connect_from(snd_seq_t *seq, int my_port, int src_client, int src_port);
int snd_seq_connect_to(snd_seq_t *seq, int my_port, int dest_client, int dest_port);

size_t snd_seq_client_info_sizeof(void);
size_t snd_seq_port_info_sizeof(void);

#define snd_seq_client_info_alloca(ptr) \
    do { *ptr = (snd_seq_client_info_t *)alloca(snd_seq_client_info_sizeof()); memset(*ptr, 0, snd_seq_client_info_sizeof()); } while (0)

#define snd_seq_port_info_alloca(ptr) \
    do { *ptr = (snd_seq_port_info_t *)alloca(snd_seq_port_info_sizeof()); memset(*ptr, 0, snd_seq_port_info_sizeof()); } while (0)

int snd_seq_query_next_client(snd_seq_t *seq, snd_seq_client_info_t *info);
int snd_seq_query_next_port(snd_seq_t *seq, snd_seq_port_info_t *info);

int snd_seq_client_info_get_client(const snd_seq_client_info_t *info);
const char *snd_seq_client_info_get_name(const snd_seq_client_info_t *info);
void snd_seq_client_info_set_client(snd_seq_client_info_t *info, int client);

int snd_seq_port_info_get_port(const snd_seq_port_info_t *info);
unsigned int snd_seq_port_info_get_capability(const snd_seq_port_info_t *info);
void snd_seq_port_info_set_client(snd_seq_port_info_t *info, int client);
void snd_seq_port_info_set_port(snd_seq_port_info_t *info, int port);

int snd_seq_event_input_pending(snd_seq_t *seq, int fetch_sequencer);
int snd_seq_event_input(snd_seq_t *seq, snd_seq_event_t **ev);
int snd_seq_event_output(snd_seq_t *seq, snd_seq_event_t *ev);
int snd_seq_drain_output(snd_seq_t *seq);
int snd_seq_free_event(snd_seq_event_t *ev);

#define snd_seq_ev_clear(ev) memset((ev), 0, sizeof(snd_seq_event_t))
#define snd_seq_ev_set_source(ev, p) ((ev)->source.port = (p))
#define snd_seq_ev_set_subs(ev) ((ev)->dest.client = 255, (ev)->dest.port = 255)
#define snd_seq_ev_set_direct(ev) ((ev)->queue = 253)

static inline void snd_seq_ev_set_noteon(snd_seq_event_t *ev, uint8_t ch, uint8_t note, uint8_t vel) {
    ev->type = SND_SEQ_EVENT_NOTEON;
    ev->data.note.channel = ch;
    ev->data.note.note = note;
    ev->data.note.velocity = vel;
}

static inline void snd_seq_ev_set_noteoff(snd_seq_event_t *ev, uint8_t ch, uint8_t note, uint8_t vel) {
    ev->type = SND_SEQ_EVENT_NOTEOFF;
    ev->data.note.channel = ch;
    ev->data.note.note = note;
    ev->data.note.velocity = vel;
}

static inline void snd_seq_ev_set_controller(snd_seq_event_t *ev, uint8_t ch, uint8_t param, int32_t val) {
    ev->type = SND_SEQ_EVENT_CONTROLLER;
    ev->data.control.channel = ch;
    ev->data.control.param = param;
    ev->data.control.value = val;
}

static inline void snd_seq_ev_set_sysex(snd_seq_event_t *ev, uint32_t len, void *ptr) {
    ev->type = 130; // SND_SEQ_EVENT_SYSEX
    ev->data.ext.len = len;
    ev->data.ext.ptr = ptr;
}

#ifdef __cplusplus
}
#endif

#endif // ALSA_ASOUNDLIB_H
