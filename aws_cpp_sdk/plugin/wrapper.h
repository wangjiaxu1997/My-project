#ifndef __WRAPPER_H_
#define __WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif 

	int init(const char* pszendPoint, const char* pszregion, const char* pszaccessKey, const char* pszsecretKey, int useVirtualAddressing);

	bool handle_sync_upload_file(const char* pszBucketName, const char* pszObjectKey, const char* pszUuid, const char* fileContents,int contentLen);

	bool handle_asyns_upload_file(const char* pszBucketName, const char* pszObjectKey, const char* pszUuid, const char* fileContents, int contentLen);

	bool handle_random_http_put(const char* pszBucketName, const char* pszObjectKey, const char* pszUuid, const char* fileContents, int contentLen);


#ifdef __cplusplus
}
#endif 

#endif // __WRAPPER_H_
