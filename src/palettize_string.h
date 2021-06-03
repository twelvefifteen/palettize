#ifndef PALETTIZE_STRING_H
#define PALETTIZE_STRING_H

inline char flip_case(char c) {
    char result = '\0';

    if(('a' <= c) && (c <= 'z'))
    {
        result = ((c - 'a') + 'A');
    }
    else if(('A' <= c) && (c <= 'Z'))
    {
        result = ((c - 'A') + 'a');
    }
    else
    {
        Invalid_Code_Path;
    }
    
    return(result);
}

inline b32 strings_match(char *a, char *b, b32 case_sensitive = true) {
    while (*a &&
          *b) {
        if ((*a == *b) ||
            ((case_sensitive == false) &&
             (*a == flip_case(*b)))) {
            a++;
            b++;
        } else {
            break;
        }
    }
    
    b32 result = *a == *b;

    return(result);
}

#endif
