/*
 * System includes
 */
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>

/*
 * Local includes
 */
#include "logging.h"
#include "stor.h"
#include "cksm.h"
#include "pio.h"

globus_result_t
stor_can_change_cos(char *Pathname, int *can_change_cos)
{
    int                  retval;
    hpss_fileattr_t      fileattr;
    ns_FilesetAttrBits_t fileset_attr_bits;
    ns_FilesetAttrs_t    fileset_attr;

    memset(&fileattr, 0, sizeof(hpss_fileattr_t));
    retval = Hpss_FileGetAttributes(Pathname, &fileattr);
    if (retval)
        return hpss_error_to_globus_result(retval);

    fileset_attr_bits = orbit64m(0, NS_FS_ATTRINDEX_COS);
    memset(&fileset_attr, 0, sizeof(ns_FilesetAttrs_t));
    retval = Hpss_FilesetGetAttributes(NULL,
                                       &fileattr.Attrs.FilesetId,
                                       NULL,
                                       NULL,
                                       fileset_attr_bits,
                                       &fileset_attr);
    if (retval)
        return hpss_error_to_globus_result(retval);

    *can_change_cos = !fileset_attr.ClassOfService;
    return GLOBUS_SUCCESS;
}

globus_result_t
stor_open_for_writing(char *        Pathname,
                      globus_off_t  AllocSize,
                      globus_bool_t Truncate,
                      int *         FileFD,
                      int *         FileStripeWidth)
{
    int                   oflags         = 0;
    int                   retval         = 0;
    int                   can_change_cos = 0;
    globus_off_t          file_length    = 0;
    globus_result_t       result         = GLOBUS_SUCCESS;
    hpss_cos_hints_t      hints_in;
    hpss_cos_hints_t      hints_out;
    hpss_cos_priorities_t priorities;

    *FileFD = -1;

    /* Initialize the hints in. */
    memset(&hints_in, 0, sizeof(hpss_cos_hints_t));

    /* Initialize the hints out. */
    memset(&hints_out, 0, sizeof(hpss_cos_hints_t));

    /* Initialize the priorities. */
    memset(&priorities, 0, sizeof(hpss_cos_priorities_t));

    /*
     * If this is a new file we need to determine the class of service
     * by either:
     *  1) set explicitly within the session handle or
     *  2) determined by the size of the incoming file
     */
    if (Truncate == GLOBUS_TRUE)
    {
        if (AllocSize != 0)
        {
            /*
             * Use the ALLO size.
             */
            file_length = AllocSize;
            CONVERT_LONGLONG_TO_U64(file_length, hints_in.MinFileSize);
            CONVERT_LONGLONG_TO_U64(file_length, hints_in.MaxFileSize);
            priorities.MinFileSizePriority = REQUIRED_PRIORITY;
            /*
             * If MaxFileSizePriority is required, you can not place
             *  a file into a COS where it's max size is < the size
             *  of this file regardless of whether or not enforce max
             *  is enabled on the COS (it doesn't even try).
             */
            priorities.MaxFileSizePriority = HIGHLY_DESIRED_PRIORITY;
        }
    }

    /* Always use O_CREAT in support of S3 transfers. */
    oflags = O_WRONLY | O_CREAT;
    if (Truncate == GLOBUS_TRUE)
        oflags |= O_TRUNC;

    /* Open the HPSS file. */
    *FileFD = Hpss_Open(Pathname,
                        oflags,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
                        &hints_in,
                        &priorities,
                        &hints_out);
    if (*FileFD < 0)
    {
        result = hpss_error_to_globus_result(*FileFD);
        goto cleanup;
    }

    result = stor_can_change_cos(Pathname, &can_change_cos);
    if (result != GLOBUS_SUCCESS)
        goto cleanup;

    /* Handle the case of the file that already existed. */
    if (Truncate == GLOBUS_TRUE && can_change_cos)
    {
        hpss_cos_md_t cos_md;

        retval = Hpss_SetCOSByHints(*FileFD, 0, &hints_in, &priorities, &cos_md);

        if (retval)
        {
            result = hpss_error_to_globus_result(retval);
            goto cleanup;
        }
    }

    /* Copy out the file stripe width. */
    *FileStripeWidth = hints_out.StripeWidth;

cleanup:
    if (result)
    {
        if (*FileFD >= 0)
            Hpss_Close(*FileFD);
        *FileFD = -1;
    }

    return result;
}

