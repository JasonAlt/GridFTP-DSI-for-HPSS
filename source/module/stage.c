/*
 * System includes
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

/*
 * Local includes
 */
#include "logging.h"
#include "stage.h"
#include "hpss.h"

#if (HPSS_MAJOR_VERSION == 7 && HPSS_MINOR_VERSION > 4) ||                     \
    HPSS_MAJOR_VERSION >= 8
#define bitfile_id_t bfs_bitfile_obj_handle_t
#define ATTR_TO_BFID(x) (x.Attrs.BitfileObj.BfId)
#else
#define bitfile_id_t hpssoid_t
#define ATTR_TO_BFID(x) (x.Attrs.BitfileId)
#endif

typedef enum
{
    ARCHIVED,
    RESIDENT,
    TAPE_ONLY,
} residency_t;

/*
 * Use a constant request number for stage requests so that we can
 * query the stage request status between processes.
 */
#if HPSS_MAJOR_VERSION >= 8
static hpss_reqid_t REQUEST_ID = {0xdeadbeef, 0xdead, 0xbeef, 0xde, 0xad, {0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef}};
#else
static hpss_reqid_t REQUEST_ID = 0xDEADBEEF;
#endif

static globus_result_t
check_request_status(bitfile_id_t *BitfileID, int *Status)
{
#if (HPSS_MAJOR_VERSION == 7 && HPSS_MINOR_VERSION > 4) ||                     \
    HPSS_MAJOR_VERSION >= 8
    int retval = Hpss_GetAsyncStatus(REQUEST_ID, BitfileID, Status);
#else
    int retval = Hpss_GetAsynchStatus(REQUEST_ID, BitfileID, Status);
#endif

    if (retval)
        return hpss_error_to_globus_result(retval);

    switch (*Status)
    {
    case HPSS_STAGE_STATUS_UNKNOWN:
        WARN("Stage request status UNKNOWN");
        break;
    case HPSS_STAGE_STATUS_ACTIVE:
        DEBUG("Stage request status ACTIVE");
        break;
    case HPSS_STAGE_STATUS_QUEUED:
        DEBUG("Stage request status QUEUED");
        break;
    }

    return GLOBUS_SUCCESS;
}

int
min(size_t x, size_t y)
{
    if (x < y)
        return x;
    return y;
}

static globus_result_t
submit_stage_request(const char *Pathname)
{
    hpss_fileattr_t fattrs;
    int retval = Hpss_FileGetAttributes((char *)Pathname, &fattrs);
    if (retval)
        return hpss_error_to_globus_result(retval);

    bfs_callback_addr_t callback_addr;
    memset(&callback_addr, 0, sizeof(callback_addr));

    const char *callback_addr_str = getenv("ASYNC_CALLBACK_ADDR");
    if (callback_addr_str)
    {
        char node[NI_MAXHOST];
        char serv[NI_MAXSERV];

        memset(node, 0, sizeof(node));
        memset(serv, 0, sizeof(serv));

        char *colon;
        if ((colon = strchr(callback_addr_str, ':')))
        {
            strncpy(node,
                    callback_addr_str,
                    min(sizeof(node) - 1, colon - callback_addr_str));
            strncpy(serv, colon + 1, sizeof(serv));
        } else
        {
            strncpy(node, callback_addr_str, sizeof(node) - 1);
        }

        char errbuf[HPSS_NET_MAXBUF];
        retval = Hpss_net_getaddrinfo(node,
                                      serv,
                                      0,
                                      HPSS_IPPROTO_TCP,
                                      &callback_addr.sockaddr,
                                      errbuf,
                                      sizeof(errbuf));

        if (retval)
        {
            ERROR("Failed to set stage callback address %s: %d (%s) - %s",
                   callback_addr_str,
                   retval,
                   gai_strerror(hpss_error_status(retval)),
                   errbuf);
            return hpss_error_to_globus_result(retval);
        }
    }

    callback_addr.id = REQUEST_ID;

    DEBUG("Requesting stage for %s", Pathname);

    /*
     * We use hpss_StageCallBack() so that we do not block while the
     * stage completes. We could use hpss_Open(O_NONBLOCK) and then
     * hpss_Stage(BFS_ASYNCH_CALL) but then we block in hpss_Close().
     */
    bitfile_id_t bitfile_id;
    retval = Hpss_StageCallBack((char *)Pathname,
                                cast64m(0),
                                fattrs.Attrs.DataLength,
                                0,
                                &callback_addr,
                                BFS_STAGE_ALL,
                                &REQUEST_ID,
                                &bitfile_id);
    if (retval)
        return hpss_error_to_globus_result(retval);

    return GLOBUS_SUCCESS;
}

