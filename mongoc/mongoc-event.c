/*
 * Copyright 2013 10gen Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <errno.h>

#include "mongoc-event-private.h"


bson_bool_t
mongoc_event_encode (mongoc_event_t     *event,
                     bson_uint8_t      **buf,
                     size_t             *buflen,
                     bson_realloc_func   realloc_func,
                     bson_error_t       *error)
{
   bson_uint32_t len;
   bson_uint32_t i;
   bson_uint32_t iovcnt;
   bson_uint8_t *dst;
   struct iovec *iov;

   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(buf, FALSE);
   bson_return_val_if_fail(buflen, FALSE);

   if (!realloc_func) {
      realloc_func = bson_realloc;
   }

   switch (event->type) {
   case MONGOC_OPCODE_QUERY:
      MONGOC_EVENT_SCATTER_QUERY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_QUERY(event);
      break;
   case MONGOC_OPCODE_UPDATE:
      MONGOC_EVENT_SCATTER_UPDATE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_UPDATE(event);
      break;
   case MONGOC_OPCODE_INSERT:
      MONGOC_EVENT_SCATTER_INSERT(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_INSERT(event);
      break;
   case MONGOC_OPCODE_GET_MORE:
      MONGOC_EVENT_SCATTER_GET_MORE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_GET_MORE(event);
      break;
   case MONGOC_OPCODE_DELETE:
      MONGOC_EVENT_SCATTER_DELETE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_DELETE(event);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      MONGOC_EVENT_SCATTER_KILL_CURSORS(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_KILL_CURSORS(event);
      break;
   case MONGOC_OPCODE_MSG:
      MONGOC_EVENT_SCATTER_MSG(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_MSG(event);
      break;
   case MONGOC_OPCODE_REPLY:
      MONGOC_EVENT_SCATTER_REPLY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_REPLY(event);
      break;
   default:
      return FALSE;
   }

   len = event->any.len;

   if (len > MONGOC_EVENT_MAX_LEN) {
      bson_set_error(error,
                     1, /* MONGOC_CLIENT_ERROR, */
                     1, /* MONGOC_CLIENT_ERROR_MSG_TOO_LARGE, */
                     "The event length is too large: %u",
                     len);
      return FALSE;
   }

   MONGOC_EVENT_SWAB_HEADER(event);

   if (!*buf || (*buflen < len)) {
      *buf = realloc_func(*buf, len);
   }

   dst = *buf;
   *buflen = len;

   for (i = 0; i < iovcnt; i++) {
      memcpy(dst, iov[i].iov_base, iov[i].iov_len);
      dst += iov[i].iov_len;
   }

   return TRUE;
}