void
stor_gridftp_callback(globus_gfs_operation_t Operation,
                      globus_result_t        Result,
                      globus_byte_t        * Buffer,
                      globus_size_t          Length,
                      globus_off_t           Offset,
                      globus_bool_t          Eof,
                      void                 * UserArg)
{
    stor_buffer_t *stor_buffer = UserArg;
    stor_info_t *  stor_info   = stor_buffer->StorInfo;

    if (stor_buffer->Valid != VALID_TAG)
    {
        TRACE("Gridftp read callback with invalid tag.");
        return;
    }

    TRACE("Gridftp read callback. Result=%u, Length=%u, Offset=%lu, Eof=%d", Result, Length, Offset, Eof);

    // Make sure we have the right buffer / UserArg combo
    assert(stor_buffer->Buffer == (char *)Buffer);

    pthread_mutex_lock(&stor_info->Mutex);
    {
        /* Save EOF */
        if (Eof)
            stor_info->Eof = Eof;
        /* Save any error */
        if (Result && !stor_info->Result)
            stor_info->Result = Result;

        assert(Length <= stor_info->BlockSize);

        /* Set buffer counters. */
        stor_buffer->BufferOffset   = 0;
        stor_buffer->TransferOffset = Offset;
        stor_buffer->BufferLength   = Length;

        /* Stor the buffer. */
        if (Length)
            globus_list_insert(&stor_info->ReadyBufferList, stor_buffer);
        else
            globus_list_insert(&stor_info->FreeBufferList, stor_buffer);

        /* Decrease the current connection count. */
        stor_info->CurConnCnt--;

        /* Wake the PIO thread */
        pthread_cond_signal(&stor_info->Cond);
    }
    pthread_mutex_unlock(&stor_info->Mutex);
}

/* 1 = found, 0 = not found */
int
stor_find_buffer(void *Datum, void *Arg)
{
    if (((stor_buffer_t *)Datum)->TransferOffset == *((uint64_t *)Arg))
        return 1;

    return 0;
}

/* Called locked. */
uint64_t
stor_copy_out_buffers(stor_info_t *StorInfo,
                      void *       Buffer,
                      uint64_t     Offset,
                      uint64_t     Length)
{
    globus_list_t *buf_entry      = NULL;
    stor_buffer_t *stor_buffer    = NULL;
    uint64_t       offset_needed  = 0;
    uint64_t       copied_length  = 0;
    uint64_t       length_to_copy = 0;

    do
    {
        offset_needed = Offset + copied_length;

        /* Look for a buffer containing this offset. */
        buf_entry = globus_list_search_pred(
            StorInfo->ReadyBufferList, stor_find_buffer, &offset_needed);

        if (buf_entry)
        {
            stor_buffer = globus_list_first(buf_entry);

            /* Set length to copy to size of our GridFTP buffer. */
            length_to_copy = stor_buffer->BufferLength;

            /* Limit to length that DS3 is asking for. */
            if (length_to_copy > (Length - copied_length))
                length_to_copy = (Length - copied_length);

            memcpy(Buffer + copied_length,
                   stor_buffer->Buffer + stor_buffer->BufferOffset,
                   length_to_copy);

            /* Update buffer counters. */
            stor_buffer->BufferOffset += length_to_copy;
            stor_buffer->TransferOffset += length_to_copy;
            stor_buffer->BufferLength -= length_to_copy;
            copied_length += length_to_copy;

            /* If empty, move it to free. */
            if (stor_buffer->BufferLength == 0)
            {
                globus_list_remove(&StorInfo->ReadyBufferList, buf_entry);
                globus_list_insert(&StorInfo->FreeBufferList, stor_buffer);
            }
        }
    } while (copied_length != Length && buf_entry);
    return copied_length;
}

