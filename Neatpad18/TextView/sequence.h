#ifndef SEQUENCE_INCLUDED
#define SEQUENCE_INCLUDED

#include <stdlib.h>
#include <windows.h>

#include "..\vector\vector.h"

//
//  Define the underlying string/character type of the sequence.
//
//  'seqchar' can be redefined to BYTE, WCHAR, ULONG etc
//  depending on what kind of string you want your sequence to hold
//
typedef unsigned char seqchar;

#ifdef SEQUENCE64
typedef unsigned __int64 size_w;
#else
typedef unsigned long size_w;
#endif

#define MAX_SEQUENCE_LENGTH ((size_w)(-1) / sizeof(seqchar))

typedef struct sequence__span sequence__span;
typedef struct sequence__ref sequence__ref;
typedef struct sequence__span_range sequence__span_range;
typedef struct sequence__buffer_control sequence__buffer_control;

//
//  sequence__action
//
//  enumeration of the type of 'edit actions' our sequence supports.
//  only important when we try to 'optimize' repeated operations on the
//  sequence by coallescing them into a single span.
//
typedef enum {
    action_invalid,
    action_insert,
    action_erase,
    action_replace
} sequence__action;

//
//  sequence class!
//
typedef struct {
    //
    //  Span-table management
    //
    size_w sequence_length;
    sequence__span *head;
    sequence__span *tail;
    sequence__span *frag1;
    sequence__span *frag2;

    Vector undostack; // sequence__span_range *
    Vector redostack; // sequence__span_range *
    size_t group_id;
    size_t group_refcount;
    size_w undoredo_index;
    size_w undoredo_length;

    Vector buffer_list; // sequence__buffer_control *
    int modifybuffer_id;
    int modifybuffer_pos;

    size_w lastaction_index;
    sequence__action lastaction;
    bool can_quicksave;
} sequence;

// public:
// sequence construction
sequence *sequence__new(void);
void sequence__delete(sequence *);

//
// initialize with a file
//
bool sequence__init(sequence *ps);
bool sequence__open(sequence *ps, TCHAR *filename, bool readonly);
bool sequence__clear(sequence *ps);

//
// initialize from an in-memory buffer
//
bool sequence__init_buffer(sequence *ps, const seqchar *buffer, size_t length);

//
//  sequence statistics
//
size_w sequence__size(sequence *ps);

//
// sequence manipulation
//
bool sequence__insert_buf(sequence *ps, size_w index, const seqchar *buf,
                          size_w length);
bool sequence__insert_count(sequence *ps, size_w index, const seqchar val,
                            size_w count);
bool sequence__insert(sequence *ps, size_w index, const seqchar val);
bool sequence__replace_buf_erase(sequence *ps, size_w index, const seqchar *buf,
                                 size_w length, size_w erase_length);
bool sequence__replace_buf(sequence *ps, size_w index, const seqchar *buf,
                           size_w length);
bool sequence__replace_count(sequence *ps, size_w index, const seqchar val,
                             size_w count);
bool sequence__replace(sequence *ps, size_w index, const seqchar val);
bool sequence__erase_len(sequence *ps, size_w index, size_w len);
bool sequence__erase(sequence *ps, size_w index);
bool sequence__append_buf(sequence *ps, const seqchar *buf, size_w len);
bool sequence__append(sequence *ps, const seqchar val);
void sequence__breakopt(sequence *ps);

//
// undo/redo support
//
bool sequence__undo(sequence *ps);
bool sequence__redo(sequence *ps);
bool sequence__canundo(sequence *ps);
bool sequence__canredo(sequence *ps);
void sequence__group(sequence *ps);
void sequence__ungroup(sequence *ps);
inline size_w sequence__event_index(sequence *ps) { return ps->undoredo_index; }
inline size_w sequence__event_length(sequence *ps)
{
    return ps->undoredo_length;
}
// print out the sequence void debug1();
void sequence__debug2(sequence *ps);

//
// access and iteration
//
size_w sequence__render(sequence *ps, size_w index, seqchar *buf, size_w len);
seqchar sequence__peek(sequence *ps, size_w index);
bool sequence__poke(sequence *ps, size_w index, seqchar val);

