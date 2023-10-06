#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>
#include <sys/types.h>      // for ssize_t
#include "imp/imp_audio.h"  // for IMPAudioBitWidth, IMPAudioSoundMode

#define PROG_TAG "AO_T31"

typedef struct ClientNode {
    int sockfd;  // Socket descriptor for the client
    struct ClientNode *next;  // Pointer to the next client node
} ClientNode;

extern ClientNode *client_list_head;
extern pthread_mutex_t audio_buffer_lock;   // Declaration for the global variable
extern pthread_cond_t audio_data_cond;      // Declaration for the global variable
extern unsigned char *audio_buffer;
extern ssize_t audio_buffer_size;
extern int active_client_sock;

int create_thread(pthread_t *thread_id, void *(*start_routine) (void *), void *arg);

int compute_numPerFrm(int sample_rate);

IMPAudioBitWidth string_to_bitwidth(const char* str);
IMPAudioSoundMode string_to_soundmode(const char* str);

// Function declarations
void perform_cleanup(void);
void handle_sigint(int sig);

#endif // UTILS_H
