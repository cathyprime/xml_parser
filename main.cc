#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "parser.hh"

int main()
{
    XMLDocument doc = {0};
    if (load_file(&doc, "test.xml")) {
        printf("successful :3\n");
        doc.root->print();
    }
    else fprintf(stderr, "error parsing :c\n");
}