seqchar sequence__operator_char(sequence *ps, size_w index);
sequence__ref *sequence__operator_ref(sequence *ps, size_w index);

// private:
//     typedef std__vector<span_range *> eventstack;
//     typedef std__vector<buffer_control *> bufferlist;
void sequence__clear_vector_sr(Vector *pv);
void sequence__clear_vector_bc(Vector *pv);

//
//  Span-table management
//
void sequence__deletefromsequence(sequence *ps, sequence__span **sptr);
sequence__span *sequence__spanfromindex(sequence *ps, size_w index,
                                        size_w *spanindex);
void sequence__scan(sequence *ps, sequence__span *sptr);

//
//  Undo and redo stacks
//
sequence__span_range *sequence__initundo(sequence *ps, size_w index,
                                         size_w length, sequence__action act);
void sequence__restore_spanrange(sequence *ps, sequence__span_range *range,
                                 bool undo_or_redo);
void sequence__swap_spanrange(sequence *ps, sequence__span_range *src,
                              sequence__span_range *dest);
bool sequence__undoredo(sequence *ps, Vector *source,
                        Vector *dest); // sequence__span_range *
void sequence__clearstack(sequence *ps,
                          Vector *source); // sequence__span_range *
sequence__span_range *sequence__stackback(sequence *ps, Vector *source,
                                          size_t idx); // sequence__span_range *

//
//  File and memory buffer management
//
sequence__buffer_control *sequence__alloc_buffer(sequence *ps, size_t size);
sequence__buffer_control *sequence__alloc_modifybuffer(sequence *ps,
                                                       size_t size);
bool sequence__import_buffer(sequence *ps, const seqchar *buf, size_t len,
                             size_t *buffer_offset);

//
//  Sequence manipulation
//
bool sequence__insert_worker(sequence *ps, size_w index, const seqchar *buf,
                             size_w len, sequence__action act);
bool sequence__erase_worker(sequence *ps, size_w index, size_w len,
                            sequence__action act);
bool sequence__can_optimize(sequence *ps, sequence__action act, size_w index);
void sequence__record_action(sequence *ps, sequence__action act, size_w index);

void sequence__LOCK(sequence *ps);
void sequence__UNLOCK(sequence *ps);

//
//  sequence__span
//
//  private class to the sequence
//
struct sequence__span {
    sequence__span *next;
    sequence__span *prev; // double-link-list

    size_w offset;
    size_w length;
    int buffer;

    int id;
};

// public:
//  constructor
inline sequence__span *sequence__span__new_nx_pr(size_w off, size_w len,
                                                 int buf, sequence__span *nx,
                                                 sequence__span *pr)
{
    sequence__span *pss = malloc(sizeof(sequence__span));
    if (pss == NULL)
        return NULL;
    pss->offset = off;
    pss->length = len;
    pss->buffer = buf;
    pss->next = nx;
    pss->prev = pr;
    static int count = -2;
    pss->id = count++;
    return pss;
}

inline sequence__span *sequence__span__new(size_w off, size_w len, int buf)
{
    return sequence__span__new_nx_pr(off, len, buf, NULL, NULL);
}

inline void sequence__span__delete(sequence__span *pss) { free(pss); }

//
//  sequence__span_range
//
//  private class to the sequence. Used to represent a contiguous range of
//  spans. used by the undo/redo stacks to store state. A span-range effectively
//  represents the range of spans affected by an event (operation) on the
//  sequence
//
//
struct sequence__span_range {
    // private:
    // the span range
    sequence__span *first;
    sequence__span *last;
    bool boundary;

    // sequence state
    size_w sequence_length;
    size_w index;
    size_w length;
    sequence__action act;
    bool quicksave;
    size_t group_id;
};

