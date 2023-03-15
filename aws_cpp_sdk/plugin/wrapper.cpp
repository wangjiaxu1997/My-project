
#include "wrapper.h"
#include "aws_s3.h"
#include "Options.h"

CAWSS3Tools AWSS3Tools;
int init(const char* pszendPoint, const char* pszregion, const char* pszaccessKey, const char* pszsecretKey, int useVirtualAddressing)
{
	return AWSS3Tools.init(pszendPoint, pszregion, pszaccessKey, pszsecretKey, useVirtualAddressing);
}

bool handle_sync_upload_file(const char* pszBucketName, const char* pszObjectKey, const char* pszUuid, const char* fileContents, int contentLen)
{
	return AWSS3Tools.PutObject(pszBucketName, pszObjectKey, pszUuid, fileContents, contentLen);
}

bool handle_asyns_upload_file(const char* pszBucketName, const char* pszObjectKey, const char* pszUuid, const char* fileContents, int contentLen)
{
	return AWSS3Tools.PutObjectAsync(pszBucketName, pszObjectKey, pszUuid, fileContents, contentLen);
}

bool handle_random_http_put(const char* pszBucketName, const char* pszObjectKey, const char* pszUuid, const char* fileContents, int contentLen)
{
	return AWSS3Tools.RandomHttpPut(pszBucketName, pszObjectKey, pszUuid, fileContents, contentLen);
}