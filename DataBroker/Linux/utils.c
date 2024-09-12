#include "utils.h"

bool strtobool(char *string){
    char *true_string = "true";

    for ( ; *string; ++string) *string = tolower(*string);
    if (strcmp(string,true_string) == 0){
        return true;
    }
    else{
        return false;
    }
}