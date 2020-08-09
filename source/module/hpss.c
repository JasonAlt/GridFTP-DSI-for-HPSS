/*
 * Local includes.
 */
#include "hpss.h"

void
HpssAPI_ConvertTimeToPosixTime(
    const hpss_Attrs_t          *  Attrs,
    timestamp_sec_t             *  Atime,
    timestamp_sec_t             *  Mtime,
    timestamp_sec_t             *  Ctime)
{
    API_ConvertTimeToPosixTime(Attrs, Atime, Mtime, Ctime);
}
 
signed32
Hpss_AuthnMechTypeFromString(
   const char              *  AuthnMechString,
   hpss_authn_mech_t       *  AuthnMech)
{
    signed32 return_value;
    return_value = hpss_AuthnMechTypeFromString(AuthnMechString, AuthnMech);
    return return_value;
}

int
Hpss_Chmod(
    const char                  *  Path,
    mode_t                         Mode)
{
    int return_value;
    return_value = hpss_Chmod(Path, Mode);
    return return_value;
}

char*
Hpss_ChompXMLHeader(
    char                        * XML,
    char                        * Header)
{
    char * return_value;
    return_value = hpss_ChompXMLHeader(XML, Header);
    return return_value;
}

int
Hpss_Chown(
    const char                  *  Path,
    uid_t                          Owner,
    gid_t                          Group)
{
    int return_value;
    return_value = hpss_Chown(Path, Owner, Group);
    return return_value;
}

int
Hpss_Close(int Fildes)
{
    int return_value;
    return_value = hpss_Close(Fildes);
    return return_value;
}

int
Hpss_Closedir(int Dirdes)
{
   int return_value;
   return_value = hpss_Closedir(Dirdes);
   return return_value;
}

int
Hpss_FileGetAttributes(
    const char                  *  Path,
    hpss_fileattr_t             *  AttrOut)
{
    int return_value;
    return_value = hpss_FileGetAttributes(Path, AttrOut);
    return return_value;
}

int
Hpss_FileGetXAttributes(
    const char                  *  Path,
    uint32_t                       Flags,
    uint32_t                       StorageLevel,
    hpss_xfileattr_t            *  AttrOut)
{
    int return_value;
    return_value = hpss_FileGetXAttributes(Path, Flags, StorageLevel, AttrOut);
    return return_value;
}

int
Hpss_FilesetGetAttributes(
    const char                  *  Name,
    const uint64_t              *  FilesetId,
    const ns_ObjHandle_t        *  FilesetHandle,
    const hpss_srvr_id_t        *  CoreServerID,
    ns_FilesetAttrBits_t           FilesetAttrBits,
    ns_FilesetAttrs_t           *  FilesetAttrs)
{
    int return_value;
    return_value = hpss_FilesetGetAttributes(Name,
                                             FilesetId,
                                             FilesetHandle,
                                             CoreServerID,
                                             FilesetAttrBits,
                                             FilesetAttrs);
    return return_value;
}

#if (HPSS_MAJOR_VERSION == 7 && HPSS_MINOR_VERSION < 4)
int
Hpss_GetAsynchStatus(
    signed32                       CallBackId,
    hpssoid_t                   *  BitfileID,
    signed32                    *  Status)
{
    int return_value;
    return_value = hpss_GetAsynchStatus(CallBackId, BitfileID, Status);
    return return_value;
}
#else
int
Hpss_GetAsyncStatus(
    hpss_reqid_t                   CallBackId,
    bfs_bitfile_obj_handle_t    *  BitfileObj,
    int32_t                     *  Status)
{
    int return_value;
    return_value = hpss_GetAsyncStatus(CallBackId, BitfileObj, Status);
    return return_value;
}
#endif

char *
Hpss_Getenv(const char *Env)
{
    char * return_value;
    return_value = hpss_Getenv(Env);
    return return_value;
}

int
Hpss_GetConfiguration(
    api_config_t                *  ConfigOut)
{
    int return_value;
    return_value = hpss_GetConfiguration(ConfigOut);
    return return_value;
}

int
Hpss_GetThreadUcred(
    sec_cred_t                  *  RetUcred)
{
    int return_value;
    return_value = hpss_GetThreadUcred(RetUcred);
    return return_value;
}

int
Hpss_LoadDefaultThreadState(
    uid_t                     UserID,
    mode_t                    Umask,
    char                   *  ClientFullName)
{
    int return_value;
    return_value = hpss_LoadDefaultThreadState(UserID, Umask, ClientFullName);
    return return_value;
}

int
Hpss_Lstat(
    const char                  *  Path,
    hpss_stat_t                 *  Buf)
{
    int return_value;
    return_value = hpss_Lstat(Path, Buf);
    return return_value;
}

int
Hpss_Mkdir(
    const char                  *  Path,
    mode_t                         Mode)
{
    int return_value;
    return_value = hpss_Mkdir(Path, Mode);
    return return_value;
}