/* Called locked. */
globus_result_t
stor_launch_gridftp_reads(stor_info_t *StorInfo)
{
    stor_buffer_t * stor_buffer = NULL;
    globus_result_t result      = GLOBUS_SUCCESS;

    if (StorInfo->Eof)
        return GLOBUS_SUCCESS;

    if (StorInfo->ConnChkCnt++ == 0)
        globus_gridftp_server_get_optimal_concurrency(StorInfo->Operation,
                                                      &StorInfo->OptConnCnt);
    if (StorInfo->ConnChkCnt >= 100)
        StorInfo->ConnChkCnt = 0;

    // This code assumes the buffers are coming in in order.
    while (StorInfo->CurConnCnt < StorInfo->OptConnCnt)
    {
        if (!globus_list_empty(StorInfo->FreeBufferList))
        {
            /* Grab a buffer from the free list. */
            stor_buffer = globus_list_remove(&StorInfo->FreeBufferList,
                                             StorInfo->FreeBufferList);
        } else if (globus_list_size(StorInfo->AllBufferList) >=
                   StorInfo->OptConnCnt)
        {
            break;
        } else
        {
            /* Allocate a new buffer. */
            stor_buffer = globus_malloc(sizeof(stor_buffer_t));
            if (!stor_buffer)
            {
                result = GlobusGFSErrorMemory("stor_buffer_t");
                break;
            }
            stor_buffer->Buffer = globus_malloc(StorInfo->BlockSize);
            if (!stor_buffer->Buffer)
            {
                free(stor_buffer);
                result = GlobusGFSErrorMemory("stor_buffer_t");
                break;
            }
            stor_buffer->StorInfo = StorInfo;
            stor_buffer->Valid    = VALID_TAG;
            globus_list_insert(&StorInfo->AllBufferList, stor_buffer);
        }

        TRACE("Register gridftp read with %d outstanding.", StorInfo->CurConnCnt);
        result = globus_gridftp_server_register_read(
            StorInfo->Operation,
            (globus_byte_t *)stor_buffer->Buffer,
            StorInfo->BlockSize,
            stor_gridftp_callback,
            stor_buffer);

        if (result)
        {
            TRACE("Register gridftp read failed.");
            break;
        }

        /* Increase the current connection count. */
        StorInfo->CurConnCnt++;
    }

    return result;
}

int
stor_pio_callout(char     * Buffer,
                 uint32_t * Length,
                 uint64_t   Offset,
                 void     * CallbackArg)
{
    uint64_t        offset_needed = 0;
    uint64_t        copied_length = 0;
    stor_info_t *   stor_info     = CallbackArg;
    globus_result_t result        = GLOBUS_SUCCESS;

    TRACE("PIO stor callout: Length:%u, Offset:%lu", *Length, Offset);

    pthread_mutex_lock(&stor_info->Mutex);
    {
        while (copied_length != *Length && !stor_info->Result)
        {
            offset_needed = Offset + copied_length;

            copied_length += stor_copy_out_buffers(stor_info,
                                                   Buffer + copied_length,
                                                   offset_needed,
                                                   *Length - copied_length);

            if (stor_info->Eof && stor_info->CurConnCnt == 0)
            {
                if (copied_length != *Length &&
                    (copied_length + offset_needed) !=
                        stor_info->TransferInfo->alloc_size)
                    result =
                        GlobusGFSErrorGeneric("Premature end of data transfer");
                break;
            }

            if ((result = stor_launch_gridftp_reads(stor_info)))
                break;

            if (copied_length != *Length)
                pthread_cond_wait(&stor_info->Cond, &stor_info->Mutex);
        }

        if (copied_length)
            globus_gridftp_server_update_bytes_recvd(stor_info->Operation,
                                                     copied_length);

        // If no other error has occurred, store our error
        if (stor_info->Result == GLOBUS_SUCCESS)
            stor_info->Result = result;
        // If we didn't have an error, copy out any other error
        if (result == GLOBUS_SUCCESS)
            result = stor_info->Result;
    }
    pthread_mutex_unlock(&stor_info->Mutex);

    int exit_code = !(result == GLOBUS_SUCCESS);
    TRACE("PIO stor callout: exit_code:%d", exit_code);
// XXX copied_length == *Length if exit_code == 0 (ALWAYS)
    return exit_code;
}

