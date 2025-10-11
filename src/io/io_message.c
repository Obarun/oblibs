/*
 * io_message.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <oblibs/io_rb.h>
#include <oblibs/io_message.h>

int io_message_reader_init(io_message_t *reader, io_rb_t *io, void *header_buf, size_t header_size, void *payload_buf, size_t payload_size, void *data)
{
    if (!reader || !io || !header_buf ||
        header_size == 0 || !payload_buf ||
        payload_size == 0)
        return (errno = EINVAL, 0) ;

    memset(reader, 0, sizeof(*reader)) ;

    reader->io = io ;
    reader->state = IO_MESSAGE_HEADER ;
    reader->header_buf = header_buf ;
    reader->header_size = header_size ;
    reader->payload_buf = payload_buf ;
    reader->payload_size = payload_size ;
    reader->data = data ;

    return 1 ;
}

int io_message_read(io_message_t *reader, io_message_header_parser_t *parse_header)
{
    if (!reader || !parse_header)
        return (errno = EINVAL, -1) ;

    if (reader->state == IO_MESSAGE_ERROR)
        return (errno = EPROTO, -1) ;

    if (reader->state == IO_MESSAGE_COMPLETE)
        return 1 ;  // Already have a message

    size_t needed, available, to_read, payload_len ;
    uint8_t *dest ;
    ssize_t r ;

    while (io_rb_pending_in(reader->io)) {

        // Read header
        if (reader->state == IO_MESSAGE_HEADER) {
            needed = reader->header_size - reader->header_received ;
            if (needed == 0) {
                reader->state = IO_MESSAGE_ERROR ;
                return (errno = EOVERFLOW, -1) ;
            }

            available = io_rb_pending_in(reader->io) ;
            if (available == 0)
                return 0 ;  // Need more data

            to_read = (needed < available) ? needed : available ;
            dest = (uint8_t *)reader->header_buf + reader->header_received ;

            r = io_rb_get(reader->io, dest, to_read) ;
            if (r <= 0) {
                reader->state = IO_MESSAGE_ERROR ;
                return -1 ;
            }

            reader->header_received += r ;

            // Try to parse header
            payload_len = 0 ;
            int parsed = parse_header(reader->header_buf, reader->header_received, &payload_len, reader->data) ;

            if (parsed == 0)
                return 0 ;  // Need more header data

            if (parsed < 0) {
                reader->state = IO_MESSAGE_ERROR ;
                return (errno = EPROTO, -1) ;
            }

            // Header parsed
            if (payload_len > reader->payload_size) {
                reader->state = IO_MESSAGE_ERROR ;
                return (errno = EMSGSIZE, -1) ;
            }

            reader->payload_needed = payload_len ;

            if (payload_len == 0) {
                // Header-only message
                reader->state = IO_MESSAGE_COMPLETE ;
                return 1 ;
            }

            reader->state = IO_MESSAGE_PAYLOAD ;
        }

        // Read payload
        if (reader->state == IO_MESSAGE_PAYLOAD) {
            needed = reader->payload_needed - reader->payload_received ;
            if (needed == 0) {
                reader->state = IO_MESSAGE_COMPLETE ;
                return 1 ;
            }

            available = io_rb_pending_in(reader->io) ;
            if (available == 0)
                return 0 ;  // Need more data

            to_read = (needed < available) ? needed : available ;
            dest = (uint8_t *)reader->payload_buf + reader->payload_received ;

            r = io_rb_get(reader->io, dest, to_read) ;
            if (r <= 0) {
                reader->state = IO_MESSAGE_ERROR ;
                return -1 ;
            }

            reader->payload_received += r ;

            if (reader->payload_received >= reader->payload_needed) {
                reader->state = IO_MESSAGE_COMPLETE ;
                return 1 ;
            }
        }
    }
    return 0 ;
}

void io_message_reset(io_message_t *reader)
{
    if (!reader)
        return ;

    reader->state = IO_MESSAGE_HEADER ;
    reader->header_received = 0 ;
    reader->payload_needed = 0 ;
    reader->payload_received = 0 ;
}

int io_message_get_buffers(const io_message_t *reader, void **header, size_t *header_len, void **payload, size_t *payload_len)
{
    if (!reader || reader->state != IO_MESSAGE_COMPLETE)
        return (errno = EINVAL, 0) ;

    if (header) *header = reader->header_buf ;
    if (header_len) *header_len = reader->header_received ;
    if (payload) *payload = reader->payload_buf ;
    if (payload_len) *payload_len = reader->payload_received ;

    return 1 ;
}