int
Hpss_net_getaddrinfo(
    const char                  *  hostname,
    const char                  *  service,
    int                            flags,
    hpss_ipproto_t                 protocol,
    hpss_sockaddr_t             *  addr,
    char                        *  errbuf,
    size_t                         errbuflen)
{
    int return_value;
    return_value = hpss_net_getaddrinfo(hostname,
                                        service,
                                        flags,
                                        protocol,
                                        addr,
                                        errbuf,
                                        errbuflen);
    return return_value;
}

int
Hpss_Open(
    const char                  * Path,
    int                           Oflag,
    mode_t                        Mode,
    const hpss_cos_hints_t      * HintsIn,
    const hpss_cos_priorities_t * HintsPri,
    hpss_cos_hints_t            * HintsOut)
{
    int return_value;
    return_value = hpss_Open(Path,
                             Oflag,
                             Mode,
                             HintsIn,
                             HintsPri,
                             HintsOut);
    return return_value;
}

#if HPSS_MAJOR_VERSION >= 8
int
Hpss_OpendirHandle(
    const ns_ObjHandle_t        *  DirHandle,
    const sec_cred_t            *  Ucred)
{
    int return_value;
    return_value = hpss_OpendirHandle(DirHandle, Ucred);
    return return_value;
}
#endif

signed32
Hpss_ParseAuthString(
    char                   *  AuthenticatorString,
    hpss_authn_mech_t      *  AuthnMechanism,
    hpss_rpc_auth_type_t   *  AuthenticatorType,
    void                   ** Authenticator)
{
    signed32 return_value;
    return_value = hpss_ParseAuthString(AuthenticatorString,
                                        AuthnMechanism,
                                        AuthenticatorType,
                                        Authenticator);
    return return_value;
}

int
Hpss_PIOEnd(
    hpss_pio_grp_t                 StripeGroup)
{
    int return_value;
    return_value = hpss_PIOEnd(StripeGroup);
    return return_value;
}

int
Hpss_PIOExecute(
    int                            Fd,
    uint64_t                       FileOffset,
    uint64_t                       Size,
    const hpss_pio_grp_t           StripeGroup,
    hpss_pio_gapinfo_t          *  GapInfo,
    uint64_t                    *  BytesMoved)
{
    int return_value;
    return_value = hpss_PIOExecute(Fd,
                                   FileOffset,
                                   Size,
                                   StripeGroup,
                                   GapInfo,
                                   BytesMoved);
    return return_value;
}

int
Hpss_PIOExportGrp(
    const hpss_pio_grp_t           StripeGroup,
    void                        ** Buffer,
    unsigned int                *  BufLength)
{
    int return_value;
    return_value = hpss_PIOExportGrp(StripeGroup, Buffer, BufLength);
    return return_value;
}

int
Hpss_PIOImportGrp(
    const void                  *  Buffer,
    unsigned int                   BufLength,
    hpss_pio_grp_t              *  StripeGroup)
{
    int return_value;
    return_value = hpss_PIOImportGrp(Buffer, BufLength, StripeGroup);
    return return_value;
}

int
Hpss_PIORegister(
    uint32_t                       StripeElement,
    const hpss_sockaddr_t       *  DataNetSockAddr,
    void                        *  DataBuffer,
    uint32_t                       DataBufLen,
    hpss_pio_grp_t                 StripeGroup,
    const hpss_pio_cb_t            IOCallback,
    const void                  *  IOCallbackArg)
{
    int return_value;
    return_value = hpss_PIORegister(StripeElement,
                                    DataNetSockAddr,
                                    DataBuffer,
                                    DataBufLen,
                                    StripeGroup,
                                    IOCallback,
                                    IOCallbackArg);
    return return_value;
}

int
Hpss_PIOStart(
    hpss_pio_params_t           *  InputParams,
    hpss_pio_grp_t              *  StripeGroup)
{
    int return_value;
    return_value = hpss_PIOStart(InputParams, StripeGroup);
    return return_value;
}

#if HPSS_MAJOR_VERSION >= 8
int
Hpss_ReadAttrsPlus(
    int                            Dirdes,
    uint64_t                       OffsetIn,
    uint32_t                       BufferSize,
    hpss_readdir_flags_t           Flags,
    uint32_t                    *  End,
    uint64_t                    *  OffsetOut,
    ns_DirEntry_t               *  DirentPtr)
{
    int return_value;
    return_value = hpss_ReadAttrsPlus(Dirdes,
                                      OffsetIn,
                                      BufferSize,
                                      Flags,
                                      End,
                                      OffsetOut,
                                      DirentPtr);
    return return_value;
}
#else
int
Hpss_ReadAttrsHandle(
    const ns_ObjHandle_t        *  ObjHandle,
    uint64_t                       OffsetIn,
    const sec_cred_t            *  Ucred,
    uint32_t                       BufferSize,
    uint32_t                       GetAttributes,
    uint32_t                    *  End,
    uint64_t                    *  OffsetOut,
    ns_DirEntry_t               *  DirentPtr)
{
    int return_value;
    return_value = hpss_ReadAttrsHandle(ObjHandle,
                                        OffsetIn,
                                        Ucred,
                                        BufferSize,
                                        GetAttributes,
                                        End,
                                        OffsetOut,
                                        DirentPtr);
    return return_value;
}
#endif

