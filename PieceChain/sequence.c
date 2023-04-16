/*
   sequence.cpp

   data-sequence class

   Copyright J Brown 1999-2006
   www.catch22.net

   Freeware
 */
#include <stdarg.h>
#include <stdio.h>
#include <windows.h>

#include "sequence.h"

#ifdef DEBUG_SEQUENCE

char debugfile[MAX_PATH];
void odebug(const char *fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    char buf[512];

    vsprintf(buf, fmt, varg);
    OutputDebugString(buf);

    va_end(varg);
}

void debug(const char *fmt, ...)
{
    FILE *fp;
    va_list varg;
    va_start(varg, fmt);

    if ((fp = fopen(debugfile, "a")) != 0) {
        vfprintf(fp, fmt, varg);
        fclose(fp);
    }

    va_end(varg);
}

#else
#define debug
#define odebug
#endif

sequence *sequence__new(void)
{
    sequence *ps = malloc(sizeof(sequence));
    if (ps == NULL)
        return NULL;

    vector_setup(&(ps->undostack), 10, sizeof(sequence__span_range *));
    vector_setup(&(ps->redostack), 10, sizeof(sequence__span_range *));
    vector_setup(&(ps->buffer_list), 10, sizeof(sequence__buffer_control *));

    sequence__record_action(ps, action_invalid, 0);

    ps->head = ps->tail = 0;
    ps->sequence_length = 0;
    ps->group_id = 0;
    ps->group_refcount = 0;

    ps->head = sequence__span__new(0, 0, 0);
    ps->tail = sequence__span__new(0, 0, 0);
    ps->head->next = ps->tail;
    ps->tail->prev = ps->head;

#ifdef DEBUG_SEQUENCE
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf(debugfile, "seqlog-%04d%02d%02d-%02d%02d%0d.txt", st.wYear,
            st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
#endif

    return ps;
}

void sequence__delete(sequence *ps)
{
    sequence__clear(ps);

    sequence__span__delete(ps->head);
    sequence__span__delete(ps->tail);

    vector_destroy(&(ps->undostack));
    vector_destroy(&(ps->redostack));
    vector_destroy(&(ps->buffer_list));
    free(ps);
}

bool sequence__init(sequence *ps)
{
    ps->sequence_length = 0;

    if (!sequence__alloc_modifybuffer(ps, 0x10000))
        return false;

    sequence__record_action(ps, action_invalid, 0);
    ps->group_id = 0;
    ps->group_refcount = 0;
    ps->undoredo_index = 0;
    ps->undoredo_length = 0;

    return true;
}

bool sequence__init_buffer(sequence *ps, const seqchar *buffer, size_t length)
{
    sequence__clear(ps);

    if (!sequence__init(ps))
        return false;

    sequence__buffer_control *bc = sequence__alloc_modifybuffer(ps, length);
    memcpy(bc->buffer, buffer, length * sizeof(seqchar));
    bc->length = length;

    sequence__span *sptr =
        sequence__span__new_nx_pr(0, length, bc->id, ps->tail, ps->head);
    ps->head->next = sptr;
    ps->tail->prev = sptr;

    ps->sequence_length = length;
    return true;
}

//
//  Initialize from an on-disk file
//
bool sequence__open(sequence *ps, TCHAR *filename, bool readonly)
{
    return false;
}

void sequence__clear_vector_sr(Vector *vector)
{
    VECTOR_FOR_EACH(vector, i)
    {
        sequence__span_range *pssr =
            ITERATOR_GET_AS(sequence__span_range *, &i);
        sequence__span_range__delete(pssr);
    }
}

void sequence__clear_vector_bc(Vector *vector)
{
    VECTOR_FOR_EACH(vector, i)
    {
        sequence__buffer_control *psbc =
            ITERATOR_GET_AS(sequence__buffer_control *, &i);
        free(psbc);
        ;
    }
}

void sequence__clearstack(sequence *ps, Vector *dest)
{
    VECTOR_FOR_EACH(dest, i)
    {
        sequence__span_range *pssr =
            ITERATOR_GET_AS(sequence__span_range *, &i);
        sequence__span_range__free(pssr);
        sequence__span_range__delete(pssr);
    }
    vector_clear(dest);
}

void sequence__debug1(sequence *ps)
{
    sequence__span *sptr;

    for (sptr = ps->head; sptr; sptr = sptr->next) {
        sequence__buffer_control *psbc = VECTOR_GET_AS(
            sequence__buffer_control *, &(ps->buffer_list), sptr->buffer);
        char *buffer = (char *)psbc->buffer;
        printf("%.*s", sptr->length, buffer + sptr->offset);
    }

    printf("\n");
}

void sequence__debug2(sequence *ps)
{
    sequence__span *sptr;

    printf("**********************\n");
    for (sptr = ps->head; sptr; sptr = sptr->next) {
        sequence__buffer_control *psbc = VECTOR_GET_AS(
            sequence__buffer_control *, &(ps->buffer_list), sptr->buffer);
        char *buffer = (char *)psbc->buffer;

        printf("[%d] [%4d %4d] %.*s\n", sptr->id, sptr->offset, sptr->length,
               sptr->length, buffer + sptr->offset);
    }

    printf("-------------------------\n");

    for (sptr = ps->tail; sptr; sptr = sptr->prev) {
        sequence__buffer_control *psbc = VECTOR_GET_AS(
            sequence__buffer_control *, &(ps->buffer_list), sptr->buffer);
        char *buffer = (char *)psbc->buffer;

        printf("[%d] [%4d %4d] %.*s\n", sptr->id, sptr->offset, sptr->length,
               sptr->length, buffer + sptr->offset);
    }

    printf("**********************\n");

    for (sptr = ps->head; sptr; sptr = sptr->next) {
        sequence__buffer_control *psbc = VECTOR_GET_AS(
            sequence__buffer_control *, &(ps->buffer_list), sptr->buffer);
        char *buffer = (char *)psbc->buffer;
        printf("%.*s", sptr->length, buffer + sptr->offset);
    }

    printf("\nsequence length = %d chars\n", ps->sequence_length);
    printf("\n\n");
}

//
//  Allocate a buffer and add it to our 'buffer control' list
//
sequence__buffer_control *sequence__alloc_buffer(sequence *ps, size_t maxsize)
{
    sequence__buffer_control *bc;

    if ((bc = malloc(sizeof(sequence__buffer_control))) == 0)
        return 0;

    // allocate a new buffer of byte/wchar/long/whatever
    if ((bc->buffer = malloc(sizeof(seqchar) * maxsize)) == 0) {
        free(bc);
        return 0;
    }

    bc->length = 0;
    bc->maxsize = maxsize;
    bc->id = ps->buffer_list.size; // assign the id

    vector_push_back(&(ps->buffer_list), &bc);

    return bc;
}

sequence__buffer_control *sequence__alloc_modifybuffer(sequence *ps,
                                                       size_t maxsize)
{
    sequence__buffer_control *bc;

    if ((bc = sequence__alloc_buffer(ps, maxsize)) == 0)
        return 0;

    ps->modifybuffer_id = bc->id;
    ps->modifybuffer_pos = 0;

    return bc;
}

//
//  Import the specified range of data into the sequence so we have our own
// private copy
//
bool sequence__import_buffer(sequence *ps, const seqchar *buf, size_t len,
                             size_t *buffer_offset)
{
    sequence__buffer_control *bc;

    // get the current modify-buffer
    bc = VECTOR_GET_AS(sequence__buffer_control *, &(ps->buffer_list),
                       ps->modifybuffer_id);

    // if there isn't room then allocate a new modify-buffer
    if (bc->length + len >= bc->maxsize) {
        bc = sequence__alloc_modifybuffer(ps, len + 0x10000);

        // make sure that no old spans use this buffer
        sequence__record_action(ps, action_invalid, 0);
    }

    if (bc == 0)
        return false;

    // import the data
    memcpy(bc->buffer + bc->length, buf, len * sizeof(seqchar));

    *buffer_offset = bc->length;
    bc->length += len;

    return true;
}

//
//  sequence__spanfromindex
//
//  search the spanlist for the span which encompasses the specified index
// position
//
//  index       - character-position index
//  *spanindex  - index of span within sequence
//
sequence__span *sequence__spanfromindex(sequence *ps, size_w index,
                                        size_w *spanindex)
{
    sequence__span *sptr;
    size_w curidx = 0;

    // scan the list looking for the span which holds the specified index
    for (sptr = ps->head->next; sptr->next; sptr = sptr->next) {
        if (index >= curidx && index < curidx + sptr->length) {
            if (spanindex)
                *spanindex = curidx;

            return sptr;
        }

        curidx += sptr->length;
    }

    // insert at tail
    if (sptr && index == curidx) {
        *spanindex = curidx;
        return sptr;
    }

    return 0;
}

void sequence__swap_spanrange(sequence *ps, sequence__span_range *src,
                              sequence__span_range *dest)
{
    if (src->boundary) {
        if (!dest->boundary) {
            src->first->next = dest->first;
            src->last->prev = dest->last;
            dest->first->prev = src->first;
            dest->last->next = src->last;
        }
    } else {
        if (dest->boundary) {
            src->first->prev->next = src->last->next;
            src->last->next->prev = src->first->prev;
        } else {
            src->first->prev->next = dest->first;
            src->last->next->prev = dest->last;
            dest->first->prev = src->first->prev;
            dest->last->next = src->last->next;
        }
    }
}

void sequence__restore_spanrange(sequence *ps, sequence__span_range *range,
                                 bool undo_or_redo)
{
    if (range->boundary) {
        sequence__span *first = range->first->next;
        sequence__span *last = range->last->prev;

        // unlink spans from main list
        range->first->next = range->last;
        range->last->prev = range->first;

        // store the span range we just removed
        range->first = first;
        range->last = last;
        range->boundary = false;
    } else {
        sequence__span *first = range->first->prev;
        sequence__span *last = range->last->next;

        // are we moving spans into an "empty" region?
        // (i.e. inbetween two adjacent spans)
        if (first->next == last) {
            // move the old spans back into the empty region
            first->next = range->first;
            last->prev = range->last;

            // store the span range we just removed
            range->first = first;
            range->last = last;
            range->boundary = true;
        }
        // we are replacing a range of spans in the list,
        // so swap the spans in the list with the one's in our "undo" event
        else {
            // find the span range that is currently in the list
            first = first->next;
            last = last->prev;

            // unlink the the spans from the main list
            first->prev->next = range->first;
            last->next->prev = range->last;

            // store the span range we just removed
            range->first = first;
            range->last = last;
            range->boundary = false;
        }
    }

    // update the 'sequence length' and 'quicksave' states
    // std__swap(range->sequence_length, sequence_length);
    size_w temp;
    temp = range->sequence_length;
    range->sequence_length = ps->sequence_length;
    ps->sequence_length = temp;
    // std__swap(range->quicksave, can_quicksave);
    bool tempb;
    tempb = range->quicksave;
    range->quicksave = ps->can_quicksave;
    ps->can_quicksave = tempb;

    ps->undoredo_index = range->index;

    if (range->act == action_erase && undo_or_redo == true ||
        range->act != action_erase && undo_or_redo == false) {
        ps->undoredo_length = range->length;
    } else {
        ps->undoredo_length = 0;
    }
}

//
//  sequence__undoredo
//
//  private routine used to undo/redo spanrange events to/from
//  the sequence - handles 'grouped' events
//
bool sequence__undoredo(sequence *ps, Vector *source, Vector *dest)
{
    sequence__span_range *range = 0;
    size_t group_id;

    if (vector_is_empty(source))
        return false;

    // make sure that no "optimized" actions can occur
    sequence__record_action(ps, action_invalid, 0);

    group_id = (*(sequence__span_range **)vector_back(source))->group_id;

    do {
        // remove the next event from the source stack
        range = *(sequence__span_range **)vector_back(source);
        vector_pop_back(source);

        // add event onto the destination stack
        vector_push_back(dest, &range);

        // do the actual work
        sequence__restore_spanrange(ps, range,
                                    source == &(ps->undostack) ? true : false);
    } while (!vector_is_empty(source) &&
             ((*(sequence__span_range **)vector_back(source))->group_id ==
                  group_id &&
              group_id != 0));

    return true;
}

//
//  UNDO the last action
//
bool sequence__undo(sequence *ps)
{
    debug("Undo\n");
    return sequence__undoredo(ps, &(ps->undostack), &(ps->redostack));
}

//
//  REDO the last UNDO
//
bool sequence__redo(sequence *ps)
{
    debug("Redo\n");
    return sequence__undoredo(ps, &(ps->redostack), &(ps->undostack));
}

//
//  Will calling sequence__undo change the sequence?
//
bool sequence__canundo(sequence *ps) { return ps->undostack.size != 0; }

//
//  Will calling sequence__redo change the sequence?
//
bool sequence__canredo(sequence *ps) { return ps->redostack.size != 0; }

//
//  Group repeated actions on the sequence (insert/erase etc)
//  into a single 'undoable' action
//
void sequence__group(sequence *ps)
{
    if (ps->group_refcount == 0) {
        if (++ps->group_id == 0)
            ++ps->group_id;

        ps->group_refcount++;
    }
}

//
//  Close the grouping
//
void sequence__ungroup(sequence *ps)
{
    if (ps->group_refcount > 0)
        ps->group_refcount--;
}

//
//  Return logical length of the sequence
//
size_w sequence__size(sequence *ps) { return ps->sequence_length; }

//
//  sequence__initundo
//
//  create a new (empty) span range and save the current sequence state
//
sequence__span_range *sequence__initundo(sequence *ps, size_w index,
                                         size_w length, sequence__action act)
{
    sequence__span_range *event = sequence__span_range__new_(
        ps->sequence_length, index, length, act, ps->can_quicksave,
        ps->group_refcount ? ps->group_id : 0);

    vector_push_back(&(ps->undostack), &event);

    return event;
}

sequence__span_range *sequence__stackback(sequence *ps, Vector *source,
                                          size_t idx)
{
    size_t length = source->size;

    if (length > 0 && idx < length) {
        return VECTOR_GET_AS(sequence__span_range *, source, length - idx - 1);
    } else {
        return 0;
    }
}

void sequence__record_action(sequence *ps, sequence__action act, size_w index)
{
    ps->lastaction_index = index;
    ps->lastaction = act;
}

bool sequence__can_optimize(sequence *ps, sequence__action act, size_w index)
{
    return (ps->lastaction == act && ps->lastaction_index == index);
}

//
//  sequence__insert_worker
//
bool sequence__insert_worker(sequence *ps, size_w index, const seqchar *buf,
                             size_w length, sequence__action act)
{
    sequence__span *sptr;
    size_w spanindex;
    size_t modbuf_offset;
    sequence__span_range *newspans = sequence__span_range__new();
    size_w insoffset;

    if (index > ps->sequence_length) {
        sequence__span_range__delete(newspans);
        return false;
    }
    // find the span that the insertion starts at
    if ((sptr = sequence__spanfromindex(ps, index, &spanindex)) == 0) {
        sequence__span_range__delete(newspans);
        return false;
    }
    // ensure there is room in the modify buffer...
    // allocate a new buffer if necessary and then invalidate span cache
    // to prevent a span using two buffers of data
    if (!sequence__import_buffer(ps, buf, length, &modbuf_offset)) {
        sequence__span_range__delete(newspans);
        return false;
    }

    debug("Inserting: idx=%d len=%d %.*s\n", index, length, length, buf);

    sequence__clearstack(ps, &(ps->redostack));
    insoffset = index - spanindex;

    // special-case #1: inserting at the end of a prior insertion, at a
    // span-boundary
    if (insoffset == 0 && sequence__can_optimize(ps, act, index)) {
        // simply extend the last span's length
        sequence__span_range *event =
            *(sequence__span_range **)vector_back(&(ps->undostack));
        sptr->prev->length += length;
        event->length += length;
    }
    // general-case #1: inserting at a span boundary?
    else if (insoffset == 0) {
        //
        // Create a new undo event; because we are inserting at a span
        // boundary there are no spans to replace, so use a "span boundary"
        //
        sequence__span_range *oldspans =
            sequence__initundo(ps, index, length, act);
        sequence__span_range__spanboundary(oldspans, sptr->prev, sptr);

        // allocate new span in the modify buffer
        sequence__span_range__append(
            newspans,
            sequence__span__new(modbuf_offset, length, ps->modifybuffer_id));

        // link the span into the sequence
        sequence__swap_spanrange(ps, oldspans, newspans);
    }
    // general-case #2: inserting in the middle of a span
    else {
        //
        //  Create a new undo event and add the span
        //  that we will be "splitting" in half
        //
        sequence__span_range *oldspans =
            sequence__initundo(ps, index, length, act);
        sequence__span_range__append(oldspans, sptr);

        //  span for the existing data before the insertion
        sequence__span_range__append(
            newspans,
            sequence__span__new(sptr->offset, insoffset, sptr->buffer));

        // make a span for the inserted data
        sequence__span_range__append(
            newspans,
            sequence__span__new(modbuf_offset, length, ps->modifybuffer_id));

        // span for the existing data after the insertion
        sequence__span_range__append(
            newspans,
            sequence__span__new(sptr->offset + insoffset,
                                sptr->length - insoffset, sptr->buffer));

        sequence__swap_spanrange(ps, oldspans, newspans);
    }

    ps->sequence_length += length;

    sequence__span_range__delete(newspans);
    return true;
}

//
//  sequence__insert
//
//  Insert a buffer into the sequence at the specified position.
//  Consecutive insertions are optimized into a single event
//
bool sequence__insert_buf(sequence *ps, size_w index, const seqchar *buf,
                          size_w length)
{
    if (sequence__insert_worker(ps, index, buf, length, action_insert)) {
        sequence__record_action(ps, action_insert, index + length);
        return true;
    } else {
        return false;
    }
}

//
//  sequence__insert
//
//  Insert specified character-value into sequence
//
bool sequence__insert(sequence *ps, size_w index, const seqchar val)
{
    return sequence__insert_buf(ps, index, &val, 1);
}

//
//  sequence__deletefromsequence
//
//  Remove + delete the specified *span* from the sequence
//
void sequence__deletefromsequence(sequence *ps, sequence__span **psptr)
{
    sequence__span *sptr = *psptr;
    sptr->prev->next = sptr->next;
    sptr->next->prev = sptr->prev;

    memset(sptr, 0, sizeof(sequence__span));
    sequence__span__delete(sptr);
    *psptr = 0;
}

//
//  sequence__erase_worker
//
bool sequence__erase_worker(sequence *ps, size_w index, size_w length,
                            sequence__action act)
{
    sequence__span *sptr;
    sequence__span_range *oldspans = sequence__span_range__new();
    sequence__span_range *newspans = sequence__span_range__new();
    sequence__span_range *event;
    size_w spanindex;
    size_w remoffset;
    size_w removelen;
    bool append_spanrange;

    debug("Erasing: idx=%d len=%d\n", index, length);

    // make sure we stay within the range of the sequence
    if (length == 0 || length > ps->sequence_length ||
        index > ps->sequence_length - length) {
        sequence__span_range__delete(oldspans);
        sequence__span_range__delete(newspans);
        return false;
    }
    // find the span that the deletion starts at
    if ((sptr = sequence__spanfromindex(ps, index, &spanindex)) == 0) {
        sequence__span_range__delete(oldspans);
        sequence__span_range__delete(newspans);
        return false;
    }
    // work out the offset relative to the start of the *span*
    remoffset = index - spanindex;
    removelen = length;

    //
    //  can we optimize?
    //
    //  special-case 1: 'forward-delete'
    //  erase+replace operations will pass through here
    //
    if (index == spanindex && sequence__can_optimize(ps, act, index)) {
        event = sequence__stackback(ps, &(ps->undostack),
                                    act == action_replace ? 1 : 0);
        event->length += length;
        append_spanrange = true;

        if (ps->frag2 != 0) {
            if (length < ps->frag2->length) {
                ps->frag2->length -= length;
                ps->frag2->offset += length;
                ps->sequence_length -= length;
                sequence__span_range__delete(oldspans);
                sequence__span_range__delete(newspans);
                return true;
            } else {
                if (act == action_replace)
                    sequence__stackback(ps, &(ps->undostack), 0)->last =
                        ps->frag2->next;

                removelen -= sptr->length;
                sptr = sptr->next;
                sequence__deletefromsequence(ps, &(ps->frag2));
            }
        }
    }
    //
    //  special-case 2: 'backward-delete'
    //  only erase operations can pass through here
    //
    else if (index + length == spanindex + sptr->length &&
             sequence__can_optimize(ps, action_erase, index + length)) {
        event = *(sequence__span_range **)vector_back(&(ps->undostack));
        event->length += length;
        event->index -= length;
        append_spanrange = false;

        if (ps->frag1 != 0) {
            if (length < ps->frag1->length) {
                ps->frag1->length -= length;
                ps->frag1->offset += 0;
                ps->sequence_length -= length;
                sequence__span_range__delete(oldspans);
                sequence__span_range__delete(newspans);
                return true;
            } else {
                removelen -= ps->frag1->length;
                sequence__deletefromsequence(ps, &(ps->frag1));
            }
        }
    } else {
        append_spanrange = true;
        ps->frag1 = ps->frag2 = 0;

        if ((event = sequence__initundo(ps, index, length, act)) == 0) {
            sequence__span_range__delete(oldspans);
            sequence__span_range__delete(newspans);
            return false;
        }
    }

    //
    //  general-case 2+3
    //
    sequence__clearstack(ps, &(ps->redostack));

    // does the deletion *start* mid-way through a span?
    if (remoffset != 0) {
        // split the span - keep the first "half"
        sequence__span_range__append(
            newspans,
            sequence__span__new(sptr->offset, remoffset, sptr->buffer));
        ps->frag1 = newspans->first;

        // have we split a single span into two?
        // i.e. the deletion is completely within a single span
        if (remoffset + removelen < sptr->length) {
            // make a second span for the second half of the split
            sequence__span_range__append(
                newspans,
                sequence__span__new(sptr->offset + remoffset + removelen,
                                    sptr->length - remoffset - removelen,
                                    sptr->buffer));

            ps->frag2 = newspans->last;
        }

        removelen -= min(removelen, (sptr->length - remoffset));

        // archive the span we are going to replace
        sequence__span_range__append(oldspans, sptr);
        sptr = sptr->next;
    }
    // we are now on a proper span boundary, so remove
    // any further spans that the erase-range encompasses
    while (removelen > 0 && sptr != ps->tail) {
        // will the entire span be removed?
        if (removelen < sptr->length) {
            // split the span, keeping the last "half"
            sequence__span_range__append(
                newspans,
                sequence__span__new(sptr->offset + removelen,
                                    sptr->length - removelen, sptr->buffer));

            ps->frag2 = newspans->last;
        }

        removelen -= min(removelen, sptr->length);

        // archive the span we are replacing
        sequence__span_range__append(oldspans, sptr);
        sptr = sptr->next;
    }

    // for replace operations, update the undo-event for the
    // insertion so that it knows about the newly removed spans
    if (act == action_replace && !oldspans->boundary)
        sequence__stackback(ps, &(ps->undostack), 0)->last =
            oldspans->last->next;

    sequence__swap_spanrange(ps, oldspans, newspans);
    ps->sequence_length -= length;

    if (append_spanrange)
        sequence__span_range__append_range(event, oldspans);
    else
        sequence__span_range__append_range(event, oldspans);

    sequence__span_range__delete(oldspans);
    sequence__span_range__delete(newspans);
    return true;
}

//
//  sequence__erase
//
//  "removes" the specified range of data from the sequence.
//
bool sequence__erase_len(sequence *ps, size_w index, size_w len)
{
    if (sequence__erase_worker(ps, index, len, action_erase)) {
        sequence__record_action(ps, action_erase, index);
        return true;
    } else {
        return false;
    }
}

//
//  sequence__erase
//
//  remove single character from sequence
//
bool sequence__erase(sequence *ps, size_w index)
{
    return sequence__erase_len(ps, index, 1);
}

//
//  sequence__replace
//
//  A 'replace' (or 'overwrite') is a combination of erase+inserting
//  (first we erase a section of the sequence, then insert a new block
//  in it's place).
//
//  Doing this as a distinct operation (erase+insert at the
//  same time) is really complicated, so I just make use of the existing
//  sequence__erase and sequence__insert and combine them into action. We
//  need to play with the undo stack to combine them in a 'true' sense.
//
bool sequence__replace_buf_erase(sequence *ps, size_w index, const seqchar *buf,
                                 size_w length, size_w erase_length)
{
    size_t remlen = 0;

    debug("Replacing: idx=%d len=%d %.*s\n", index, length, length, buf);

    // make sure operation is within allowed range
    if (index > ps->sequence_length || MAX_SEQUENCE_LENGTH - index < length)
        return false;

    // for a "replace" which will overrun the sequence, make sure we
    // only delete up to the end of the sequence
    remlen = min(ps->sequence_length - index, erase_length);

    // combine the erase+insert actions together
    sequence__group(ps);

    // first of all remove the range
    if (remlen > 0 && index < ps->sequence_length &&
        !sequence__erase_worker(ps, index, remlen, action_replace)) {
        sequence__ungroup(ps);
        return false;
    }
    // then insert the data
    if (sequence__insert_worker(ps, index, buf, length, action_replace)) {
        sequence__ungroup(ps);
        sequence__record_action(ps, action_replace, index + length);
        return true;
    } else {
        // failed...cleanup what we have done so far
        sequence__ungroup(ps);
        sequence__record_action(ps, action_invalid, 0);

        sequence__span_range *range =
            *(sequence__span_range **)vector_back(&(ps->undostack));
        vector_pop_back(&(ps->undostack));
        sequence__restore_spanrange(ps, range, true);
        sequence__span_range__delete(range);

        return false;
    }
}

//
//  sequence__replace
//
//  overwrite with the specified buffer
//
bool sequence__replace_buf(sequence *ps, size_w index, const seqchar *buf,
                           size_w length)
{
    return sequence__replace_buf_erase(ps, index, buf, length, length);
}

//
//  sequence__replace
//
//  overwrite with a single character-value
//
bool sequence__replace(sequence *ps, size_w index, const seqchar val)
{
    return sequence__replace_buf(ps, index, &val, 1);
}

//
//  sequence__append
//
//  very simple wrapper around sequence__insert, just inserts at
//  the end of the sequence
//
bool sequence__append_buf(sequence *ps, const seqchar *buf, size_w length)
{
    return sequence__insert_buf(ps, sequence__size(ps), buf, length);
}

//
//  sequence__append
//
//  append a single character to the sequence
//
bool sequence__append(sequence *ps, const seqchar val)
{
    return sequence__append_buf(ps, &val, 1);
}

//
//  sequence__clear
//
//  empty the entire sequence, clear undo/redo history etc
//
bool sequence__clear(sequence *ps)
{
    sequence__span *sptr, *tmp;

    // delete all spans in the sequence
    for (sptr = ps->head->next; sptr != ps->tail; sptr = tmp) {
        tmp = sptr->next;
        sequence__span__delete(sptr);
    }

    // re-link the head+tail
    ps->head->next = ps->tail;
    ps->tail->prev = ps->head;

    // delete everything in the undo/redo stacks
    sequence__clearstack(ps, &(ps->undostack));
    sequence__clearstack(ps, &(ps->redostack));

    // delete all memory-buffers
    VECTOR_FOR_EACH(&(ps->buffer_list), i)
    {
        sequence__buffer_control *psbc =
            ITERATOR_GET_AS(sequence__buffer_control *, &i);
        free(psbc->buffer);
        free(psbc);
    }

    vector_clear(&(ps->buffer_list));
    ps->sequence_length = 0;
    return true;
}

//
//  sequence__render
//
//  render the specified range of data (index, len) and store in 'dest'
//
//  Returns number of chars copied into destination
//
size_w sequence__render(sequence *ps, size_w index, seqchar *dest,
                        size_w length)
{
    size_w spanoffset = 0;
    size_w total = 0;
    sequence__span *sptr;

    // find span to start rendering at
    if ((sptr = sequence__spanfromindex(ps, index, &spanoffset)) == 0)
        return false;

    // might need to start mid-way through the first span
    spanoffset = index - spanoffset;

    // copy each span's referenced data in succession
    while (length && sptr != ps->tail) {
        size_w copylen = min(sptr->length - spanoffset, length);
        sequence__buffer_control *psbc = VECTOR_GET_AS(
            sequence__buffer_control *, &(ps->buffer_list), sptr->buffer);
        seqchar *source = psbc->buffer;

        memcpy(dest, source + sptr->offset + spanoffset,
               copylen * sizeof(seqchar));

        dest += copylen;
        length -= copylen;
        total += copylen;

        sptr = sptr->next;
        spanoffset = 0;
    }

    return total;
}

//
//  sequence__peek
//
//  return single element at specified position in the sequence
//
seqchar sequence__peek(sequence *ps, size_w index)
{
    seqchar value;
    return sequence__render(ps, index, &value, 1) ? value : 0;
}

//
//  sequence__poke
//
//  modify single element at specified position in the sequence
//
bool sequence__poke(sequence *ps, size_w index, seqchar value)
{
    return sequence__replace_buf(ps, index, &value, 1);
}

//
//  sequence__operator[] const
//
//  readonly array access
//
seqchar sequence__operator_char(sequence *ps, size_w index)
{
    return sequence__peek(ps, index);
}

//
//  sequence__operator[]
//
//  read/write array access
//
sequence__ref *sequence__operator_ref(sequence *ps, size_w index)
{
    return sequence__ref__new(ps, index);
}

//
//  sequence__breakopt
//
//  Prevent subsequent operations from being optimized (coalesced)
//  with the last.
//
void sequence__breakopt(sequence *ps) { ps->lastaction = action_invalid; }