bson_bool_t
mongoc_event_write (mongoc_event_t  *event,
                    mongoc_stream_t *stream,
                    bson_error_t    *error)
{
   bson_uint32_t len;
   struct iovec *iov;
   ssize_t ret;
   int iovcnt;

   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(stream, FALSE);

   switch (event->type) {
   case MONGOC_OPCODE_QUERY:
      MONGOC_EVENT_SCATTER_QUERY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_QUERY(event);
      break;
   case MONGOC_OPCODE_UPDATE:
      MONGOC_EVENT_SCATTER_UPDATE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_UPDATE(event);
      break;
   case MONGOC_OPCODE_INSERT:
      MONGOC_EVENT_SCATTER_INSERT(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_INSERT(event);
      break;
   case MONGOC_OPCODE_GET_MORE:
      MONGOC_EVENT_SCATTER_GET_MORE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_GET_MORE(event);
      break;
   case MONGOC_OPCODE_DELETE:
      MONGOC_EVENT_SCATTER_DELETE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_DELETE(event);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      MONGOC_EVENT_SCATTER_KILL_CURSORS(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_KILL_CURSORS(event);
      break;
   case MONGOC_OPCODE_MSG:
      MONGOC_EVENT_SCATTER_MSG(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_MSG(event);
      break;
   case MONGOC_OPCODE_REPLY:
      MONGOC_EVENT_SCATTER_REPLY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_REPLY(event);
      break;
   default:
      return FALSE;
   }

   len = event->any.len;

   if (len > MONGOC_EVENT_MAX_LEN) {
      bson_set_error(error,
                     1, /* MONGOC_CLIENT_ERROR, */
                     1, /* MONGOC_CLIENT_ERROR_MSG_TOO_LARGE, */
                     "The event length is too large: %u",
                     len);
      return FALSE;
   }

   MONGOC_EVENT_SWAB_HEADER(event);

   errno = 0;

   if (len == (ret = mongoc_stream_writev(stream, iov, iovcnt))) {
      errno = 0;
      if (0 != mongoc_stream_flush(stream)) {
         printf("Failed to flush: %s\n", strerror(errno));
      }
      return TRUE;
   }

   /*
    * TODO: Handle short write or write failure.
    */

   return FALSE;
}


bson_bool_t
mongoc_event_read (mongoc_event_t  *event,
                   mongoc_stream_t *stream,
                   bson_error_t    *error)
{
   bson_uint32_t max_msg_len;
   struct iovec iov;
   ssize_t n;
   size_t toread;

   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(stream, FALSE);
   bson_return_val_if_fail(error, FALSE);

   memset(event, 0, sizeof *event);

   /*
    * Read the event header.
    */
   errno = 0;
   iov.iov_base = &event->any.len;
   iov.iov_len = 16;
   n = mongoc_stream_readv(stream, &iov, 1);
   switch (n) {
   case -1:
   case 0:
      /*
       * TODO: Set error.
       */
      return FALSE;
   case 16:
      break;
   default:
      /*
       * TODO: Handle short read.
       */
      return FALSE;
   }

   MONGOC_EVENT_SWAB_HEADER(event);
   event->any.type = event->any.opcode;

   /*
    * TODO: Plumb this through.
    */
   max_msg_len = 48 * 1024 * 1024;

   /*
    * Make sure message length is not invalid.
    */
   if ((event->any.len <= 16) || (event->any.len > max_msg_len)) {
      return FALSE;
   }

   /*
    * Read in the rest of the network packet.
    */
   toread = event->any.len - 16;
   mongoc_buffer_init(&event->any.rawbuf, NULL, 0, NULL);
again:
   n = mongoc_buffer_fill(&event->any.rawbuf, stream, toread, error);
   switch (n) {
   case -1:
   case 0:
      return FALSE;
   default:
      toread -= n;
      if (toread) {
         goto again;
      }
      break;
   }

   /*
    * TODO: Get a buffer from the pool large enough to contain @n bytes.
    */

   switch (event->any.opcode) {
   case MONGOC_OPCODE_REPLY:
      iov.iov_base = &event->reply.desc;
      iov.iov_len = 20;
      if (mongoc_buffer_readv(&event->any.rawbuf, &iov, 1) != 20) {
         return FALSE;
      }
      bson_reader_init_from_data(&event->reply.docs_reader,
                                 &event->any.rawbuf.data[event->any.rawbuf.off],
                                 event->any.rawbuf.len);
      MONGOC_EVENT_SWAB_REPLY(event);
      break;
   case MONGOC_OPCODE_MSG:
      event->msg.msglen = event->any.len - 17;
      event->msg.msg = (const char *)event->any.rawbuf.data;
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      iov.iov_base = &event->kill_cursors.desc;
      iov.iov_len = 8;
      if (mongoc_buffer_readv(&event->any.rawbuf, &iov, 1) != 8) {
         return FALSE;
      }
      if (event->any.rawbuf.len !=
          ((8 * BSON_UINT64_FROM_LE(event->kill_cursors.desc.n_cursors)))) {
         return FALSE;
      }
      /*
       * This is only safe to do because we know our alignment in the data
       * will actually be pointer aligned and data is itself pointer aligned.
       */
      event->kill_cursors.cursors =
         (bson_uint64_t *)&event->any.rawbuf.data[event->any.rawbuf.off];
      MONGOC_EVENT_SWAB_KILL_CURSORS(event);
      break;
   case MONGOC_OPCODE_QUERY:
      if (!mongoc_buffer_read_typed(&event->any.rawbuf,
                                    MONGOC_BUFFER_INT32, &event->query.flags,
                                    MONGOC_BUFFER_CSTRING, &event->query.ns,
                                    MONGOC_BUFFER_INT32, &event->query.skip,
                                    MONGOC_BUFFER_INT32, &event->query.n_return,
                                    0)) {
         return FALSE;
      }
      event->query.nslen = strlen(event->query.ns);
      bson_reader_init_from_data(&event->query.docs_reader,
                                 &event->any.rawbuf.data[event->any.rawbuf.off],
                                 event->any.rawbuf.len);
      break;
   /*
    * NOTE:
    *
    * Any of the following messages are unsupported at the moment, but are not
    * really difficult to do if anyone is interested in them. As libmongoc gets
    * used in interesting places, these will need to be implemented.
    */
   case MONGOC_OPCODE_DELETE:
   case MONGOC_OPCODE_GET_MORE:
   case MONGOC_OPCODE_INSERT:
   case MONGOC_OPCODE_UPDATE:
   default:
      return FALSE;
   }

   return TRUE;
}