int
Hpss_Readlink(
    const char                  *  Path,
    char                        *  Contents,
    size_t                         BufferSize)
{
    int return_value;
    return_value = hpss_Readlink(Path, Contents, BufferSize);
    return return_value;
}

int
Hpss_ReadlinkHandle(
    const ns_ObjHandle_t        *  ObjHandle,
    const char                  *  Path,
    char                        *  Contents,
    size_t                         BufferSize,
    const sec_cred_t            *  Ucred)
{
    int return_value;
    return_value = hpss_ReadlinkHandle(ObjHandle,
                                       Path,
                                       Contents,
                                       BufferSize,
                                       Ucred);
    return return_value;
}

int
Hpss_Rename(
    const char                  *  Old,
    const char                  *  New)
{
    int return_value;
    return_value = hpss_Rename(Old, New);
    return return_value;
}

int
Hpss_Rmdir(
    const char                  *  Path)
{
    int return_value;
    return_value = hpss_Rmdir(Path);
    return return_value;
}

int
Hpss_SetConfiguration(
    const api_config_t          * ConfigIn)
{
    int return_value;
    return_value = hpss_SetConfiguration(ConfigIn);
    return return_value;
}

int
Hpss_SetCOSByHints(
    int                            Fildes,
    uint32_t                       Flags,
    const hpss_cos_hints_t      *  HintsPtr,
    const hpss_cos_priorities_t *  PrioPtr,
    hpss_cos_md_t               *  COSPtr)
{
    int return_value;
    return_value = hpss_SetCOSByHints(Fildes, Flags, HintsPtr, PrioPtr, COSPtr);
    return return_value;
}

int
Hpss_SetLoginCred(
    char                   *  PrincipalName,
    hpss_authn_mech_t         Mechanism,
    hpss_rpc_cred_type_t      CredType,
    hpss_rpc_auth_type_t      AuthType,
    void                   *  Authenticator)
{
    int return_value;
    return_value = hpss_SetLoginCred(PrincipalName,
                                     Mechanism,
                                     CredType,
                                     AuthType,
                                     Authenticator);
    return return_value;
}

int
Hpss_StageCallBack(
    const char                  *  Path,
    uint64_t                       Offset,
    uint64_t                       Length,
    uint32_t                       StorageLevel,
    bfs_callback_addr_t         *  CallBackPtr,
    uint32_t                       Flags,
    hpss_reqid_t                *  ReqID,
    bfs_bitfile_obj_handle_t    *  BitfileObj)
{
    int return_value;
    return_value = hpss_StageCallBack(Path,
                                      Offset,
                                      Length,
                                      StorageLevel,
                                      CallBackPtr,
                                      Flags,
                                      ReqID,
                                      BitfileObj);
    return return_value;
}

int
Hpss_Stat(
    const char                  * Path,
    hpss_stat_t                 * Buf)
{
    int return_value;
    return_value = hpss_Stat(Path, Buf);
    return return_value;
}

int
Hpss_Symlink(
    const char                  *  Contents,
    const char                  *  Path)
{
    int return_value;
    return_value = hpss_Symlink(Contents, Path);
    return return_value;
}

int
Hpss_Truncate(
    const char                  *  Path,
    uint64_t                       Length)
{
    int return_value;
    return_value = hpss_Truncate(Path, Length);
    return return_value;
}

mode_t
Hpss_Umask(mode_t CMask)
{
    mode_t return_value;
    return_value = hpss_Umask(CMask);
    return return_value;
}

int
Hpss_Unlink(
    const char                  *  Path)
{
    int return_value;
    return_value = hpss_Unlink(Path);
    return return_value;
}

int
Hpss_UnlinkHandle(
    const ns_ObjHandle_t        *  ObjHandle,
    const char                  *  Path,
    const sec_cred_t            *  Ucred)
{
    int return_value;
    return_value = hpss_UnlinkHandle(ObjHandle, Path, Ucred);
    return return_value;
}

#if (HPSS_MAJOR_VERSION == 7 && HPSS_MINOR_VERSION < 4)
int
Hpss_UserAttrGetAttrs(
    char                        * Path,
    hpss_userattr_list_t        * Attr,
    int                           XMLFlag)
{
    int return_value;
    return_value = hpss_UserAttrGetAttrs(Path, Attr, XMLFlag);
    return return_value;
}
#else
int
Hpss_UserAttrGetAttrs(
    const char                  * Path,
    hpss_userattr_list_t        * Attr,
    int                           XMLFlag,
    int                           XMLSize)
{
    int return_value;
    return_value = hpss_UserAttrGetAttrs(Path, Attr, XMLFlag, XMLSize);
    return return_value;
}
#endif

int
Hpss_UserAttrSetAttrs(
    const char                  * Path,
    const hpss_userattr_list_t  * Attr,
    const char                  * Schema)
{
    int return_value;
    return_value = hpss_UserAttrSetAttrs(Path, Attr, Schema);
    return return_value;
}


int
Hpss_Utime(
    const char                  *  Path,
    const struct utimbuf        *  Times)
{
    int return_value;
    return_value = hpss_Utime(Path, Times);
    return return_value;
}