static residency_t
check_xattr_residency(hpss_xfileattr_t *XFileAttr)
{
    /* Check if this is a regular file. */
    if (XFileAttr->Attrs.Type != NS_OBJECT_TYPE_FILE &&
        XFileAttr->Attrs.Type != NS_OBJECT_TYPE_HARD_LINK)
    {
        return RESIDENT;
    }

    /* Check if the top level is tape. */
    if (XFileAttr->SCAttrib[0].Flags & BFS_BFATTRS_LEVEL_IS_TAPE)
    {
        return TAPE_ONLY;
    }

    /* Handle zero length files. */
    if (eqz64m(XFileAttr->Attrs.DataLength))
    {
        return RESIDENT;
    }

    /*
     * Determine the archive status. Due to holes, we can not expect
     * XFileAttr->Attrs.DataLength bytes on disk. And we really don't know
     * how much data is really in this file. So the algorithm assumes the
     * file is staged unless it finds a tape SC that has more BytesAtLevel
     * than the disk SCs before it.
     */
    int      level     = 0;
    uint64_t max_bytes = 0;

    for (level = 0; level < HPSS_MAX_STORAGE_LEVELS; level++)
    {
        if (XFileAttr->SCAttrib[level].Flags & BFS_BFATTRS_LEVEL_IS_DISK)
        {
            /* Save the largest count of bytes on disk. */
            if (gt64(XFileAttr->SCAttrib[level].BytesAtLevel, max_bytes))
                max_bytes = XFileAttr->SCAttrib[level].BytesAtLevel;
        } else if (XFileAttr->SCAttrib[level].Flags & BFS_BFATTRS_LEVEL_IS_TAPE)
        {
            /* File is purged if more bytes are on tape. */
            if (gt64(XFileAttr->SCAttrib[level].BytesAtLevel, max_bytes))
                return ARCHIVED;
        }
    }

    return RESIDENT;
}

static void
free_xfileattr(hpss_xfileattr_t *XFileAttr)
{
    int level    = 0;
    int vv_index = 0;

    /* Free the extended information. */
    for (level = 0; level < HPSS_MAX_STORAGE_LEVELS; level++)
    {
        for (vv_index = 0; vv_index < XFileAttr->SCAttrib[level].NumberOfVVs;
             vv_index++)
        {
            if (XFileAttr->SCAttrib[level].VVAttrib[vv_index].PVList != NULL)
            {
                free(XFileAttr->SCAttrib[level].VVAttrib[vv_index].PVList);
            }
        }
    }
}

static globus_result_t
check_file_residency(const char *Pathname, residency_t *Residency)
{
    int              retval = 0;
    hpss_xfileattr_t xattr;

    memset(&xattr, 0, sizeof(hpss_xfileattr_t));

    /*
     * Stat the object. Without API_GET_XATTRS_NO_BLOCK, this call would hang
     * on any file moving between levels in its hierarchy (ie staging).
     */
    retval = Hpss_FileGetXAttributes((char *)Pathname,
                                     API_GET_STATS_FOR_ALL_LEVELS |
                                         API_GET_XATTRS_NO_BLOCK,
                                     0,
                                     &xattr);

    if (retval)
        return hpss_error_to_globus_result(retval);

    *Residency = check_xattr_residency(&xattr);

    /* Release the hpss_xfileattr_t */
    free_xfileattr(&xattr);

    switch (*Residency)
    {
    case ARCHIVED:
        DEBUG("File is ARCHIVED: %s", Pathname);
        break;
    case RESIDENT:
        DEBUG("File is RESIDENT: %s", Pathname);
        break;
    case TAPE_ONLY:
        DEBUG("File is TAPE_ONLY: %s", Pathname);
        break;
    }

    return GLOBUS_SUCCESS;
}