void
stor_wait_for_gridftp(stor_info_t *StorInfo)
{
    pthread_mutex_lock(&StorInfo->Mutex);
    {
        while (1)
        {
            if (StorInfo->Result)
                break;

            if (globus_list_size(StorInfo->AllBufferList) ==
                globus_list_size(StorInfo->FreeBufferList))
                break;

            pthread_cond_wait(&StorInfo->Cond, &StorInfo->Mutex);
        }
    }
    pthread_mutex_unlock(&StorInfo->Mutex);
}

void
stor_range_complete_callback(globus_off_t *Offset,
                             globus_off_t *Length,
                             int *         Eot,
                             void *        UserArg)
{
    stor_info_t *stor_info = UserArg;

    DEBUG("Restart marker sent: %lld, %lld", *Offset, *Length);
    globus_gridftp_server_update_range_recvd(stor_info->Operation,
                                             *Offset, 
                                             *Length);

    assert(*Length <= stor_info->RangeLength);

    stor_info->RangeLength -= *Length;
    *Offset += *Length;
    *Length = stor_info->RangeLength;

    *Eot = 0;
    if (stor_info->RangeLength == 0)
    {
        globus_gridftp_server_get_write_range(
            stor_info->Operation, Offset, Length);
        if (*Length == -1)
            *Eot = 1;
        stor_info->RangeLength = *Length;
    }
}

static int
release_buffer(void *Datum, void *Arg)
{
    ((stor_buffer_t *)Datum)->Valid = INVALID_TAG;
    free(((stor_buffer_t *)Datum)->Buffer);

    return 0;
}

void
stor_transfer_complete_callback(globus_result_t Result, void *UserArg)
{
    globus_result_t result    = Result;
    stor_info_t *   stor_info = UserArg;
    int             rc        = 0;

    // issue #37: insist on reading EOF before calling finished
    if (result == GLOBUS_SUCCESS)
    {
        pthread_mutex_lock(&stor_info->Mutex);
        {
            while (!result && !stor_info->Result && !stor_info->Eof)
            {
                result = stor_launch_gridftp_reads(stor_info);
                if (result)
                    break;
                pthread_cond_wait(&stor_info->Cond, &stor_info->Mutex);
            }
        }
        pthread_mutex_unlock(&stor_info->Mutex);
    }

    /* Prefer our error over PIO's. */
    if (stor_info->Result)
        result = stor_info->Result;

    rc = Hpss_Close(stor_info->FileFD);
    if (rc && !result)
        result = hpss_error_to_globus_result(rc);

    globus_gridftp_server_finished_transfer(stor_info->Operation, result);

    /*
     * From this point on, if the server is shutting down, we have no guarantee
     * that the process will exist long enough to complete this function. Therefore
     * we get Hpss_Close() out of the way (above) and below we leave non consequential
     * cleanup that only needs to be completed if the server is going to perform another
     * transfer.
     */
    stor_wait_for_gridftp(stor_info);

    pthread_mutex_destroy(&stor_info->Mutex);
    pthread_cond_destroy(&stor_info->Cond);
    globus_list_free(stor_info->FreeBufferList);
    globus_list_free(stor_info->ReadyBufferList);

    globus_list_search_pred(stor_info->AllBufferList, release_buffer, NULL);
    globus_list_destroy_all(stor_info->AllBufferList, free);
    free(stor_info);
}