// public:
//  constructor
inline sequence__span_range *sequence__span_range__new_(size_w seqlen,
                                                        size_w idx, size_w len,
                                                        sequence__action a,
                                                        bool qs, size_t id)
{
    sequence__span_range *pssr = malloc(sizeof(sequence__span_range));
    if (pssr == NULL)
        return NULL;
    pssr->first = NULL;
    pssr->last = NULL;
    pssr->boundary = true;
    pssr->sequence_length = seqlen;
    pssr->index = idx;
    pssr->length = len;
    pssr->act = a;
    pssr->quicksave = qs;
    pssr->group_id = id;
    return pssr;
}

inline sequence__span_range *sequence__span_range__new(void)
{
    return sequence__span_range__new_(0, 0, 0, action_invalid, false, 0);
}

// destructor does nothing - because sometimes we don't want
// to free the contents when the span_range is deleted. e.g. when
// the span_range is just a temporary helper object. The contents
// must be deleted manually with span_range__free
inline void sequence__span_range__delete(sequence__span_range *pssr)
{
    free(pssr);
}

// separate 'destruction' used when appropriate
inline void sequence__span_range__free(sequence__span_range *pssr)
{
    sequence__span *sptr, *next, *term;

    if (pssr->boundary == false) {
        // delete the range of spans
        for (sptr = pssr->first, term = pssr->last->next; sptr && sptr != term;
             sptr = next) {
            next = sptr->next;
            sequence__span__delete(sptr);
        }
    }
}

// add a span into the range
inline void sequence__span_range__append(sequence__span_range *pssr,
                                         sequence__span *sptr)
{
    if (sptr != 0) {
        // first time a span has been added?
        if (pssr->first == 0) {
            pssr->first = sptr;
        }
        // otherwise chain the spans together.
        else {
            pssr->last->next = sptr;
            sptr->prev = pssr->last;
        }

        pssr->last = sptr;
        pssr->boundary = false;
    }
}

// join two span-ranges together
inline void sequence__span_range__append_range(sequence__span_range *pssr,
                                               sequence__span_range *range)
{
    if (range->boundary == false) {
        if (pssr->boundary) {
            pssr->first = range->first;
            pssr->last = range->last;
            pssr->boundary = false;
        } else {
            range->first->prev = pssr->last;
            pssr->last->next = range->first;
            pssr->last = range->last;
        }
    }
}

// join two span-ranges together. used only for 'back-delete'
inline void sequence__span_range__prepend(sequence__span_range *pssr,
                                          sequence__span_range *range)
{
    if (range->boundary == false) {
        if (pssr->boundary) {
            pssr->first = range->first;
            pssr->last = range->last;
            pssr->boundary = false;
        } else {
            range->last->next = pssr->first;
            pssr->first->prev = range->last;
            pssr->first = range->first;
        }
    }
}

// An 'empty' range is represented by storing pointers to the
// spans ***either side*** of the span-boundary position. Input is
// always the span following the boundary.
inline void sequence__span_range__spanboundary(sequence__span_range *pssr,
                                               sequence__span *before,
                                               sequence__span *after)
{
    pssr->first = before;
    pssr->last = after;
    pssr->boundary = true;
}

//
//  sequence__ref
//
//  temporary 'reference' to the sequence, used for
//  non-const array access with sequence__operator[]
//
struct sequence__ref {
    // private:
    size_w index;
    sequence *seq;
};

// public:
inline sequence__ref *sequence__ref__new(sequence *s, size_w i)
{
    sequence__ref *psr = malloc(sizeof(sequence__ref));
    if (psr == NULL)
        return NULL;
    psr->seq = s;
    psr->index = i;
    return psr;
}

inline void sequence__ref__delete(sequence__ref *psr) { free(psr); }

inline seqchar sequence__ref__seqchar(sequence__ref *psr)
{
    return sequence__peek(psr->seq, psr->index);
}

inline sequence__ref *sequence__ref__is(sequence__ref *psr, seqchar rhs)
{
    sequence__poke(psr->seq, psr->index, rhs);
    return psr;
}

//
//  buffer_control
//
struct sequence__buffer_control {
    seqchar *buffer;
    size_w length;
    size_w maxsize;
    int id;
};
/*
   class sequence: :iterator {
   public:
   };
 */
#endif
