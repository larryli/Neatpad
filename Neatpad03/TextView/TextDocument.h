#ifndef TEXTDOC_INCLUDED
#define TEXTDOC_INCLUDED

typedef struct {
    char *buffer;
    ULONG length;

    ULONG *linebuffer;
    ULONG numlines;
} TEXTDOCUMENT;

TEXTDOCUMENT *TextDocument__new(void);
void TextDocument__delete(TEXTDOCUMENT *ptd);
BOOL TextDocument__init(TEXTDOCUMENT *ptd, HANDLE hFile);
BOOL TextDocument__init_filename(TEXTDOCUMENT *ptd, char *filename);
BOOL TextDocument__clear(TEXTDOCUMENT *ptd);
ULONG TextDocument__getline(TEXTDOCUMENT *ptd, ULONG lineno, char *buf,
                            size_t len);
ULONG TextDocument__linecount(TEXTDOCUMENT *ptd);
ULONG TextDocument__longestline(TEXTDOCUMENT *ptd, int tabwidth);
// private
BOOL TextDocument__init_linebuffer(TEXTDOCUMENT *ptd);

#endif
