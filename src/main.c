#include <stdio.h>
#include <stdbool.h>
#include "player.h"
#include <unistd.h>

bool valid_args(int argc, char *argv[]);

int main(int argc, char *argv[]){

    if(!valid_args(argc, argv)){
        return 1;
    };
    playVideo(argv[1]);
    return 0;
}

bool valid_args(int argc, char *argv[]) {
    fflush(stdout);

    // Check that correct arguments are given
    if (argc != 2){
        fprintf(stderr, "Need a filename\n");
        return false;
    }

    // Check that the argument is an existing file
    if(access(argv[1], F_OK) == -1){
        fprintf(stderr, "Invalid file name, \"%s\".\n", argv[1]);
        return false;
    }
    return true;
}