#ifndef CMDLINE_H
#define CMDLINE_H

void print_usage(char *program_name);
int parse_arguments(int argc, char *argv[], int *use_stdin, char **audio_file_path, int *record_audio);

#endif // CMDLINE_H

