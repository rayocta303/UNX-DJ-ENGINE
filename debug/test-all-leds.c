#include <alsa/asoundlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

typedef struct _snd_midi_event snd_midi_event_t;
int snd_midi_event_new(size_t bufsize, snd_midi_event_t **rdev);
void snd_midi_event_free(snd_midi_event_t *dev);
void snd_midi_event_init(snd_midi_event_t *dev);
long snd_midi_event_encode(snd_midi_event_t *dev, const unsigned char *buf,
                           long count, snd_seq_event_t *ev);

static void send_bytes(snd_seq_t *seq, int port, snd_midi_event_t *coder, const unsigned char *b, int len) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_midi_event_init(coder);
    snd_midi_event_encode(coder, b, len, &ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_event_output(seq, &ev);
    snd_seq_drain_output(seq);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("===========================================\n");
    printf("  Pioneer DDJ-FLX6 Full LED Test Tool     \n");
    printf("===========================================\n\n");

    // -------------------------------------------------------------
    // TEST 1: Direct RawMIDI (/dev/snd/midiC1D0)
    // -------------------------------------------------------------
    printf("[1] Testing RawMIDI direct device /dev/snd/midiC1D0...\n");
    int raw_fd = open("/dev/snd/midiC1D0", O_WRONLY | O_NONBLOCK);
    if (raw_fd >= 0) {
        printf("    Opened /dev/snd/midiC1D0 successfully!\n");

        // Send SysEx Handshakes
        unsigned char sysex1[] = {0xF0, 0x00, 0x20, 0x7F, 0x03, 0x01, 0xF7};
        unsigned char sysex2[] = {0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7};
        unsigned char sysex3[] = {0xF0, 0x00, 0x20, 0x2B, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xF7};

        write(raw_fd, sysex1, sizeof(sysex1));
        write(raw_fd, sysex2, sizeof(sysex2));
        write(raw_fd, sysex3, sizeof(sysex3));

        // Blast ALL Note On (0x90 .. 0x9F) and CC (0xB0 .. 0xBF) to 127
        printf("    Blasting Note On (0x90..0x9F) and CC (0xB0..0xBF) to RawMIDI...\n");
        for (int ch = 0; ch < 16; ch++) {
            for (int note = 0; note < 128; note++) {
                unsigned char noteMsg[3] = { (unsigned char)(0x90 | ch), (unsigned char)note, 0x7F };
                unsigned char ccMsg[3]   = { (unsigned char)(0xB0 | ch), (unsigned char)note, 0x7F };
                write(raw_fd, noteMsg, 3);
                write(raw_fd, ccMsg, 3);
            }
        }
        close(raw_fd);
        printf("    RawMIDI blast complete!\n\n");
    } else {
        printf("    /dev/snd/midiC1D0 not accessible or busy: %s\n\n", strerror(errno));
    }

    // -------------------------------------------------------------
    // TEST 2: ALSA Sequencer (snd_seq)
    // -------------------------------------------------------------
    printf("[2] Testing ALSA Sequencer (snd_seq)...\n");
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        printf("    Failed to open ALSA sequencer.\n");
        return 1;
    }
    snd_seq_set_client_name(seq, "FLX6 LED Tester");
    int port = snd_seq_create_simple_port(seq, "Out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

    // Find DDJ-FLX6 client
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);

    int target_client = -1, target_port = -1;
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);
        const char *name = snd_seq_client_info_get_name(cinfo);
        if (name && strstr(name, "DDJ-FLX6")) {
            snd_seq_port_info_set_client(pinfo, client);
            snd_seq_port_info_set_port(pinfo, -1);
            if (snd_seq_query_next_port(seq, pinfo) >= 0) {
                target_client = client;
                target_port = snd_seq_port_info_get_port(pinfo);
                printf("    Found target ALSA client [%d:%d] '%s'\n", target_client, target_port, name);
                break;
            }
        }
    }

    if (target_client >= 0) {
        snd_seq_connect_to(seq, port, target_client, target_port);

        snd_midi_event_t *coder = NULL;
        snd_midi_event_new(1024, &coder);

        // SysEx Handshakes
        unsigned char sysex1[] = {0xF0, 0x00, 0x20, 0x7F, 0x03, 0x01, 0xF7};
        unsigned char sysex2[] = {0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7};
        unsigned char sysex3[] = {0xF0, 0x00, 0x20, 0x2B, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xF7};

        send_bytes(seq, port, coder, sysex1, sizeof(sysex1));
        send_bytes(seq, port, coder, sysex2, sizeof(sysex2));
        send_bytes(seq, port, coder, sysex3, sizeof(sysex3));

        printf("    Sending NoteOn/CC to ALSA Sequencer...\n");
        for (int ch = 0; ch < 16; ch++) {
            for (int note = 0; note < 128; note++) {
                unsigned char noteMsg[3] = { (unsigned char)(0x90 | ch), (unsigned char)note, 0x7F };
                unsigned char ccMsg[3]   = { (unsigned char)(0xB0 | ch), (unsigned char)note, 0x7F };
                send_bytes(seq, port, coder, noteMsg, 3);
                send_bytes(seq, port, coder, ccMsg, 3);
            }
        }
        snd_midi_event_free(coder);
        printf("    ALSA Sequencer blast complete!\n");
    } else {
        printf("    DDJ-FLX6 ALSA client not found.\n");
    }

    snd_seq_close(seq);
    printf("\nAll LED tests dispatched. Check DDJ-FLX6 hardware!\n");
    return 0;
}