static bool
this_is_a_restart(globus_off_t Offset)
{
    return (Offset != 0);
}

static globus_result_t
validate_restart(const char * Pathname,
                 globus_off_t Offset,
                 globus_off_t Length)
{
    hpss_stat_t hpss_stat_buf;
    int retval = Hpss_Lstat((char *)Pathname, &hpss_stat_buf);
    if (retval)
        return hpss_error_to_globus_result(retval);

    if (hpss_stat_buf.st_size < Offset)
        return globus_error_put(GlobusGFSErrorObjAppendNotSupported(GlobusGFSErrorObjGeneric("Bad restart marker found.")));

    return GLOBUS_SUCCESS;
}

void
stor(globus_gfs_operation_t      Operation,
     globus_gfs_transfer_info_t *TransferInfo,
     bool                        UseUDAChecksums)
{
    stor_info_t *   stor_info         = NULL;
    globus_result_t result            = GLOBUS_SUCCESS;
    int             file_stripe_width = 0;
    globus_off_t    offset            = 0;

    /*
     * Create our structure.
     */
    stor_info = malloc(sizeof(stor_info_t));
    if (!stor_info)
    {
        result = GlobusGFSErrorMemory("stor_info_t");
        goto cleanup;
    }
    memset(stor_info, 0, sizeof(stor_info_t));
    stor_info->Operation    = Operation;
    stor_info->TransferInfo = TransferInfo;
    stor_info->FileFD       = -1;
    pthread_mutex_init(&stor_info->Mutex, NULL);
    pthread_cond_init(&stor_info->Cond, NULL);

    globus_gridftp_server_get_block_size(Operation, &stor_info->BlockSize);

    /*
     * Clearing a UDA checksum is not as innocuous as it sounds. It actually
     * sets UDA values that indicate that the checksum is invalid. Setting
     * _any_ value in UDA when the endpoint admin has disabled UDA would
     * seem to violate that configuration option. So we check if we should
     * be using UDA at all before clearing the stored value.
     */
    if (UseUDAChecksums)
    {
        result = cksm_clear_uda_checksum(TransferInfo->pathname);
        if (result)
            goto cleanup;
    }

    /*
     * Open the file.
     */
    result = stor_open_for_writing(TransferInfo->pathname,
                                   TransferInfo->alloc_size,
                                   TransferInfo->truncate,
                                   &stor_info->FileFD,
                                   &file_stripe_width);
    if (result)
        goto cleanup;

    // Write_range() must occur before begin_transfer() so that
    // offsets are correct for DSIs that require ordered offsets
    globus_gridftp_server_get_write_range(
        Operation, &offset, &stor_info->RangeLength);

    globus_gridftp_server_begin_transfer(Operation, 0, NULL);

    INFO("Receiving %s: Offset:%lld  Length:%lld",
           TransferInfo->pathname,
           offset, 
           TransferInfo->alloc_size);

    if (stor_info->RangeLength == -1)
        stor_info->RangeLength = TransferInfo->alloc_size;

    if (this_is_a_restart(offset))
    {
        globus_off_t length = stor_info->RangeLength;
        result = validate_restart(TransferInfo->pathname, offset, length);
        if (result)
            goto cleanup;
    }

    /*
     * Setup PIO
     */
    result = pio_start(HPSS_PIO_WRITE,
                       stor_info->FileFD,
                       file_stripe_width,
                       stor_info->BlockSize,
                       offset,
                       stor_info->RangeLength,
                       stor_pio_callout,
                       stor_range_complete_callback,
                       stor_transfer_complete_callback,
                       stor_info);

cleanup:
    if (result)
    {
        globus_gridftp_server_finished_transfer(Operation, result);
        if (stor_info)
        {
            if (stor_info->FileFD >= 0)
                Hpss_Close(stor_info->FileFD);
            pthread_mutex_destroy(&stor_info->Mutex);
            pthread_cond_destroy(&stor_info->Cond);
            free(stor_info);
        }
    }
}