static globus_result_t
stage_get_timeout(globus_gfs_operation_t     Operation,
                  globus_gfs_command_info_t *CommandInfo,
                  int *                      Timeout)
{
    globus_result_t result;
    char **         argv   = NULL;
    int             argc   = 0;
    int             retval = 0;

    /* Get the command arguments. */
    result = globus_gridftp_server_query_op_info(Operation,
                                                 CommandInfo->op_info,
                                                 GLOBUS_GFS_OP_INFO_CMD_ARGS,
                                                 &argv,
                                                 &argc);

    if (result)
        return GlobusGFSErrorWrapFailed("Unable to get command args", result);

    /* Convert the timeout. */
    retval = sscanf(argv[2], "%d", Timeout);
    if (retval != 1)
        return GlobusGFSErrorGeneric("Illegal timeout value");

    return GLOBUS_SUCCESS;
}

static globus_result_t
get_bitfile_id(const char *Pathname, bitfile_id_t *bitfile_id)
{
    hpss_fileattr_t attrs;
    int retval = Hpss_FileGetAttributes((char *)Pathname, &attrs);
    if (retval)
        return hpss_error_to_globus_result(retval);

    memcpy(bitfile_id, &ATTR_TO_BFID(attrs), sizeof(bitfile_id_t));
    return GLOBUS_SUCCESS;
}

static char *
generate_output(const char *Pathname, residency_t Residency)
{
    if (Residency == RESIDENT)
        return globus_common_create_string(
            "250 Stage of file %s succeeded.\r\n", Pathname);

    if (Residency == TAPE_ONLY)
        return globus_common_create_string(
            "250 %s is on a tape only class of service.\r\n", Pathname);

    return globus_common_create_string(
        "450 %s: is being retrieved from the archive...\r\n", Pathname);
}

/* Returns 1 if Timeout has elapsed, 0 otherwise. */
static int
pause_1_second(time_t StartTime, int Timeout)
{
    if ((time(NULL) - StartTime) > Timeout)
        return 1;

    // Sleep for 1.0 seconds
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    select(0, NULL, NULL, NULL, &tv);

    return 0;
}

static globus_result_t
stage_internal(const char *Path, int Timeout, residency_t *Residency)
{
    int             time_elapsed = 0;
    time_t          start_time   = time(NULL);
    bitfile_id_t    bitfile_id;
    globus_result_t result;

    *Residency = ARCHIVED;

    result = get_bitfile_id(Path, &bitfile_id);
    if (result)
        goto cleanup;

    while (*Residency == ARCHIVED && !time_elapsed)
    {
        result = check_file_residency(Path, Residency);
        if (result)
            goto cleanup;
        if (*Residency != ARCHIVED)
            break;

        int status;
        result = check_request_status(&bitfile_id, &status);
        if (result)
            goto cleanup;

        if (status == HPSS_STAGE_STATUS_UNKNOWN)
        {
            result = submit_stage_request(Path);
            if (result)
                goto cleanup;
        }

        time_elapsed = pause_1_second(start_time, Timeout);
    }

cleanup:
    return result;
}

void
stage(globus_gfs_operation_t     Operation,
      globus_gfs_command_info_t *CommandInfo,
      commands_callback          Callback)
{
    int             timeout;
    char *          command_output = NULL;
    residency_t     residency      = ARCHIVED;
    globus_result_t result;

    result = stage_get_timeout(Operation, CommandInfo, &timeout);
    if (result)
        goto cleanup;

    result = stage_internal(CommandInfo->pathname, timeout, &residency);
    if (result != GLOBUS_SUCCESS)
        goto cleanup;

    command_output = generate_output(CommandInfo->pathname, residency);

cleanup:
    Callback(Operation, result, command_output);
    if (command_output)
        globus_free(command_output);
}
