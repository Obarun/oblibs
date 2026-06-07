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
 *
 * @brief Message-oriented I/O abstraction using ring buffer transport.
 *
 * This module provides a state-machine-based reader for parsing structured messages
 * from a ring buffer (io_rb_t), where each message consists of a header (fixed or
 * variable length) followed by an optional payload. The implementation is transport-
 * agnostic and works with any io_rb_t backend (pipes, sockets, files, etc.).
 *
 * The protocol is defined by a callback function that parses the header to determine
 * the expected payload size. This allows for flexible message formats including
 * length-prefixed protocols, type-length-value (TLV) structures, or custom binary formats.
 *
 * Key features:
 * - Non-blocking incremental message parsing
 * - External buffer management (no internal allocations)
 * - Support for header-only messages
 * - Automatic state management through message lifecycle
 * - Integration with io_rb_t ring buffer infrastructure
 */


#ifndef OB_IO_MESSAGE_H
#define OB_IO_MESSAGE_H

#include <sys/types.h>

#include <oblibs/io_rb.h>

enum io_message_state_e
{
    IO_MESSAGE_HEADER,      // Reading header
    IO_MESSAGE_PAYLOAD,     // Reading payload
    IO_MESSAGE_COMPLETE,    // Message ready
    IO_MESSAGE_ERROR        // Error state
} ;
typedef enum io_message_state_e io_message_state_t ;

/**
 * @brief Message reader - operates on io_rb buffers
 *
 * This handles the mechanics of reading a fixed-size header followed
 * by a variable-size payload. It doesn't interpret the content.
 *
 * The caller is responsible for ensuring buffer lifetimes exceed the reader's usage.
 */
struct io_message_s
{
    io_rb_t *io ;                // I/O buffer to read from
    io_message_state_t state ;   // Current state

    void *header_buf ;           // External header buffer (caller-managed)
    size_t header_size ;         // Header size in bytes
    void *payload_buf ;          // External payload buffer (caller-managed)
    size_t payload_size ;        // Payload buffer capacity

    size_t header_received ;     // Bytes of header received
    size_t payload_needed ;      // Bytes of payload expected (from header parsing)
    size_t payload_received ;    // Bytes of payload received

    void *data ;                 // User data context
} ;
typedef struct io_message_s io_message_t ;

/**
 * @brief Callback to determine payload size from header
 *
 * This callback is invoked when sufficient header bytes have been received to attempt parsing.
 * It should validate the header structure and extract the expected payload length.
 *
 * @param header Pointer to the header buffer containing received bytes
 * @param header_len Number of valid bytes currently in the header buffer
 * @param[out] payload_len Set to the expected payload size (0 for header-only messages)
 * @param data User-provided context data (passed through from io_message_read())
 *
 * @return Status code indicating parsing result:
 *      - >0: Number of header bytes consumed (should match actual header size)
 *      - 0: Need more header data (incomplete but valid so far)
 *      - -1: Protocol error (malformed header, invalid values, etc.)
 *
 * @note The callback must handle partial headers gracefully and only return >0 when
 *       a complete, valid header has been parsed.
 */
typedef int (io_message_header_parser_t)(const void *header, size_t header_len, size_t *payload_len, void *data) ;

/**
 * @brief Initialize a message reader instance
 *
 * Prepares the reader for message parsing by associating it with an I/O buffer
 * and external storage buffers. The reader starts in IO_MESSAGE_HEADER state.
 *
 * @param[out] reader Pointer to reader structure to initialize
 * @param[in] io I/O ring buffer to read from (must be valid and connected)
 * @param[in] header_buf External buffer to store message headers (must be at least header_size bytes)
 * @param[in] header_size Size of the header buffer in bytes (must be > 0)
 * @param[in] payload_buf External buffer to store message payloads (must be at least payload_size bytes)
 * @param[in] payload_size Size of the payload buffer in bytes (must be > 0)
 * @param[in] data User data context
 *
 * @return Status code:
 *      - 1 on success
 *      - 0 on failure; errno is set to indicate the error (e.g., EINVAL for invalid parameters)
 *
 * @retval 0 errno is set to EINVAL if any required parameter is NULL or buffer sizes are zero
 *
 * @note The caller must ensure that header_buf and payload_buf remain valid for the
 *       entire lifetime of the reader. The reader does not take ownership of these buffers.
 */
int io_message_reader_init(io_message_t *reader, io_rb_t *io, void *header_buf, size_t header_size, void *payload_buf, size_t payload_size, void *data) ;

/**
 * @brief Read and parse one message from the input buffer
 *
 * Processes available data in the I/O buffer according to the current state machine.
 * This function should be called whenever new data arrives in the underlying io_rb_t.
 * It processes as much data as possible in a non-blocking manner.
 *
 * @param[in,out] reader Initialized message reader instance
 * @param[in] parse_header Callback function to parse headers and determine payload size
 * @param[in] data User context passed to the parse_header callback
 *
 * @return Status code:
 *      - 1 if a complete message was assembled (state is IO_MESSAGE_COMPLETE)
 *      - 0 if more data is needed to complete the current message
 *      - -1 on error; errno is set (e.g., EPROTO for protocol errors, EMSGSIZE for oversized payloads)
 *
 * @retval 1 Message is complete and ready for processing via io_message_get_buffers()
 * @retval 0 Caller should wait for more data and call again when available
 * @retval -1 Fatal error occurred; reader is in IO_MESSAGE_ERROR state
 *
 * @note After a successful return (1), the caller must process the message and then call
 *       io_message_reset() before attempting to read the next message.
 * @note The parse_header callback must be valid and handle partial headers correctly.
 */
int io_message_read(io_message_t *reader, io_message_header_parser_t *parse_header) ;

/**
 * @brief Reset reader state for processing the next message
 *
 * Clears all internal state counters and returns the reader to IO_MESSAGE_HEADER state.
 * This must be called after successfully processing a complete message (when state is
 * IO_MESSAGE_COMPLETE) before attempting to read the next message.
 *
 * @param[in,out] reader Message reader to reset
 *
 * @note This function does not modify the external header_buf or payload_buf contents.
 *       The caller is responsible for processing the message data before calling reset.
 */
void io_message_reset(io_message_t *reader) ;

/**
 * @brief Get pointers to the complete message buffers
 *
 * Retrieves pointers and lengths for the assembled message when in IO_MESSAGE_COMPLETE state.
 * This provides safe access to the message components without exposing internal reader state.
 *
 * @param[in] reader Initialized message reader (must be in IO_MESSAGE_COMPLETE state)
 * @param[out] header Optional pointer to receive header buffer address (may be NULL)
 * @param[out] header_len Optional pointer to receive header length in bytes (may be NULL)
 * @param[out] payload Optional pointer to receive payload buffer address (may be NULL)
 * @param[out] payload_len Optional pointer to receive payload length in bytes (may be NULL)
 *
 * @return Status code:
 *      - 1 on success
 *      - 0 on failure; errno is set to EINVAL if reader is not in complete state
 *
 * @retval 0 errno is set to EINVAL if reader state is not IO_MESSAGE_COMPLETE
 *
 * @note The returned pointers reference the external buffers provided during initialization.
 *       The caller must not modify these buffers while the reader is active.
 */
int io_message_get_buffers(const io_message_t *reader, void **header, size_t *header_len, void **payload, size_t *payload_len) ;

#endif